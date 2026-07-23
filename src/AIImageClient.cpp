#include "AIImageClient.h"

#include <QSettings>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QEventLoop>
#include <QTimer>
#include <QRegularExpression>
#include <QDebug>
#include <QHash>

// AIImageClient core + routing (refactor E28).

namespace {
constexpr int kConnectionTestTimeoutMs = 15000;
constexpr qsizetype kConnectionTestMaxBytes = 8 * 1024 * 1024;
constexpr const char *kReplyFailureProperty = "drawingstudio_ai_reply_failure";
}

// --- AIImageClient ---
AIImageClient::AIImageClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}


// --- ~AIImageClient ---
AIImageClient::~AIImageClient()
{
    cancel();
}


// --- cancel ---
void AIImageClient::cancel()
{
    const bool wasBusy = m_busy
        || (m_process && m_process->state() != QProcess::NotRunning);
    if (!wasBusy)
        return;

    // Mark idle first so in-flight reply handlers no-op in finishWithImage/failWith.
    m_busy = false;

    if (m_nam) {
        const auto replies = m_nam->findChildren<QNetworkReply *>();
        for (QNetworkReply *reply : replies) {
            if (reply && reply->isRunning())
                reply->abort();
        }
    }

    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }

    emit failed(QStringLiteral("AI job cancelled."));
}


// --- generate ---
void AIImageClient::generate(const Request &request)
{
    runRequest(request, false);
}


// --- edit ---
void AIImageClient::edit(const Request &request)
{
    if (request.sourceImage.isNull() && request.referenceImages.isEmpty()) {
        // Idle validation — failWith ignores non-busy callers to drop late network aborts.
        emit failed(QStringLiteral("No source/reference image for AI edit."));
        return;
    }
    runRequest(request, true);
}


// --- runRequest ---
void AIImageClient::runRequest(Request request, bool isEdit)
{
    if (m_busy) {
        emit failed(QStringLiteral("An AI job is already running."));
        return;
    }
    if (request.prompt.trimmed().isEmpty()) {
        emit failed(QStringLiteral("Prompt is empty."));
        return;
    }

    m_busy = true;
    m_activePrompt = request.prompt.trimmed();
    request.prompt = m_activePrompt;
    const QString provider = resolveProvider(request);
    const QString model = resolveModel(provider, request);
    request.model = model;

    emit started(QStringLiteral("AI (%1): %2…")
                     .arg(provider, isEdit ? QStringLiteral("editing")
                                           : QStringLiteral("generating")));

    if (provider == QLatin1String("openai")) {
        runOpenAI(request, isEdit);
    } else if (provider == QLatin1String("stability")) {
        runStability(request, isEdit);
    } else if (provider == QLatin1String("nanobanana")) {
        runNanoBananaGoogle(request, isEdit);
    } else if (provider == QLatin1String("higgsfield")) {
        runHiggsfieldCli(request, model);
    } else if (provider == QLatin1String("remotesd")) {
        runRemoteSd(request);
    } else {
        failWith(QStringLiteral("Unknown AI provider: %1").arg(provider));
    }
}


// --- resolveProvider ---
QString AIImageClient::resolveProvider(const Request &req) const
{
    if (!req.provider.isEmpty())
        return req.provider;
    return QSettings()
        .value(QStringLiteral("AI/provider"), QStringLiteral("nanobanana"))
        .toString();
}


// --- resolveModel ---
QString AIImageClient::resolveModel(const QString &provider,
                                    const Request &req) const
{
    QString model = req.model.trimmed();
    QSettings s;
    if (model.isEmpty()) {
        if (provider == QLatin1String("nanobanana"))
            model = s.value(QStringLiteral("AI/nanoBananaModel"),
                            QStringLiteral("gemini-3.1-flash-image"))
                        .toString();
        else if (provider == QLatin1String("higgsfield"))
            model = s.value(QStringLiteral("AI/higgsfieldModel"),
                            QStringLiteral("gpt_image_2"))
                        .toString();
        else if (provider == QLatin1String("openai"))
            model = s.value(QStringLiteral("AI/openaiModel"),
                            s.value(QStringLiteral("AI/model"),
                                    QStringLiteral("dall-e-3"))
                                .toString())
                        .toString();
    }
    if (provider == QLatin1String("nanobanana"))
        model = normalizeNanoBananaModel(model);
    return model;
}


