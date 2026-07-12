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
#include <QTimer>
#include <QHash>

AIImageClient::AIImageClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

AIImageClient::~AIImageClient()
{
    cancel();
}

void AIImageClient::cancel()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    m_busy = false;
}

void AIImageClient::generate(const Request &request)
{
    runRequest(request, false);
}

void AIImageClient::edit(const Request &request)
{
    if (request.sourceImage.isNull() && request.referenceImages.isEmpty()) {
        failWith(QStringLiteral("No source/reference image for AI edit."));
        return;
    }
    runRequest(request, true);
}

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

QString AIImageClient::resolveProvider(const Request &req) const
{
    if (!req.provider.isEmpty())
        return req.provider;
    return QSettings()
        .value(QStringLiteral("AI/provider"), QStringLiteral("nanobanana"))
        .toString();
}

QString AIImageClient::normalizeNanoBananaModel(const QString &modelIn)
{
    QString model = modelIn.trimmed();
    if (model.startsWith(QLatin1String("models/")))
        model = model.mid(7);

    // Only map vague shorthand — keep real Google IDs (incl. nano-banana-pro-preview)
    static const QHash<QString, QString> aliases = {
        {QStringLiteral("nano-banana"), QStringLiteral("gemini-3.1-flash-image")},
        {QStringLiteral("nano_banana"), QStringLiteral("gemini-3.1-flash-image")},
        {QStringLiteral("nanobanana"), QStringLiteral("gemini-3.1-flash-image")},
    };
    const auto it = aliases.constFind(model.toLower());
    if (it != aliases.cend())
        return it.value();
    return model;
}

QString AIImageClient::sanitizeNanoBananaImageSize(const QString &model,
                                                   const QString &sizeIn)
{
    QString size = sizeIn.trimmed();
    if (size.isEmpty())
        size = QStringLiteral("1K");
    const QString m = model.toLower();
    const bool no512 =
        m.contains(QLatin1String("pro")) || m.contains(QLatin1String("nano-banana-pro"));
    if (no512 && size == QLatin1String("512"))
        return QStringLiteral("1K");
    return size;
}

QString AIImageClient::nextNanoBananaImageSize(const QString &size)
{
    if (size == QLatin1String("512"))
        return QStringLiteral("1K");
    if (size == QLatin1String("1K"))
        return QStringLiteral("2K");
    if (size == QLatin1String("2K"))
        return QStringLiteral("4K");
    return {};
}

QString AIImageClient::explainNanoBananaFinishReason(const QString &reason)
{
    if (reason == QLatin1String("IMAGE_RECITATION"))
        return QStringLiteral(
            "Google filtered the image (IMAGE_RECITATION — possible copyright/"
            "recitation block). Try a more original prompt or a different output size.");
    if (reason == QLatin1String("SAFETY") || reason == QLatin1String("IMAGE_SAFETY"))
        return QStringLiteral(
            "Google blocked the image for safety reasons. Change the prompt and retry.");
    if (reason == QLatin1String("NO_IMAGE"))
        return QStringLiteral("Model returned no image. Try another model or size.");
    if (reason.isEmpty())
        return QStringLiteral("No image in the response.");
    return QStringLiteral("No image (finishReason=%1).").arg(reason);
}

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

