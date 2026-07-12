#include "CoreMLDiffusionHelper.h"
#include <QJsonArray>
#include <QBuffer>
#include <QDebug>
#include <QCoreApplication>
#include <QThread>

CoreMLDiffusionHelper::CoreMLDiffusionHelper(QObject* parent)
    : QObject(parent)
    , m_process(nullptr)
    , m_initialized(false)
{
}

CoreMLDiffusionHelper::~CoreMLDiffusionHelper()
{
    if (m_process) {
        // Send quit command
        QJsonObject cmd;
        cmd["action"] = "quit";
        sendCommand(cmd);
        
        m_process->waitForFinished(1000);
        m_process->kill();
        delete m_process;
    }
}

bool CoreMLDiffusionHelper::initialize()
{
    qDebug() << "Initializing Core ML Stable Diffusion...";
    
    // Create process
    m_process = new QProcess(this);
    
    // Connect signals
    connect(m_process, &QProcess::readyReadStandardOutput, this, &CoreMLDiffusionHelper::onProcessReadyRead);
    connect(m_process, &QProcess::errorOccurred, this, &CoreMLDiffusionHelper::onProcessError);
    
    // Get path to MLX Python service (pure Metal GPU, no MPS bugs)
    QString servicePath = QCoreApplication::applicationDirPath() + "/../coreml_service/mlx_diffusion_service.py";
    
    // Use venv Python (has all dependencies)
    QString venvPython = QCoreApplication::applicationDirPath() + "/../coreml_service/venv/bin/python3";
    
    // Start Python service with venv
    m_process->start(venvPython, QStringList() << servicePath);
    
    if (!m_process->waitForStarted(5000)) {
        qDebug() << "Failed to start Core ML service";
        emit errorOccurred("Failed to start Core ML service");
        return false;
    }
    
    // Wait for "ready" message
    if (!m_process->waitForReadyRead(10000)) {
        QString errorOutput = m_process->readAllStandardError();
        qDebug() << "Core ML service did not respond";
        qDebug() << "Error output:" << errorOutput;
        emit errorOccurred("Core ML service timeout: " + errorOutput);
        return false;
    }
    
    // Read ready message
    QJsonObject response = readResponse();
    qDebug() << "Core ML service response:" << response;
    
    if (response["status"].toString() == "ready") {
        m_initialized = true;
        qDebug() << "✓ Core ML Stable Diffusion initialized successfully";
        return true;
    }
    
    if (response.contains("error")) {
        QString error = response["error"].toString();
        qDebug() << "Core ML service error:" << error;
        emit errorOccurred(error);
    }
    
    qDebug() << "Core ML service initialization failed";
    return false;
}

QImage CoreMLDiffusionHelper::generateImage(const QString& prompt,
                                            const QString& negativePrompt,
                                            int width,
                                            int height,
                                            int steps,
                                            float cfgScale,
                                            int seed)
{
    if (!m_initialized) {
        qDebug() << "Core ML not initialized";
        emit errorOccurred("Core ML not initialized");
        return QImage();
    }
    
    emit generationStarted();
    
    qDebug() << "Generating image with Core ML...";
    qDebug() << "Prompt:" << prompt;
    qDebug() << "Size:" << width << "x" << height;
    qDebug() << "Steps:" << steps;
    
    // Create command
    QJsonObject cmd;
    cmd["action"] = "generate";
    cmd["prompt"] = prompt;
    cmd["negative_prompt"] = negativePrompt;
    cmd["width"] = width;
    cmd["height"] = height;
    cmd["steps"] = steps;
    cmd["guidance_scale"] = cfgScale;
    cmd["seed"] = seed;
    
    // Send command
    if (!sendCommand(cmd)) {
        emit errorOccurred("Failed to send command to Core ML service");
        emit generationFinished();
        return QImage();
    }
    
    // Wait for response (first time can take 10+ minutes for model download)
    // Subsequent generations: 2-20 seconds
    if (!m_process->waitForReadyRead(600000)) {  // 10 minute timeout for first run
        emit errorOccurred("Core ML generation timeout (model may be downloading)");
        emit generationFinished();
        return QImage();
    }
    
    // Read response
    QJsonObject response = readResponse();
    qDebug() << "Core ML generation response:" << response;
    
    if (!response["success"].toBool()) {
        QString error = response["error"].toString();
        qDebug() << "Core ML generation failed:" << error;
        
        // Check stderr for more details
        QString stderrOutput = m_process->readAllStandardError();
        if (!stderrOutput.isEmpty()) {
            qDebug() << "Core ML stderr:" << stderrOutput;
        }
        
        emit errorOccurred(error);
        emit generationFinished();
        return QImage();
    }
    
    // Decode base64 image
    QString imageBase64 = response["image"].toString();
    qDebug() << "Received base64 image, length:" << imageBase64.length();
    
    QByteArray imageData = QByteArray::fromBase64(imageBase64.toUtf8());
    qDebug() << "Decoded base64 to" << imageData.size() << "bytes";
    
    QImage image;
    if (!image.loadFromData(imageData)) {
        qDebug() << "Failed to decode image from base64";
        emit errorOccurred("Failed to decode generated image");
        emit generationFinished();
        return QImage();
    }
    
    qDebug() << " Image decoded successfully:" << image.width() << "x" << image.height();
    qDebug() << "Emitting imageGenerated signal...";
    emit generationFinished();
    return image;
}

void CoreMLDiffusionHelper::generateImageAsync(const QString& prompt,
                                               const QString& negativePrompt,
                                               int width,
                                               int height,
                                               int steps,
                                               float cfgScale,
                                               int seed)
{
    m_currentPrompt = prompt;
    
    // Run in separate thread
    QThread* thread = QThread::create([=, this]() {
        QImage image = generateImage(prompt, negativePrompt, width, height, steps, cfgScale, seed);
        if (!image.isNull()) {
            emit imageGenerated(image, prompt);
        }
    });
    
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

bool CoreMLDiffusionHelper::sendCommand(const QJsonObject& command)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        return false;
    }
    
    QJsonDocument doc(command);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    
    qint64 written = m_process->write(data);
    m_process->waitForBytesWritten();
    
    return written == data.size();
}

QJsonObject CoreMLDiffusionHelper::readResponse()
{
    if (!m_process) {
        return QJsonObject();
    }
    
    QByteArray line = m_process->readLine();
    QJsonDocument doc = QJsonDocument::fromJson(line);
    
    return doc.object();
}

void CoreMLDiffusionHelper::onProcessReadyRead()
{
    // Handle any async messages from service
    while (m_process && m_process->canReadLine()) {
        QJsonObject response = readResponse();
        
        // Handle progress updates if service sends them
        if (response.contains("progress")) {
            int step = response["step"].toInt();
            int total = response["total"].toInt();
            int percentage = (step * 100) / total;
            emit progressUpdate(step, total, percentage);
        }
    }
}

void CoreMLDiffusionHelper::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Core ML service failed to start. Is Python 3 installed?";
            break;
        case QProcess::Crashed:
            errorMsg = "Core ML service crashed";
            break;
        default:
            errorMsg = "Core ML service error";
            break;
    }
    
    qDebug() << errorMsg;
    emit errorOccurred(errorMsg);
}
