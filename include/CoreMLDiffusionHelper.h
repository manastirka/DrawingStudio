#ifndef COREMLDIFFUSIONHELPER_H
#define COREMLDIFFUSIONHELPER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>

/**
 * @brief Core ML Stable Diffusion Helper using Apple's optimized framework
 * 
 * Provides GPU-accelerated image generation using Core ML (Metal + Neural Engine)
 * Much faster than CPU and more stable than ggml Metal backend
 */
class CoreMLDiffusionHelper : public QObject
{
    Q_OBJECT

public:
    explicit CoreMLDiffusionHelper(QObject* parent = nullptr);
    ~CoreMLDiffusionHelper();

    /**
     * @brief Initialize Core ML service
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Check if Core ML is initialized and ready
     * @return True if ready to generate images
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief Generate image from text prompt (blocking)
     * @param prompt Text description of desired image
     * @param negativePrompt What to avoid in the image
     * @param width Image width (default 512)
     * @param height Image height (default 512)
     * @param steps Number of sampling steps (default 20)
     * @param cfgScale Classifier-free guidance scale (default 7.5)
     * @param seed Random seed (-1 for random)
     * @return Generated image
     */
    QImage generateImage(const QString& prompt,
                        const QString& negativePrompt = "",
                        int width = 512,
                        int height = 512,
                        int steps = 20,
                        float cfgScale = 7.5f,
                        int seed = -1);

    /**
     * @brief Generate image asynchronously (non-blocking)
     */
    void generateImageAsync(const QString& prompt,
                           const QString& negativePrompt = "",
                           int width = 512,
                           int height = 512,
                           int steps = 20,
                           float cfgScale = 7.5f,
                           int seed = -1);

signals:
    /**
     * @brief Emitted when image generation is complete
     */
    void imageGenerated(const QImage& image, const QString& prompt);

    /**
     * @brief Emitted during generation to show progress
     */
    void progressUpdate(int step, int totalSteps, int percentage);

    /**
     * @brief Emitted when generation starts
     */
    void generationStarted();

    /**
     * @brief Emitted when generation completes
     */
    void generationFinished();

    /**
     * @brief Emitted on error
     */
    void errorOccurred(const QString& error);

private slots:
    void onProcessReadyRead();
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess* m_process;
    bool m_initialized;
    QString m_currentPrompt;
    
    /**
     * @brief Send command to Python service
     */
    bool sendCommand(const QJsonObject& command);
    
    /**
     * @brief Parse response from Python service
     */
    QJsonObject readResponse();
};

#endif // COREMLDIFFUSIONHELPER_H
