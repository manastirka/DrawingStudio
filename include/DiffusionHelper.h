#ifndef DIFFUSIONHELPER_H
#define DIFFUSIONHELPER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <memory>

// Forward declarations
struct sd_ctx_t;

/**
 * @brief Stable Diffusion Helper for offline image generation
 * 
 * Provides text-to-image generation using Stable Diffusion.
 * Works completely offline - no API keys or internet required.
 */
class DiffusionHelper : public QObject
{
    Q_OBJECT

public:
    explicit DiffusionHelper(QObject* parent = nullptr);
    ~DiffusionHelper();

    /**
     * @brief Initialize Stable Diffusion
     * @param modelPath Path to the model file (.gguf or .safetensors)
     * @param vaePath Optional VAE model path
     * @param threads Number of threads to use (0 = auto)
     * @return True if initialization successful
     */
    bool initialize(const QString& modelPath, 
                   const QString& vaePath = "",
                   int threads = 0);

    /**
     * @brief Check if diffusion is initialized and ready
     * @return True if ready to generate images
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief Generate image from text prompt
     * @param prompt Text description of desired image
     * @param negativePrompt What to avoid in the image
     * @param width Image width (default 512)
     * @param height Image height (default 512)
     * @param steps Number of sampling steps (default 20)
     * @param cfgScale Classifier-free guidance scale (default 7.0)
     * @param seed Random seed (-1 for random)
     * @return Generated image
     */
    QImage generateImage(const QString& prompt,
                        const QString& negativePrompt = "",
                        int width = 512,
                        int height = 512,
                        int steps = 20,
                        float cfgScale = 7.0f,
                        int seed = -1);

    /**
     * @brief Generate image asynchronously (non-blocking)
     * @param prompt Text description
     * @param negativePrompt What to avoid
     * @param width Image width
     * @param height Image height
     * @param steps Sampling steps
     * @param cfgScale Guidance scale
     * @param seed Random seed
     */
    void generateImageAsync(const QString& prompt,
                           const QString& negativePrompt = "",
                           int width = 512,
                           int height = 512,
                           int steps = 20,
                           float cfgScale = 7.0f,
                           int seed = -1);

    /**
     * @brief Cancel ongoing generation
     */
    void cancelGeneration();

    /**
     * @brief Get available samplers
     * @return List of sampler names
     */
    QStringList availableSamplers() const;

    /**
     * @brief Set sampler method
     * @param sampler Sampler name (euler, euler_a, heun, dpm2, etc.)
     */
    void setSampler(const QString& sampler);

signals:
    /**
     * @brief Emitted when image generation is complete
     * @param image Generated image
     * @param prompt Original prompt
     */
    void imageGenerated(const QImage& image, const QString& prompt);

    /**
     * @brief Emitted during generation to show progress
     * @param step Current step
     * @param totalSteps Total steps
     * @param percentage Progress percentage (0-100)
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
     * @param error Error message
     */
    void errorOccurred(const QString& error);

private:
    sd_ctx_t* m_sdContext;
    bool m_initialized;
    QString m_currentSampler;
    bool m_cancelRequested;

    /**
     * @brief Convert SD image buffer to QImage
     * @param buffer Raw image data
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return QImage
     */
    QImage bufferToQImage(const uint8_t* buffer, int width, int height, int channels);
};

#endif // DIFFUSIONHELPER_H