void AIImageClient::applyRequestPolicy(QNetworkRequest &request,
                                       bool allowSafeRedirects)
{
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        allowSafeRedirects ? QNetworkRequest::NoLessSafeRedirectPolicy
                           : QNetworkRequest::ManualRedirectPolicy);
}

QString AIImageClient::networkReplyError(const QNetworkReply *reply)
{
    if (!reply)
        return QStringLiteral("No network response");
    const QString boundedFailure =
        reply->property(kReplyFailureProperty).toString();
    if (!boundedFailure.isEmpty())
        return boundedFailure;
    const QUrl redirect =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isValid())
        return QStringLiteral("Redirect refused");
    if (reply->error() != QNetworkReply::NoError)
        return reply->errorString();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300)
        return QStringLiteral("HTTP %1").arg(status);
    return {};
}

// --- watchReply ---
void AIImageClient::watchReply(QNetworkReply *reply, int timeoutMs,
                               qsizetype maxBytes)
{
    if (!reply)
        return;

    const auto enforceSizeLimit = [reply, maxBytes]() {
        if (!reply->isRunning() || maxBytes <= 0)
            return;
        bool lengthOk = false;
        const qint64 contentLength =
            reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&lengthOk);
        if ((lengthOk && contentLength > maxBytes)
            || reply->bytesAvailable() > maxBytes) {
            reply->setProperty(
                kReplyFailureProperty,
                QStringLiteral("Response exceeds %1 MiB limit")
                    .arg(maxBytes / (1024 * 1024)));
            reply->abort();
        }
    };
    connect(reply, &QNetworkReply::metaDataChanged, reply, enforceSizeLimit);
    connect(reply, &QIODevice::readyRead, reply, enforceSizeLimit);

    QTimer::singleShot(timeoutMs, reply, [reply]() {
        if (reply->isRunning()) {
            reply->setProperty(kReplyFailureProperty,
                               QStringLiteral("Request timed out"));
            reply->abort();
        }
    });
}


// --- sizeSetting ---
QString AIImageClient::sizeSetting(const Request &req) const
{
    if (!req.size.isEmpty())
        return req.size;
    return QSettings()
        .value(QStringLiteral("AI/size"), QStringLiteral("1024x1024"))
        .toString();
}


// --- imageToPngBytes ---
QByteArray AIImageClient::imageToPngBytes(const QImage &image) const
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}


// --- saveTempPng ---
QString AIImageClient::saveTempPng(const QImage &image, QString *errorOut)
{
    if (!m_tempDir)
        m_tempDir = std::make_unique<QTemporaryDir>();
    if (!m_tempDir->isValid()) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not create temp directory.");
        return {};
    }
    const QString path = m_tempDir->filePath(QStringLiteral("ai_source.png"));
    if (!image.save(path, "PNG")) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not write temp PNG.");
        return {};
    }
    return path;
}


// --- finishWithImage ---
void AIImageClient::finishWithImage(const QImage &image, const QString &prompt)
{
    if (!m_busy)
        return; // cancelled or already completed
    if (image.isNull()) {
        failWith(QStringLiteral("Received an empty image."));
        return;
    }
    m_busy = false;
    emit finished(image, prompt);
}


// --- failWith ---
void AIImageClient::failWith(const QString &error)
{
    if (!m_busy)
        return; // cancelled or already completed
    m_busy = false;
    emit failed(error);
}


