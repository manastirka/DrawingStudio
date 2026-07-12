#include "RemoteSDHelper.h"
#include <QJsonArray>
#include <QBuffer>
#include <QDebug>
#include <QThread>
#include <QEventLoop>
#include <QTimer>

RemoteSDHelper::RemoteSDHelper(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_serverUrl("http://192.168.1.58:8000")
    , m_currentModel("sd3.5_large.safetensors")
    , m_initialized(false)
    , m_generating(false)
{
}

RemoteSDHelper::~RemoteSDHelper()
{
}

bool RemoteSDHelper::initialize(const QString& serverUrl)
{
    if (!serverUrl.isEmpty()) {
        m_serverUrl = serverUrl;
    }
    
    qDebug() << "Initializing Remote Stable Diffusion...";
    qDebug() << "Server URL:" << m_serverUrl;
    
    // Check server status using root endpoint
    QNetworkRequest request(QUrl(m_serverUrl + "/"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Wait for response (with timeout)
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeout.start(5000); // 5 second timeout
    loop.exec();
    
    if (timeout.isActive()) {
        timeout.stop();
        if (reply->error() == QNetworkReply::NoError) {
            m_initialized = true;
            qDebug() << "✓ Remote Stable Diffusion server connected";
            
            // Parse server response
            QByteArray data = reply->readAll();
            qDebug() << "Server response:" << data;
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString status = obj["status"].toString();
                QString model = obj["model"].toString();
                QString device = obj["device"].toString();
                
                qDebug() << "Server status:" << status;
                qDebug() << "Model:" << model;
                qDebug() << "Device:" << device;
                
                if (!model.isEmpty()) {
                    QStringList models;
                    models << model;
                    emit modelsListReceived(models);
                }
            }
            
            reply->deleteLater();
            return true;
        } else {
            qDebug() << "⚠ Network error:" << reply->errorString();
            qDebug() << "   Error code:" << reply->error();
            qDebug() << "   URL:" << reply->url().toString();
        }
    } else {
        qDebug() << "⚠ Connection timeout after 5 seconds";
        qDebug() << "   Server URL:" << m_serverUrl;
        qDebug() << "   Check if server is running and accessible";
    }
    
    QString errorMsg = reply->error() == QNetworkReply::NoError ? 
        "Connection timeout" : reply->errorString();
    qDebug() << "⚠ Failed to connect to Remote SD server:" << errorMsg;
    emit errorOccurred("Connection failed: " + errorMsg);
    
    reply->deleteLater();
    return false;
}

void RemoteSDHelper::setModel(const QString& modelName)
{
    m_currentModel = modelName;
    qDebug() << "Remote SD model set to:" << modelName;
}

void RemoteSDHelper::fetchAvailableModels()
{
    QNetworkRequest request(QUrl(m_serverUrl + "/sdapi/v1/sd-models"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &RemoteSDHelper::onModelsReply);
}

void RemoteSDHelper::checkServerStatus()
{
    QNetworkRequest request(QUrl(m_serverUrl + "/sdapi/v1/progress"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &RemoteSDHelper::onStatusReply);
}

QJsonObject RemoteSDHelper::createGenerationRequest(const QString& prompt,
                                                    const QString& negativePrompt,
                                                    int width, int height,
                                                    int steps, float cfgScale, int seed)
{
    QJsonObject request;
    request["prompt"] = prompt;
    
    if (!negativePrompt.isEmpty()) {
        request["negative_prompt"] = negativePrompt;
    }
    
    request["width"] = width;
    request["height"] = height;
    request["num_inference_steps"] = steps;
    request["guidance_scale"] = cfgScale;
    
    if (seed >= 0) {
        request["seed"] = seed;
    }
    
    request["return_format"] = "base64";  // Request base64 format
    
    return request;
}

QImage RemoteSDHelper::generateImage(const QString& prompt,
                                    const QString& negativePrompt,
                                    int width, int height,
                                    int steps, float cfgScale, int seed)
{
    if (!m_initialized) {
        qDebug() << "Remote SD not initialized";
        emit errorOccurred("Remote Stable Diffusion not initialized");
        return QImage();
    }
    
    qDebug() << "Generating image on remote server...";
    qDebug() << "Prompt:" << prompt;
    qDebug() << "Size:" << width << "x" << height;
    qDebug() << "Model:" << m_currentModel;
    
    emit generationStarted(prompt);
    
    // Create request
    QJsonObject requestData = createGenerationRequest(prompt, negativePrompt, width, height, steps, cfgScale, seed);
    QJsonDocument doc(requestData);
    
    QNetworkRequest request(QUrl(m_serverUrl + "/generate"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    
    // Wait for response
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    QImage image;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(data);
        QJsonObject responseObj = responseDoc.object();
        
        // Check for base64 image in response
        QString base64Image = responseObj["image"].toString();
        if (!base64Image.isEmpty()) {
            image = decodeBase64Image(base64Image);
            
            if (!image.isNull()) {
                qDebug() << "✓ Image generated successfully:" << image.width() << "x" << image.height();
            } else {
                qDebug() << "⚠ Failed to decode image";
                emit errorOccurred("Failed to decode generated image");
            }
        } else {
            qDebug() << "⚠ No image in response";
            qDebug() << "   Response:" << data.left(200);
            emit errorOccurred("No image in server response");
        }
    } else {
        qDebug() << "⚠ Network error:" << reply->errorString();
        qDebug() << "   Response:" << reply->readAll().left(200);
        emit errorOccurred("Network error: " + reply->errorString());
    }
    
    reply->deleteLater();
    return image;
}

void RemoteSDHelper::generateImageAsync(const QString& prompt,
                                       const QString& negativePrompt,
                                       int width, int height,
                                       int steps, float cfgScale, int seed)
{
    // Run generation in separate thread
    QThread* thread = QThread::create([=, this]() {
        QImage image = generateImage(prompt, negativePrompt, width, height, steps, cfgScale, seed);
        if (!image.isNull()) {
            emit imageGenerated(image, prompt);
        }
    });
    
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

QImage RemoteSDHelper::decodeBase64Image(const QString& base64Data)
{
    QByteArray imageData = QByteArray::fromBase64(base64Data.toLatin1());
    QImage image;
    image.loadFromData(imageData);
    return image;
}

void RemoteSDHelper::onModelsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        
        if (doc.isArray()) {
            QStringList models;
            for (const QJsonValue& val : doc.array()) {
                QString modelName = val.toObject()["model_name"].toString();
                if (!modelName.isEmpty()) {
                    models << modelName;
                }
            }
            emit modelsListReceived(models);
        }
    }
    
    reply->deleteLater();
}

void RemoteSDHelper::onStatusReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    bool online = (reply->error() == QNetworkReply::NoError);
    emit serverStatusChanged(online);
    
    if (online) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        
        double progress = obj["progress"].toDouble();
        if (progress > 0) {
            emit generationProgress(static_cast<int>(progress * 100));
        }
    }
    
    reply->deleteLater();
}

void RemoteSDHelper::onGenerationReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(data);
        QJsonObject responseObj = responseDoc.object();
        
        // Get first image from response
        QJsonArray images = responseObj["images"].toArray();
        if (!images.isEmpty()) {
            QString base64Image = images[0].toString();
            QImage image = decodeBase64Image(base64Image);
            
            if (!image.isNull()) {
                emit imageGenerated(image, "");
            }
        }
    }
    
    reply->deleteLater();
}

void RemoteSDHelper::onNetworkReply(QNetworkReply* reply)
{
    reply->deleteLater();
}
