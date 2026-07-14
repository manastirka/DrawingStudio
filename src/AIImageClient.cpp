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
        failWith(QStringLiteral("No source/reference image for AI edit."));
        return;
    }
    runRequest(request, true);
}


// --- runRequest ---
void AIImageClient::runRequest(Request request, bool isEdit)
{
    if (m_busy) {
        failWith(QStringLiteral("An AI job is already running."));
        return;
    }
    if (request.prompt.trimmed().isEmpty()) {
        failWith(QStringLiteral("Prompt is empty."));
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


// --- watchReply ---
void AIImageClient::watchReply(QNetworkReply *reply, int timeoutMs)
{
    if (!reply)
        return;
    QTimer::singleShot(timeoutMs, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
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
    if (!url.isValid()) {
        failWith(QStringLiteral("Invalid image URL from AI provider."));
        return;
    }
    emit progress(QStringLiteral("Downloading result…"));
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, prompt]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            failWith(QStringLiteral("Download failed: %1").arg(reply->errorString()));
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

    if (provider == QLatin1String("nanobanana")) {
        const QString apiKey =
            s.value(QStringLiteral("AI/nanoBananaApiKey"),
                    s.value(QStringLiteral("AI/googleApiKey")).toString())
                .toString()
                .trimmed();
        if (apiKey.isEmpty()) {
            emit connectionTestFinished(
                false, QStringLiteral("Missing Google API key (AI Settings)"));
            return;
        }
        QNetworkRequest request(QUrl(
            QStringLiteral("https://generativelanguage.googleapis.com/v1beta/models")));
        request.setRawHeader("x-goog-api-key", apiKey.toUtf8());
        QNetworkReply *reply = m_nam->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit connectionTestFinished(
                    false, QStringLiteral("Nano Banana: %1").arg(reply->errorString()));
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
        const QString apiKey =
            s.value(QStringLiteral("AI/openaiApiKey"),
                    s.value(QStringLiteral("AI/apiKey")).toString())
                .toString()
                .trimmed();
        if (apiKey.isEmpty()) {
            emit connectionTestFinished(false, QStringLiteral("Missing OpenAI API key"));
            return;
        }
        QNetworkRequest request(QUrl(QStringLiteral("https://api.openai.com/v1/models")));
        request.setRawHeader("Authorization",
                             QStringLiteral("Bearer %1").arg(apiKey).toUtf8());
        QNetworkReply *reply = m_nam->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit connectionTestFinished(
                    false, QStringLiteral("OpenAI: %1").arg(reply->errorString()));
                return;
            }
            emit connectionTestFinished(true, QStringLiteral("OpenAI connection OK"));
        });
        return;
    }

    if (provider == QLatin1String("stability")) {
        const QString apiKey =
            s.value(QStringLiteral("AI/stabilityApiKey")).toString().trimmed();
        if (apiKey.isEmpty()) {
            emit connectionTestFinished(false,
                                        QStringLiteral("Missing Stability API key"));
            return;
        }
        QNetworkRequest request(
            QUrl(QStringLiteral("https://api.stability.ai/v1/user/account")));
        request.setRawHeader("Authorization",
                             QStringLiteral("Bearer %1").arg(apiKey).toUtf8());
        QNetworkReply *reply = m_nam->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit connectionTestFinished(
                    false, QStringLiteral("Stability: %1").arg(reply->errorString()));
                return;
            }
            emit connectionTestFinished(true, QStringLiteral("Stability connection OK"));
        });
        return;
    }

    if (provider == QLatin1String("higgsfield")) {
        const QString cli =
            s.value(QStringLiteral("AI/higgsfieldCliPath"), QStringLiteral("higgsfield"))
                .toString();
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
        }
        return;
    }

    if (provider == QLatin1String("remotesd")) {
        QString base = s.value(QStringLiteral("AI/remoteSDUrl"),
                               QStringLiteral("http://127.0.0.1:8000"))
                           .toString()
                           .trimmed();
        if (base.endsWith(QLatin1Char('/')))
            base.chop(1);
        QNetworkRequest request(QUrl(base + QStringLiteral("/")));
        QNetworkReply *reply = m_nam->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, base]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit connectionTestFinished(
                    false, QStringLiteral("Remote SD unreachable: %1")
                               .arg(reply->errorString()));
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