// --- downloadImageUrl ---
void AIImageClient::downloadImageUrl(const QUrl &url, const QString &prompt)
{
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("http")
            && url.scheme() != QLatin1String("https"))) {
        failWith(QStringLiteral("AI result URL must be a valid HTTP(S) URL."));
        return;
    }
    emit progress(QStringLiteral("Downloading result…"));
    QNetworkRequest req(url);
    applyRequestPolicy(req, true);
    QNetworkReply *reply = m_nam->get(req);
    watchReply(reply, kDownloadTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply, prompt]() {
        reply->deleteLater();
        const QString error = networkReplyError(reply);
        if (!error.isEmpty()) {
            failWith(QStringLiteral("Download failed: %1").arg(error));
            return;
        }
        QImage image;
        if (!image.loadFromData(reply->readAll())) {
            failWith(QStringLiteral("Could not decode downloaded image."));
            return;
        }
        finishWithImage(image, prompt);
    });
}


// --- testConnection ---
void AIImageClient::testConnection(const QString &providerIn)
{
    const QString provider =
        providerIn.isEmpty()
            ? QSettings()
                  .value(QStringLiteral("AI/provider"), QStringLiteral("nanobanana"))
                  .toString()
            : providerIn;
    QSettings s;

    ConnectionTestConfig config;
    if (provider == QLatin1String("nanobanana")) {
        config.apiKey =
            s.value(QStringLiteral("AI/nanoBananaApiKey"),
                    s.value(QStringLiteral("AI/googleApiKey")).toString())
                .toString();
    } else if (provider == QLatin1String("openai")) {
        config.apiKey =
            s.value(QStringLiteral("AI/openaiApiKey"),
                    s.value(QStringLiteral("AI/apiKey")).toString())
                .toString();
    } else if (provider == QLatin1String("stability")) {
        config.apiKey = s.value(QStringLiteral("AI/stabilityApiKey")).toString();
    } else if (provider == QLatin1String("higgsfield")) {
        config.higgsfieldCliPath =
            s.value(QStringLiteral("AI/higgsfieldCliPath"), QStringLiteral("higgsfield"))
                .toString();
    } else if (provider == QLatin1String("remotesd")) {
        config.remoteSdUrl =
            s.value(QStringLiteral("AI/remoteSDUrl"), defaultRemoteSdUrl()).toString();
    }

    testConnection(provider, config);
}

