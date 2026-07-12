#include "DiffusionHelper.h"
#include <stable-diffusion.h>
#include <QDebug>
#include <QThread>
#include <QDir>
#include <QCoreApplication>
#include <ctime>
#include <cstdlib>

DiffusionHelper::DiffusionHelper(QObject* parent)
    : QObject(parent)
    , m_sdContext(nullptr)
    , m_initialized(false)
    , m_currentSampler("euler_a")
    , m_cancelRequested(false)
{
    qDebug() << "DiffusionHelper created";
}

DiffusionHelper::~DiffusionHelper()
{
    if (m_sdContext) {
        free_sd_ctx(m_sdContext);
    }
    qDebug() << "DiffusionHelper destroyed";
}

bool DiffusionHelper::initialize(const QString& modelPath, const QString& vaePath, int threads)
{
    qDebug() << "Initializing Stable Diffusion...";
    qDebug() << "Model path:" << modelPath;
    
    // Check if model file exists
    if (!QFile::exists(modelPath)) {
        qDebug() << "Model file not found:" << modelPath;
        emit errorOccurred("Model file not found: " + modelPath);
        return false;
    }
    
    // Determine number of threads
    if (threads <= 0) {
        threads = QThread::idealThreadCount();
    }
    
    qDebug() << "Using" << threads << "threads";
    
    // Initialize context parameters
    sd_ctx_params_t params;
    sd_ctx_params_init(&params);
    
    QByteArray modelPathBytes = modelPath.toUtf8();
    params.model_path = modelPathBytes.constData();
    
    if (!vaePath.isEmpty()) {
        QByteArray vaePathBytes = vaePath.toUtf8();
        params.vae_path = vaePathBytes.constData();
    }
    
    params.n_threads = threads;
    params.wtype = SD_TYPE_F16;
    params.rng_type = STD_DEFAULT_RNG;
    params.vae_decode_only = false;
    params.free_params_immediately = false;
    
    // CRITICAL: Disable Metal GPU to prevent crashes
    // Metal backend has bugs that cause ggml_abort errors
    // Force CPU-only mode by setting environment variable
    setenv("GGML_METAL_DISABLE", "1", 1);
    
    // Initialize SD context
    m_sdContext = new_sd_ctx(&params);
    
    if (!m_sdContext) {
        qDebug() << "Failed to create SD context";
        emit errorOccurred("Failed to initialize Stable Diffusion");
        return false;
    }
    
    m_initialized = true;
    qDebug() << "Stable Diffusion initialized successfully";
    
    return true;
}

QImage DiffusionHelper::generateImage(const QString& prompt,
                                     const QString& negativePrompt,
                                     int width,
                                     int height,
                                     int steps,
                                     float cfgScale,
                                     int seed)
{
    if (!m_initialized) {
        qDebug() << "Stable Diffusion not initialized";
        emit errorOccurred("Stable Diffusion not initialized");
        return QImage();
    }
    
    if (prompt.isEmpty()) {
        qDebug() << "Empty prompt";
        emit errorOccurred("Prompt cannot be empty");
        return QImage();
    }
    
    emit generationStarted();
    
    qDebug() << "Generating image...";
    qDebug() << "Prompt:" << prompt;
    qDebug() << "Size:" << width << "x" << height;
    qDebug() << "Steps:" << steps;
    qDebug() << "CFG Scale:" << cfgScale;
    
    // Use random seed if not specified
    if (seed < 0) {
        seed = static_cast<int>(time(nullptr));
    }
    
    qDebug() << "Seed:" << seed;
    
    // Convert sampler string to enum
    sample_method_t sampler = EULER_A;
    if (m_currentSampler == "euler") sampler = EULER;
    else if (m_currentSampler == "euler_a") sampler = EULER_A;
    else if (m_currentSampler == "heun") sampler = HEUN;
    else if (m_currentSampler == "dpm2") sampler = DPM2;
    else if (m_currentSampler == "dpmpp2s_a") sampler = DPMPP2S_A;
    else if (m_currentSampler == "dpmpp2m") sampler = DPMPP2M;
    else if (m_currentSampler == "dpmpp2mv2") sampler = DPMPP2Mv2;
    else if (m_currentSampler == "lcm") sampler = LCM;
    
    // Initialize generation parameters
    sd_img_gen_params_t gen_params;
    sd_img_gen_params_init(&gen_params);
    
    QByteArray promptBytes = prompt.toUtf8();
    QByteArray negPromptBytes = negativePrompt.toUtf8();
    
    gen_params.prompt = promptBytes.constData();
    gen_params.negative_prompt = negPromptBytes.constData();
    gen_params.clip_skip = 0;
    gen_params.width = width;
    gen_params.height = height;
    gen_params.strength = 0.75f;
    gen_params.seed = seed;
    gen_params.batch_count = 1;
    
    // Set sample parameters
    gen_params.sample_params.sample_method = sampler;
    gen_params.sample_params.sample_steps = steps;
    gen_params.sample_params.guidance.txt_cfg = cfgScale;
    gen_params.sample_params.scheduler = DEFAULT;
    
    // Generate image
    sd_image_t* result = generate_image(m_sdContext, &gen_params);
    
    if (!result) {
        qDebug() << "Image generation failed";
        emit errorOccurred("Image generation failed");
        emit generationFinished();
        return QImage();
    }
    
    // Convert to QImage
    QImage image = bufferToQImage(result->data, result->width, result->height, result->channel);
    
    // Free SD image
    free(result->data);
    free(result);
    
    qDebug() << "Image generated successfully:" << image.width() << "x" << image.height();
    
    emit generationFinished();
    
    return image;
}

void DiffusionHelper::generateImageAsync(const QString& prompt,
                                        const QString& negativePrompt,
                                        int width,
                                        int height,
                                        int steps,
                                        float cfgScale,
                                        int seed)
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

void DiffusionHelper::cancelGeneration()
{
    m_cancelRequested = true;
    qDebug() << "Generation cancellation requested";
}

QStringList DiffusionHelper::availableSamplers() const
{
    return {
        "euler",
        "euler_a",
        "heun",
        "dpm2",
        "dpmpp2s_a",
        "dpmpp2m",
        "dpmpp2mv2",
        "lcm"
    };
}

void DiffusionHelper::setSampler(const QString& sampler)
{
    if (availableSamplers().contains(sampler)) {
        m_currentSampler = sampler;
        qDebug() << "Sampler set to:" << sampler;
    } else {
        qDebug() << "Unknown sampler:" << sampler;
    }
}

QImage DiffusionHelper::bufferToQImage(const uint8_t* buffer, int width, int height, int channels)
{
    if (!buffer || width <= 0 || height <= 0) {
        return QImage();
    }
    
    QImage image(width, height, QImage::Format_RGB888);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * channels;
            
            uint8_t r = buffer[idx];
            uint8_t g = channels > 1 ? buffer[idx + 1] : r;
            uint8_t b = channels > 2 ? buffer[idx + 2] : r;
            
            image.setPixel(x, y, qRgb(r, g, b));
        }
    }
    
    return image;
}
