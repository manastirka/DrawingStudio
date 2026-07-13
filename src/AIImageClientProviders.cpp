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

// AIImageClient OpenAI/Stability/Higgsfield/RemoteSD (refactor E28).

// --- runOpenAI ---
void AIImageClient::runOpenAI(const Request &req, bool isEdit)
{
    QSettings s;
    const QString apiKey =
        s.value(QStringLiteral("AI/openaiApiKey"),
                s.value(QStringLiteral("AI/apiKey")).toString())
            .toString()
            .trimmed();
    if (apiKey.isEmpty()) {
        failWith(QStringLiteral("OpenAI API key missing. Set it in AI → API Settings."));
        return;
    }

    if (isEdit && !req.sourceImage.isNull()) {
        // OpenAI images/edits (multipart)
        auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        QHttpPart imagePart;
        imagePart.setHeader(QNetworkRequest::ContentTypeHeader,
                            QVariant(QStringLiteral("image/png")));
        imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                            QVariant(QStringLiteral(
                                "form-data; name=\"image\"; filename=\"image.png\"")));
        imagePart.setBody(imageToPngBytes(req.sourceImage));
        multi->append(imagePart);

        QHttpPart promptPart;
        promptPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                             QVariant(QStringLiteral("form-data; name=\"prompt\"")));
        promptPart.setBody(req.prompt.toUtf8());
        multi->append(promptPart);

        QHttpPart nPart;
        nPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant(QStringLiteral("form-data; name=\"n\"")));
        nPart.setBody("1");
        multi->append(nPart);

        QHttpPart sizePart;
        sizePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant(QStringLiteral("form-data; name=\"size\"")));
        sizePart.setBody(sizeSetting(req).toUtf8());
        multi->append(sizePart);

        QNetworkRequest request(
            QUrl(QStringLiteral("https://api.openai.com/v1/images/edits")));
        request.setRawHeader("Authorization",
                             QStringLiteral("Bearer %1").arg(apiKey).toUtf8());

        QNetworkReply *reply = m_nam->post(request, multi);
        multi->setParent(reply);

        connect(reply, &QNetworkReply::finished, this, [this, reply, prompt = req.prompt]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                failWith(QStringLiteral("OpenAI edit failed: %1\n%2")
                             .arg(reply->errorString(),
                                  QString::fromUtf8(reply->readAll())));
                return;
            }
            const QJsonObject obj =
                QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray data = obj.value(QStringLiteral("data")).toArray();
            if (data.isEmpty()) {
                failWith(QStringLiteral("OpenAI edit returned no image data."));
                return;
            }
            const QJsonObject first = data.at(0).toObject();
            const QString b64 = first.value(QStringLiteral("b64_json")).toString();
            if (!b64.isEmpty()) {
                QImage image;
                if (image.loadFromData(QByteArray::fromBase64(b64.toUtf8()))) {
                    finishWithImage(image, prompt);
                    return;
                }
            }
            const QString url = first.value(QStringLiteral("url")).toString();
            if (!url.isEmpty()) {
                downloadImageUrl(QUrl(url), prompt);
                return;
            }
            failWith(QStringLiteral("OpenAI edit response missing image."));
        });
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("prompt"), req.prompt);
    payload.insert(QStringLiteral("n"), 1);
    payload.insert(QStringLiteral("size"), sizeSetting(req));
    payload.insert(QStringLiteral("model"),
                   req.model.isEmpty() ? QStringLiteral("dall-e-3") : req.model);
    if (!req.quality.isEmpty())
        payload.insert(QStringLiteral("quality"), req.quality);

    QNetworkRequest request(
        QUrl(QStringLiteral("https://api.openai.com/v1/images/generations")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(apiKey).toUtf8());

    QNetworkReply *reply =
        m_nam->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, prompt = req.prompt]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            failWith(QStringLiteral("OpenAI generate failed: %1\n%2")
                         .arg(reply->errorString(),
                              QString::fromUtf8(reply->readAll())));
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray data = obj.value(QStringLiteral("data")).toArray();
        if (data.isEmpty()) {
            failWith(QStringLiteral("OpenAI returned no image data."));
            return;
        }
        const QJsonObject first = data.at(0).toObject();
        const QString b64 = first.value(QStringLiteral("b64_json")).toString();
        if (!b64.isEmpty()) {
            QImage image;
            if (image.loadFromData(QByteArray::fromBase64(b64.toUtf8()))) {
                finishWithImage(image, prompt);
                return;
            }
        }
        const QString url = first.value(QStringLiteral("url")).toString();
        if (!url.isEmpty()) {
            downloadImageUrl(QUrl(url), prompt);
            return;
        }
        failWith(QStringLiteral("OpenAI response missing image."));
    });
}


