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

// AIImageClient Google Nano Banana provider (refactor E28).

// --- normalizeNanoBananaModel ---
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


// --- sanitizeNanoBananaImageSize ---
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


// --- nextNanoBananaImageSize ---
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


// --- explainNanoBananaFinishReason ---
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


// --- runNanoBananaGoogle ---
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


// --- postNanoBananaGenerate ---
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
    applyRequestPolicy(request);

    if (attempt == 0)
        emit progress(QStringLiteral("Nano Banana (%1, %2)…").arg(model, imgSize));
    else
        emit progress(QStringLiteral("Retrying Nano Banana at %1…").arg(imgSize));

    QNetworkReply *reply =
        m_nam->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    watchReply(reply);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, req, isEdit, attempt, model, imgSize]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        const QString error = networkReplyError(reply);
        if (!error.isEmpty()) {
            const QString detail = QString::fromUtf8(body.left(800));
            if (error.contains(QStringLiteral("timed out"), Qt::CaseInsensitive)) {
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
                         .arg(model, error, detail));
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