void AIImageClient::testConnection(const QString &provider,
                                   const ConnectionTestConfig &config)
{
    const QString apiKey = config.apiKey.trimmed();

    if (provider == QLatin1String("nanobanana")) {
        if (apiKey.isEmpty()) {
            emit connectionTestFinished(
                false, QStringLiteral("Missing Google API key (AI Settings)"));
            return;
        }
        QNetworkRequest request(QUrl(
            QStringLiteral("https://generativelanguage.googleapis.com/v1beta/models")));
        request.setRawHeader("x-goog-api-key", apiKey.toUtf8());
        applyRequestPolicy(request);
        QNetworkReply *reply = m_nam->get(request);
        watchReply(reply, kConnectionTestTimeoutMs, kConnectionTestMaxBytes);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            const QString error = networkReplyError(reply);
            if (!error.isEmpty()) {
                emit connectionTestFinished(
                    false, QStringLiteral("Nano Banana: %1").arg(error));
                return;
            }
            const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
            const int n = obj.value(QStringLiteral("models")).toArray().size();
            emit connectionTestFinished(
                true, QStringLiteral("Nano Banana OK (%1 models)").arg(n));
        });
        return;
    }

    if (provider == QLatin1String("openai")) {
        if (apiKey.isEmpty()) {
            emit connectionTestFinished(false, QStringLiteral("Missing OpenAI API key"));
            return;
        }
        QNetworkRequest request(QUrl(QStringLiteral("https://api.openai.com/v1/models")));
        request.setRawHeader("Authorization",
                             QStringLiteral("Bearer %1").arg(apiKey).toUtf8());
        applyRequestPolicy(request);
        QNetworkReply *reply = m_nam->get(request);
        watchReply(reply, kConnectionTestTimeoutMs, kConnectionTestMaxBytes);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            const QString error = networkReplyError(reply);
            if (!error.isEmpty()) {
                emit connectionTestFinished(
                    false, QStringLiteral("OpenAI: %1").arg(error));
                return;
            }
            emit connectionTestFinished(true, QStringLiteral("OpenAI connection OK"));
        });
        return;
    }

    if (provider == QLatin1String("stability")) {
        if (apiKey.isEmpty()) {
            emit connectionTestFinished(false,
                                        QStringLiteral("Missing Stability API key"));
            return;
        }
        QNetworkRequest request(
            QUrl(QStringLiteral("https://api.stability.ai/v1/user/account")));
        request.setRawHeader("Authorization",
                             QStringLiteral("Bearer %1").arg(apiKey).toUtf8());
        applyRequestPolicy(request);
        QNetworkReply *reply = m_nam->get(request);
        watchReply(reply, kConnectionTestTimeoutMs, kConnectionTestMaxBytes);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            const QString error = networkReplyError(reply);
            if (!error.isEmpty()) {
                emit connectionTestFinished(
                    false, QStringLiteral("Stability: %1").arg(error));
                return;
            }
            emit connectionTestFinished(true, QStringLiteral("Stability connection OK"));
        });
        return;
    }

    if (provider == QLatin1String("higgsfield")) {
        const QString cli = config.higgsfieldCliPath.trimmed().isEmpty()
            ? QStringLiteral("higgsfield")
            : config.higgsfieldCliPath.trimmed();
        auto *proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, proc](int code, QProcess::ExitStatus) {
                    const QString out = QString::fromUtf8(proc->readAll()).trimmed();
                    proc->deleteLater();
                    if (code == 0) {
                        emit connectionTestFinished(
                            true, QStringLiteral("Higgsfield CLI OK"));
                    } else {
                        emit connectionTestFinished(
                            false,
                            QStringLiteral("Higgsfield CLI failed: %1")
                                .arg(out.left(120)));
                    }
                });
        proc->start(cli, {QStringLiteral("version")});
        if (!proc->waitForStarted(3000)) {
            proc->deleteLater();
            emit connectionTestFinished(
                false, QStringLiteral("Could not start higgsfield CLI"));
        } else {
            QTimer::singleShot(5000, proc, [proc]() {
                if (proc->state() != QProcess::NotRunning)
                    proc->kill();
            });
        }
        return;
    }

    if (provider == QLatin1String("remotesd")) {
        QString base = config.remoteSdUrl.trimmed();
        if (base.isEmpty())
            base = defaultRemoteSdUrl();
        if (base.endsWith(QLatin1Char('/')))
            base.chop(1);
        const QUrl url(base + QStringLiteral("/"));
        if (!url.isValid() || url.host().isEmpty()
            || (url.scheme() != QLatin1String("http")
                && url.scheme() != QLatin1String("https"))) {
            emit connectionTestFinished(
                false, QStringLiteral("Remote SD URL must be a valid HTTP(S) URL"));
            return;
        }
        QNetworkRequest request(url);
        applyRequestPolicy(request);
        QNetworkReply *reply = m_nam->get(request);
        watchReply(reply, kConnectionTestTimeoutMs, kConnectionTestMaxBytes);
        connect(reply, &QNetworkReply::finished, this, [this, reply, base]() {
            reply->deleteLater();
            const QString error = networkReplyError(reply);
            if (!error.isEmpty()) {
                emit connectionTestFinished(
                    false, QStringLiteral("Remote SD unreachable: %1")
                               .arg(error));
                return;
            }
            emit connectionTestFinished(
                true, QStringLiteral("Remote SD reachable (%1)").arg(base));
        });
        return;
    }

    emit connectionTestFinished(false,
                                QStringLiteral("Unknown provider: %1").arg(provider));
}

QString AIImageClient::defaultRemoteSdUrl()
{
    return QStringLiteral("http://127.0.0.1:8000");
}