// --- runStability ---
void AIImageClient::runStability(const Request &req, bool /*isEdit*/)
{
    QSettings s;
    const QString apiKey =
        s.value(QStringLiteral("AI/stabilityApiKey")).toString().trimmed();
    if (apiKey.isEmpty()) {
        failWith(QStringLiteral("Stability API key missing. Set it in AI → API Settings."));
        return;
    }

    QJsonArray textPrompts;
    QJsonObject textPrompt;
    textPrompt.insert(QStringLiteral("text"), req.prompt);
    textPrompt.insert(QStringLiteral("weight"), 1);
    textPrompts.append(textPrompt);

    const QString size = sizeSetting(req);
    const QStringList parts = size.split(QLatin1Char('x'));
    int w = 1024;
    int h = 1024;
    if (parts.size() == 2) {
        w = parts[0].toInt();
        h = parts[1].toInt();
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("text_prompts"), textPrompts);
    payload.insert(QStringLiteral("cfg_scale"), 7);
    payload.insert(QStringLiteral("height"), h);
    payload.insert(QStringLiteral("width"), w);
    payload.insert(QStringLiteral("samples"), 1);
    payload.insert(QStringLiteral("steps"), 30);

    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.stability.ai/v1/generation/stable-diffusion-xl-1024-v1-0/text-to-image")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply =
        m_nam->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, prompt = req.prompt]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            failWith(QStringLiteral("Stability failed: %1\n%2")
                         .arg(reply->errorString(),
                              QString::fromUtf8(reply->readAll())));
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray artifacts = obj.value(QStringLiteral("artifacts")).toArray();
        if (artifacts.isEmpty()) {
            failWith(QStringLiteral("Stability returned no artifacts."));
            return;
        }
        const QString b64 =
            artifacts.at(0).toObject().value(QStringLiteral("base64")).toString();
        QImage image;
        if (!image.loadFromData(QByteArray::fromBase64(b64.toUtf8()))) {
            failWith(QStringLiteral("Could not decode Stability image."));
            return;
        }
        finishWithImage(image, prompt);
    });
}


