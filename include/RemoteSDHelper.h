#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

class RemoteSDHelper : public QObject
{
    Q_OBJECT

public:
    static constexpr int kConnectionTimeoutMs = 5000;
    static constexpr int kGenerationTimeoutMs = 120000;
    static constexpr qint64 kMaxMetadataResponseBytes = 1024 * 1024;
    static constexpr qint64 kMaxGenerationResponseBytes = 96 * 1024 * 1024;
    static constexpr qint64 kMaxDecodedImageBytes = 64 * 1024 * 1024;
    static constexpr qint64 kMaxImagePixels = 16 * 1024 * 1024;
    static constexpr int kMaxImageDimension = 8192;

    explicit RemoteSDHelper(QObject* parent = nullptr);
    ~RemoteSDHelper();

    // Initialize with server URL
    bool initialize(const QString& serverUrl = "http://127.0.0.1:8000");
    bool isInitialized() const { return m_initialized; }
    
    // Get available models from server
    void fetchAvailableModels();
    
    // Set which model to use (e.g., "sd3.5_large.safetensors")
    void setModel(const QString& modelName);
    QString currentModel() const { return m_currentModel; }
    
    // Synchronous generation
    QImage generateImage(const QString& prompt,
                        const QString& negativePrompt = "",
                        int width = 512,
                        int height = 512,
                        int steps = 20,
                        float cfgScale = 7.0f,
                        int seed = -1);
    
    // Asynchronous generation
    void generateImageAsync(const QString& prompt,
                           const QString& negativePrompt = "",
                           int width = 512,
                           int height = 512,
                           int steps = 20,
                           float cfgScale = 7.0f,
                           int seed = -1);
    
    // Check server status
    void checkServerStatus();

signals:
    void imageGenerated(const QImage& image, const QString& prompt);
    void generationStarted(const QString& prompt);
    void generationProgress(int percentage);
    void errorOccurred(const QString& error);
    void serverStatusChanged(bool online);
    void modelsListReceived(const QStringList& models);

private slots:
    void onNetworkReply(QNetworkReply* reply);
    void onGenerationReply();
    void onModelsReply();
    void onStatusReply();

private:
    QNetworkAccessManager* m_networkManager;
    QString m_serverUrl;
    QString m_currentModel;
    bool m_initialized;
    bool m_generating;
    
    // Helper methods
    QNetworkRequest createRequest(const QString& endpoint) const;
    bool validateGenerationParameters(const QString& prompt,
                                      int width, int height, int steps,
                                      float cfgScale,
                                      QString& errorMessage) const;
    QJsonObject createGenerationRequest(const QString& prompt,
                                       const QString& negativePrompt,
                                       int width, int height,
                                       int steps, float cfgScale, int seed);
    QImage parseGenerationResponse(const QByteArray& data,
                                   QString& errorMessage);
    QImage decodeBase64Image(const QString& base64Data);
};
