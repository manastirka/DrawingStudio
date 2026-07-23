#include "RemoteSDHelper.h"

#include <QBuffer>
#include <QEventLoop>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonParseError>
#include <QMetaObject>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <cmath>

namespace {
constexpr const char* kTimedOutProperty = "remote_sd_timed_out";
constexpr const char* kOversizedProperty = "remote_sd_response_oversized";
constexpr const char* kPromptProperty = "remote_sd_prompt";

void attachRequestGuards(QNetworkReply* reply, int timeoutMs,
                         qint64 maxResponseBytes)
{
    if (!reply) {
        return;
    }

    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(timeoutMs);
    QObject::connect(timer, &QTimer::timeout, reply, [reply]() {
        reply->setProperty(kTimedOutProperty, true);
        reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, timer, &QTimer::stop);
    timer->start();

    const auto enforceLimit = [reply, maxResponseBytes](qint64 received) {
        if (received <= maxResponseBytes
            || reply->property(kOversizedProperty).toBool()) {
            return;
        }
        reply->setProperty(kOversizedProperty, true);
        reply->abort();
    };
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply,
                     [enforceLimit](qint64 received, qint64) {
                         enforceLimit(received);
                     });
    QObject::connect(reply, &QIODevice::readyRead, reply,
                     [reply, enforceLimit]() {
                         enforceLimit(reply->bytesAvailable());
                     });
}

QString guardedReplyError(const QNetworkReply* reply)
{
    if (reply->property(kTimedOutProperty).toBool()) {
        return QStringLiteral("Request timed out");
    }
    if (reply->property(kOversizedProperty).toBool()) {
        return QStringLiteral("Response exceeds the size limit");
    }
    if (reply->error() != QNetworkReply::NoError) {
        return reply->errorString();
    }
    return {};
}

bool parseJsonObject(const QByteArray& data, QJsonObject& object,
                     QString& errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMessage = QStringLiteral("Invalid JSON response: %1")
                           .arg(parseError.errorString());
        return false;
    }
    if (!document.isObject()) {
        errorMessage = QStringLiteral("JSON response root is not an object");
        return false;
    }
    object = document.object();
    return true;
}
} // namespace

RemoteSDHelper::RemoteSDHelper(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_serverUrl(QStringLiteral("http://127.0.0.1:8000"))
    , m_currentModel(QStringLiteral("sd3.5_large.safetensors"))
    , m_initialized(false)
    , m_generating(false)
{
}

RemoteSDHelper::~RemoteSDHelper() = default;

bool RemoteSDHelper::initialize(const QString& serverUrl)
{
    if (QThread::currentThread() != thread()) {
        emit errorOccurred(QStringLiteral(
            "Remote Stable Diffusion must be initialized on its owner thread"));
        return false;
    }

    const QUrl url(serverUrl, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != QStringLiteral("http")
                           && scheme != QStringLiteral("https"))
        || url.host().isEmpty() || !url.userInfo().isEmpty()) {
        m_initialized = false;
        emit errorOccurred(QStringLiteral(
            "Invalid server URL: use an HTTP(S) URL without credentials"));
        return false;
    }

    QUrl normalizedUrl = url;
    normalizedUrl.setQuery({});
    normalizedUrl.setFragment({});
    QString normalized = normalizedUrl.toString(QUrl::FullyEncoded);
    while (normalized.endsWith('/')) {
        normalized.chop(1);
    }
    m_serverUrl = normalized;
    m_initialized = false;

    QNetworkReply* reply = m_networkManager->get(
        createRequest(QStringLiteral("/")));
    attachRequestGuards(reply, kConnectionTimeoutMs,
                        kMaxMetadataResponseBytes);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QString errorMessage = guardedReplyError(reply);
    QJsonObject response;
    if (errorMessage.isEmpty()
        && !parseJsonObject(reply->readAll(), response, errorMessage)) {
        // parseJsonObject supplies the diagnostic.
    }

    if (errorMessage.isEmpty()) {
        m_initialized = true;
        const QString model = response.value(QStringLiteral("model")).toString();
        if (!model.isEmpty()) {
            emit modelsListReceived(QStringList{model});
        }
        reply->deleteLater();
        return true;
    }

    emit errorOccurred(QStringLiteral("Connection failed: %1")
                           .arg(errorMessage));
    reply->deleteLater();
    return false;
}

void RemoteSDHelper::setModel(const QString& modelName)
{
    m_currentModel = modelName;
}

void RemoteSDHelper::fetchAvailableModels()
{
    QNetworkReply* reply = m_networkManager->get(
        createRequest(QStringLiteral("/sdapi/v1/sd-models")));
    attachRequestGuards(reply, kConnectionTimeoutMs,
                        kMaxMetadataResponseBytes);
    connect(reply, &QNetworkReply::finished, this,
            &RemoteSDHelper::onModelsReply);
}