// --- runHiggsfieldCli ---
void AIImageClient::runHiggsfieldCli(const Request &req, const QString &model)
{
    QSettings s;
    const bool useCli = s.value(QStringLiteral("AI/higgsfieldUseCli"), true).toBool();
    if (!useCli) {
        failWith(QStringLiteral(
            "Higgsfield HTTP mode needs the CLI for now. Enable “Use local higgsfield CLI” "
            "in AI → API Settings (run `higgsfield auth login` once)."));
        return;
    }

    const QString cli =
        s.value(QStringLiteral("AI/higgsfieldCliPath"), QStringLiteral("higgsfield"))
            .toString();

    QString err;
    QString imagePath;
    if (!req.sourceImage.isNull()) {
        imagePath = saveTempPng(req.sourceImage, &err);
        if (imagePath.isEmpty()) {
            failWith(err);
            return;
        }
    }

    if (!m_process) {
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
    }
    if (m_process->state() != QProcess::NotRunning) {
        failWith(QStringLiteral("Higgsfield CLI is already running."));
        return;
    }

    QStringList args;
    args << QStringLiteral("generate") << QStringLiteral("create") << model
         << QStringLiteral("--prompt") << req.prompt << QStringLiteral("--wait")
         << QStringLiteral("--json");
    if (!imagePath.isEmpty())
        args << QStringLiteral("--image") << imagePath;
    if (!req.aspectRatio.isEmpty())
        args << QStringLiteral("--aspect_ratio") << req.aspectRatio;

    emit progress(QStringLiteral("Running higgsfield %1…").arg(model));
    qDebug() << "AIImageClient:" << cli << args;

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, prompt = req.prompt](int code, QProcess::ExitStatus status) {
                disconnect(m_process, nullptr, this, nullptr);
                const QString out = QString::fromUtf8(m_process->readAll());
                if (status != QProcess::NormalExit || code != 0) {
                    failWith(QStringLiteral("higgsfield failed (exit %1):\n%2")
                                 .arg(code)
                                 .arg(out.left(800)));
                    return;
                }

                // Prefer JSON URL fields; fall back to first https URL in output
                QString url;
                const QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8());
                if (doc.isObject()) {
                    const QJsonObject obj = doc.object();
                    url = obj.value(QStringLiteral("url")).toString();
                    if (url.isEmpty())
                        url = obj.value(QStringLiteral("result_url")).toString();
                    if (url.isEmpty()) {
                        const QJsonArray results =
                            obj.value(QStringLiteral("results")).toArray();
                        if (!results.isEmpty())
                            url = results.at(0)
                                      .toObject()
                                      .value(QStringLiteral("url"))
                                      .toString();
                    }
                    if (url.isEmpty()) {
                        const QJsonArray outputs =
                            obj.value(QStringLiteral("outputs")).toArray();
                        if (!outputs.isEmpty())
                            url = outputs.at(0)
                                      .toObject()
                                      .value(QStringLiteral("url"))
                                      .toString();
                    }
                } else if (doc.isArray()) {
                    const QJsonArray arr = doc.array();
                    if (!arr.isEmpty())
                        url = arr.at(0).toObject().value(QStringLiteral("url")).toString();
                }
                if (url.isEmpty()) {
                    static const QRegularExpression re(
                        QStringLiteral(R"((https?://[^\s\"']+\.(?:png|jpe?g|webp)[^\s\"']*))"),
                        QRegularExpression::CaseInsensitiveOption);
                    const auto m = re.match(out);
                    if (m.hasMatch())
                        url = m.captured(1);
                }
                if (url.isEmpty()) {
                    // Any https URL as last resort
                    static const QRegularExpression anyUrl(
                        QStringLiteral(R"((https?://[^\s\"']+))"));
                    const auto m = anyUrl.match(out);
                    if (m.hasMatch())
                        url = m.captured(1);
                }

                if (url.isEmpty()) {
                    failWith(QStringLiteral(
                                 "higgsfield finished but no image URL was found.\n%1")
                                 .arg(out.left(600)));
                    return;
                }
                downloadImageUrl(QUrl(url), prompt);
            });

    m_process->start(cli, args);
    if (!m_process->waitForStarted(5000)) {
        disconnect(m_process, nullptr, this, nullptr);
        failWith(QStringLiteral(
                     "Could not start higgsfield CLI (%1). Install it or set the path in AI Settings.")
                     .arg(cli));
    }
}


// --- runRemoteSd ---
void AIImageClient::runRemoteSd(const Request &req)
{
    QSettings s;
    QString base = s.value(QStringLiteral("AI/remoteSDUrl"),
                           QStringLiteral("http://192.168.1.58:8000"))
                       .toString()
                       .trimmed();
    if (base.endsWith(QLatin1Char('/')))
        base.chop(1);

    const QString size = sizeSetting(req);
    const QStringList parts = size.split(QLatin1Char('x'));
    int w = 512;
    int h = 512;
    if (parts.size() == 2) {
        w = parts[0].toInt();
        h = parts[1].toInt();
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("prompt"), req.prompt);
    payload.insert(QStringLiteral("width"), w);
    payload.insert(QStringLiteral("height"), h);
    payload.insert(QStringLiteral("steps"), 20);

    QNetworkRequest request(QUrl(base + QStringLiteral("/generate")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    QNetworkReply *reply =
        m_nam->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, prompt = req.prompt]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            failWith(QStringLiteral("Remote SD failed: %1\n%2")
                         .arg(reply->errorString(),
                              QString::fromUtf8(reply->readAll())));
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString b64 = obj.value(QStringLiteral("image")).toString();
        if (b64.isEmpty())
            b64 = obj.value(QStringLiteral("base64")).toString();
        if (b64.startsWith(QLatin1String("data:image")))
            b64 = b64.section(QLatin1Char(','), 1);
        QImage image;
        if (!image.loadFromData(QByteArray::fromBase64(b64.toUtf8()))) {
            const QString url = obj.value(QStringLiteral("url")).toString();
            if (!url.isEmpty()) {
                downloadImageUrl(QUrl(url), prompt);
                return;
            }
            failWith(QStringLiteral("Remote SD returned no image."));
            return;
        }
        finishWithImage(image, prompt);
    });
}