void AIImageClient::watchReply(QNetworkReply *reply, int timeoutMs)
{
    if (!reply)
        return;
    QTimer::singleShot(timeoutMs, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
}

QString AIImageClient::sizeSetting(const Request &req) const
{
    if (!req.size.isEmpty())
        return req.size;
    return QSettings()
        .value(QStringLiteral("AI/size"), QStringLiteral("1024x1024"))
        .toString();
}

QByteArray AIImageClient::imageToPngBytes(const QImage &image) const
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

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

void AIImageClient::finishWithImage(const QImage &image, const QString &prompt)
{
    m_busy = false;
    if (image.isNull()) {
        failWith(QStringLiteral("Received an empty image."));
        return;
    }
    emit finished(image, prompt);
}

void AIImageClient::failWith(const QString &error)
{
    m_busy = false;
    emit failed(error);
}

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

void AIImageClient::runNanoBananaGoogle(const Request &req, bool isEdit)
{
    QSettings s;
    const QString apiKey =
        s.value(QStringLiteral("AI/nanoBananaApiKey"),
                s.value(QStringLiteral("AI/googleApiKey")).toString())
            .toString()
            .trimmed();
    if (apiKey.isEmpty()) {
        failWith(QStringLiteral(
            "Nano Banana (Google) API key missing. Set it in AI → API Settings."));
        return;
    }
    Q_UNUSED(apiKey);

    Request normalized = req;
    normalized.model = normalizeNanoBananaModel(
        req.model.isEmpty() ? QStringLiteral("gemini-3.1-flash-image") : req.model);
    const QString aspect =
        !req.aspectRatio.isEmpty()
            ? req.aspectRatio
            : s.value(QStringLiteral("AI/aspectRatio"), QStringLiteral("1:1")).toString();
    const QString rawSize =
        !req.imageSize.isEmpty()
            ? req.imageSize
            : s.value(QStringLiteral("AI/imageSize"), QStringLiteral("1K")).toString();
    normalized.aspectRatio = aspect;
    normalized.imageSize =
        sanitizeNanoBananaImageSize(normalized.model, rawSize);

    postNanoBananaGenerate(normalized, isEdit, 0);
}

void AIImageClient::postNanoBananaGenerate(Request req, bool isEdit, int attempt)
{
    QSettings s;
    const QString apiKey =
        s.value(QStringLiteral("AI/nanoBananaApiKey"),
                s.value(QStringLiteral("AI/googleApiKey")).toString())
            .toString()
            .trimmed();
    const QString model = req.model;
    const QString imgSize =
        sanitizeNanoBananaImageSize(model, req.imageSize);

    QJsonArray parts;
    QJsonObject textPart;
    textPart.insert(QStringLiteral("text"), req.prompt);
    parts.append(textPart);

    auto appendPng = [&](const QImage &img) {
        if (img.isNull())
            return;
        QJsonObject inlineData;
        inlineData.insert(QStringLiteral("mimeType"), QStringLiteral("image/png"));
        inlineData.insert(QStringLiteral("data"),
                          QString::fromLatin1(imageToPngBytes(img).toBase64()));
        QJsonObject imagePart;
        imagePart.insert(QStringLiteral("inlineData"), inlineData);
        parts.append(imagePart);
    };

    // Reference cutouts first (subjects to implement), then optional plate/source
    if (isEdit) {
        for (const QImage &ref : req.referenceImages)
            appendPng(ref);
        if (!req.sourceImage.isNull())
            appendPng(req.sourceImage);
    }

    QJsonObject content;
    content.insert(QStringLiteral("role"), QStringLiteral("user"));
    content.insert(QStringLiteral("parts"), parts);
    QJsonArray contents;
    contents.append(content);

    QJsonObject generationConfig;
    QJsonArray modalities;
    modalities.append(QStringLiteral("TEXT"));
    modalities.append(QStringLiteral("IMAGE"));
    generationConfig.insert(QStringLiteral("responseModalities"), modalities);

    QJsonObject imageConfig;
    if (!req.aspectRatio.isEmpty())
        imageConfig.insert(QStringLiteral("aspectRatio"), req.aspectRatio);
    if (!imgSize.isEmpty())
        imageConfig.insert(QStringLiteral("imageSize"), imgSize);
    if (!imageConfig.isEmpty())
        generationConfig.insert(QStringLiteral("imageConfig"), imageConfig);

    QJsonObject payload;
    payload.insert(QStringLiteral("contents"), contents);
    payload.insert(QStringLiteral("generationConfig"), generationConfig);

    const QUrl url(QStringLiteral(
                       "https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent")
                       .arg(model));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("x-goog-api-key", apiKey.toUtf8());
    request.setTransferTimeout(180000);

    if (attempt == 0)
        emit progress(QStringLiteral("Nano Banana (%1, %2)…").arg(model, imgSize));
    else
        emit progress(QStringLiteral("Retrying Nano Banana at %1…").arg(imgSize));

    QNetworkReply *reply =
        m_nam->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    watchReply(reply, 180000);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, req, isEdit, attempt, model, imgSize]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString detail = QString::fromUtf8(body.left(800));
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                failWith(QStringLiteral(
                    "Nano Banana timed out after 3 minutes (model %1). Try 1K/2K or a shorter prompt.")
                             .arg(model));
                return;
            }
            // Auto-bump size on "512 not supported"
            if (detail.contains(QStringLiteral("Image size")) &&
                detail.contains(QStringLiteral("not supported")) && attempt == 0) {
                Request retry = req;
                retry.imageSize = nextNanoBananaImageSize(imgSize);
                if (retry.imageSize.isEmpty())
                    retry.imageSize = QStringLiteral("1K");
                postNanoBananaGenerate(retry, isEdit, attempt + 1);
                return;
            }
            failWith(QStringLiteral("Nano Banana failed (%1): %2\n%3")
                         .arg(model, reply->errorString(), detail));
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        if (obj.contains(QStringLiteral("error"))) {
            const QString msg =
                obj.value(QStringLiteral("error")).toObject()
                    .value(QStringLiteral("message")).toString();
            if (msg.contains(QStringLiteral("Image size")) &&
                msg.contains(QStringLiteral("not supported")) && attempt == 0) {
                Request retry = req;
                retry.imageSize = QStringLiteral("1K");
                if (imgSize == QLatin1String("1K"))
                    retry.imageSize = QStringLiteral("2K");
                postNanoBananaGenerate(retry, isEdit, attempt + 1);
                return;
            }
            failWith(QStringLiteral("Nano Banana API error (%1): %2").arg(model, msg));
            return;
        }

        const QJsonObject feedback = obj.value(QStringLiteral("promptFeedback")).toObject();
        const QString blockReason = feedback.value(QStringLiteral("blockReason")).toString();
        if (!blockReason.isEmpty()) {
            failWith(QStringLiteral("Nano Banana blocked the prompt (%1).").arg(blockReason));
            return;
        }

        const QJsonArray candidates = obj.value(QStringLiteral("candidates")).toArray();
        if (candidates.isEmpty()) {
            failWith(QStringLiteral("Nano Banana returned no candidates.\n%1")
                         .arg(QString::fromUtf8(body.left(600))));
            return;
        }

        const QJsonObject cand0 = candidates.at(0).toObject();
        const QString finishReason = cand0.value(QStringLiteral("finishReason")).toString();
        const QJsonArray outParts = cand0.value(QStringLiteral("content"))
                                        .toObject()
                                        .value(QStringLiteral("parts"))
                                        .toArray();

        QImage best;
        for (const QJsonValue &v : outParts) {
            const QJsonObject part = v.toObject();
            QJsonObject inlineData = part.value(QStringLiteral("inlineData")).toObject();
            if (inlineData.isEmpty())
                inlineData = part.value(QStringLiteral("inline_data")).toObject();
            const QString b64 = inlineData.value(QStringLiteral("data")).toString();
            if (b64.isEmpty())
                continue;
            QImage image;
            if (image.loadFromData(QByteArray::fromBase64(b64.toUtf8())))
                best = image;
        }
        if (!best.isNull()) {
            finishWithImage(best, req.prompt);
            return;
        }

        // HTTP 200 but empty image — common with IMAGE_RECITATION. Retry once at next size.
        const QString nextSize = nextNanoBananaImageSize(imgSize);
        if (attempt == 0 && !nextSize.isEmpty()) {
            emit progress(QStringLiteral("No image (%1) — retrying at %2…")
                              .arg(finishReason.isEmpty() ? QStringLiteral("empty")
                                                         : finishReason,
                                   nextSize));
            Request retry = req;
            retry.imageSize = nextSize;
            postNanoBananaGenerate(retry, isEdit, attempt + 1);
            return;
        }

        failWith(QStringLiteral("%1\nModel: %2  Size: %3")
                     .arg(explainNanoBananaFinishReason(finishReason), model, imgSize));
    });
}

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