void RemoteSDHelper::checkServerStatus()
{
    QNetworkReply* reply = m_networkManager->get(
        createRequest(QStringLiteral("/sdapi/v1/progress")));
    attachRequestGuards(reply, kConnectionTimeoutMs,
                        kMaxMetadataResponseBytes);
    connect(reply, &QNetworkReply::finished, this,
            &RemoteSDHelper::onStatusReply);
}

QNetworkRequest RemoteSDHelper::createRequest(const QString& endpoint) const
{
    QNetworkRequest request(QUrl(m_serverUrl + endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    return request;
}

bool RemoteSDHelper::validateGenerationParameters(
    const QString& prompt, int width, int height, int steps, float cfgScale,
    QString& errorMessage) const
{
    if (prompt.trimmed().isEmpty() || prompt.size() > 32768) {
        errorMessage = QStringLiteral(
            "Prompt must contain between 1 and 32768 characters");
        return false;
    }
    if (width < 1 || height < 1 || width > kMaxImageDimension
        || height > kMaxImageDimension
        || static_cast<qint64>(width) * height > kMaxImagePixels) {
        errorMessage =
            QStringLiteral("Requested image dimensions are out of range");
        return false;
    }
    if (steps < 1 || steps > 1000 || !std::isfinite(cfgScale)
        || cfgScale < 0.0f || cfgScale > 100.0f) {
        errorMessage = QStringLiteral("Generation settings are out of range");
        return false;
    }
    return true;
}

QJsonObject RemoteSDHelper::createGenerationRequest(
    const QString& prompt, const QString& negativePrompt, int width, int height,
    int steps, float cfgScale, int seed)
{
    QJsonObject request;
    request[QStringLiteral("prompt")] = prompt;
    if (!negativePrompt.isEmpty()) {
        request[QStringLiteral("negative_prompt")] = negativePrompt;
    }
    request[QStringLiteral("width")] = width;
    request[QStringLiteral("height")] = height;
    request[QStringLiteral("num_inference_steps")] = steps;
    request[QStringLiteral("guidance_scale")] = cfgScale;
    if (seed >= 0) {
        request[QStringLiteral("seed")] = seed;
    }
    request[QStringLiteral("return_format")] = QStringLiteral("base64");
    return request;
}

QImage RemoteSDHelper::generateImage(const QString& prompt,
                                     const QString& negativePrompt,
                                     int width, int height, int steps,
                                     float cfgScale, int seed)
{
    if (!m_initialized) {
        emit errorOccurred(QStringLiteral(
            "Remote Stable Diffusion not initialized"));
        return {};
    }
    if (QThread::currentThread() != thread()) {
        emit errorOccurred(QStringLiteral(
            "Synchronous generation must run on the helper's owner thread"));
        return {};
    }

    QString parameterError;
    if (!validateGenerationParameters(prompt, width, height, steps, cfgScale,
                                      parameterError)) {
        emit errorOccurred(parameterError);
        return {};
    }

    emit generationStarted(prompt);
    const QJsonDocument document(createGenerationRequest(
        prompt, negativePrompt, width, height, steps, cfgScale, seed));
    QNetworkReply* reply = m_networkManager->post(
        createRequest(QStringLiteral("/generate")),
        document.toJson(QJsonDocument::Compact));
    attachRequestGuards(reply, kGenerationTimeoutMs,
                        kMaxGenerationResponseBytes);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QString errorMessage = guardedReplyError(reply);
    QImage image;
    if (errorMessage.isEmpty()) {
        image = parseGenerationResponse(reply->readAll(), errorMessage);
    }
    if (!errorMessage.isEmpty()) {
        emit errorOccurred(errorMessage);
    }
    reply->deleteLater();
    return image;
}

void RemoteSDHelper::generateImageAsync(
    const QString& prompt, const QString& negativePrompt, int width, int height,
    int steps, float cfgScale, int seed)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, prompt, negativePrompt, width, height, steps, cfgScale,
             seed]() {
                generateImageAsync(prompt, negativePrompt, width, height,
                                   steps, cfgScale, seed);
            },
            Qt::QueuedConnection);
        return;
    }
    if (!m_initialized) {
        emit errorOccurred(QStringLiteral(
            "Remote Stable Diffusion not initialized"));
        return;
    }
    if (m_generating) {
        emit errorOccurred(QStringLiteral(
            "A Remote Stable Diffusion generation is already in progress"));
        return;
    }

    QString parameterError;
    if (!validateGenerationParameters(prompt, width, height, steps, cfgScale,
                                      parameterError)) {
        emit errorOccurred(parameterError);
        return;
    }

    m_generating = true;
    emit generationStarted(prompt);
    const QJsonDocument document(createGenerationRequest(
        prompt, negativePrompt, width, height, steps, cfgScale, seed));
    QNetworkReply* reply = m_networkManager->post(
        createRequest(QStringLiteral("/generate")),
        document.toJson(QJsonDocument::Compact));
    reply->setProperty(kPromptProperty, prompt);
    attachRequestGuards(reply, kGenerationTimeoutMs,
                        kMaxGenerationResponseBytes);
    connect(reply, &QNetworkReply::finished, this,
            &RemoteSDHelper::onGenerationReply);
}

QImage RemoteSDHelper::decodeBase64Image(const QString& base64Data)
{
    constexpr qint64 maxEncodedBytes =
        ((kMaxDecodedImageBytes + 2) / 3) * 4 + 4;
    const QByteArray encoded = base64Data.toLatin1();
    if (encoded.isEmpty() || encoded.size() > maxEncodedBytes) {
        return {};
    }

    const auto decoded = QByteArray::fromBase64Encoding(
        encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() > kMaxDecodedImageBytes) {
        return {};
    }

    QByteArray imageData = decoded.decoded;
    QBuffer buffer(&imageData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }
    QImageReader reader(&buffer);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(false);
    const QSize size = reader.size();
    if (!size.isValid() || size.width() > kMaxImageDimension
        || size.height() > kMaxImageDimension
        || static_cast<qint64>(size.width()) * size.height()
               > kMaxImagePixels) {
        return {};
    }
    const QImage image = reader.read();
    if (image.isNull() || image.size() != size) {
        return {};
    }
    return image;
}

QImage RemoteSDHelper::parseGenerationResponse(const QByteArray& data,
                                               QString& errorMessage)
{
    QJsonObject response;
    if (!parseJsonObject(data, response, errorMessage)) {
        return {};
    }

    QString encodedImage = response.value(QStringLiteral("image")).toString();
    if (encodedImage.isEmpty()) {
        const QJsonArray images =
            response.value(QStringLiteral("images")).toArray();
        if (!images.isEmpty()) {
            encodedImage = images.first().toString();
        }
    }
    if (encodedImage.isEmpty()) {
        errorMessage = QStringLiteral("No image in server response");
        return {};
    }

    QImage image = decodeBase64Image(encodedImage);
    if (image.isNull()) {
        errorMessage = QStringLiteral("Failed to decode generated image");
    }
    return image;
}

void RemoteSDHelper::onModelsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    const QString replyError = guardedReplyError(reply);
    if (replyError.isEmpty()) {
        const QByteArray data = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError
            && document.isArray()) {
            QStringList models;
            for (const QJsonValue& value : document.array()) {
                const QString modelName =
                    value.toObject().value(QStringLiteral("model_name"))
                        .toString();
                if (!modelName.isEmpty()) {
                    models << modelName;
                }
            }
            emit modelsListReceived(models);
        } else {
            emit errorOccurred(QStringLiteral("Invalid models response"));
        }
    } else {
        emit errorOccurred(QStringLiteral("Models request failed: %1")
                               .arg(replyError));
    }
    reply->deleteLater();
}

void RemoteSDHelper::onStatusReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    const QString replyError = guardedReplyError(reply);
    if (!replyError.isEmpty()) {
        emit serverStatusChanged(false);
        reply->deleteLater();
        return;
    }

    QJsonObject response;
    QString parseError;
    if (!parseJsonObject(reply->readAll(), response, parseError)) {
        emit serverStatusChanged(false);
        emit errorOccurred(QStringLiteral("Invalid status response: %1")
                               .arg(parseError));
        reply->deleteLater();
        return;
    }

    emit serverStatusChanged(true);
    const double progress =
        response.value(QStringLiteral("progress")).toDouble();
    if (std::isfinite(progress) && progress > 0.0) {
        emit generationProgress(
            qBound(0, static_cast<int>(progress * 100.0), 100));
    }
    reply->deleteLater();
}

void RemoteSDHelper::onGenerationReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    m_generating = false;
    const QString prompt = reply->property(kPromptProperty).toString();
    QString errorMessage = guardedReplyError(reply);
    QImage image;
    if (errorMessage.isEmpty()) {
        image = parseGenerationResponse(reply->readAll(), errorMessage);
    }

    if (errorMessage.isEmpty()) {
        emit imageGenerated(image, prompt);
    } else {
        emit errorOccurred(QStringLiteral("Generation failed: %1")
                               .arg(errorMessage));
    }
    reply->deleteLater();
}

void RemoteSDHelper::onNetworkReply(QNetworkReply* reply)
{
    if (reply) {
        reply->deleteLater();
    }
}
