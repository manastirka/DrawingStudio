// Silence OpenGL deprecation warnings on macOS
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include "BlueprintManager.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QImageReader>
#include <QFileInfo>
#include <QDebug>
#include <QMatrix4x4>
#include <cmath>


BlueprintManager::BlueprintManager(QObject *parent)
    : QObject(parent)
    , m_hasBlueprint(false)
    , m_visible(true)
    , m_opacity(0.5f)
    , m_scale(1.0f)
    , m_position(0.0f, 0.0f)
    , m_whiteBackground(true)
    , m_glInitialized(false)
{
}

BlueprintManager::~BlueprintManager()
{
    // OpenGL texture cleanup is handled automatically by QOpenGLTexture destructor
}

bool BlueprintManager::loadBlueprint(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        emit blueprintError("File does not exist: " + filePath);
        return false;
    }

    // Load the image
    QImageReader reader(filePath);
    if (!reader.canRead()) {
        emit blueprintError("Cannot read image format: " + fileInfo.suffix());
        return false;
    }

    m_originalImage = reader.read();
    if (m_originalImage.isNull()) {
        emit blueprintError("Failed to load image: " + reader.errorString());
        return false;
    }

    m_currentFile = filePath;
    m_blueprintSize = QSizeF(m_originalImage.width(), m_originalImage.height());
    
    // Process image for white background if enabled
    if (m_whiteBackground) {
        processImageForWhiteBackground();
    } else {
        m_processedImage = m_originalImage;
    }

    // Create OpenGL texture if GL context is available
    if (m_glInitialized) {
        createTexture();
    }

    m_hasBlueprint = true;
    emit blueprintLoaded(fileInfo.fileName());
    
    qDebug() << "Blueprint loaded:" << filePath;
    qDebug() << "Original size:" << m_originalImage.size();
    qDebug() << "White background processing:" << (m_whiteBackground ? "enabled" : "disabled");
    
    return true;
}

void BlueprintManager::clearBlueprint()
{
    m_hasBlueprint = false;
    m_originalImage = QImage();
    m_processedImage = QImage();
    m_texture.reset();
    m_currentFile.clear();
    m_blueprintSize = QSizeF();
    
    emit blueprintCleared();
    qDebug() << "Blueprint cleared";
}

void BlueprintManager::setOpacity(float opacity)
{
    m_opacity = qBound(0.0f, opacity, 1.0f);
}

void BlueprintManager::setScale(float scale)
{
    m_scale = qMax(0.1f, scale);
}

void BlueprintManager::processImageForWhiteBackground()
{
    if (m_originalImage.isNull()) return;

    // Convert to ARGB32 format for easier processing
    m_processedImage = m_originalImage.convertToFormat(QImage::Format_ARGB32);
    
    // Process each pixel to create white background
    for (int y = 0; y < m_processedImage.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(m_processedImage.scanLine(y));
        for (int x = 0; x < m_processedImage.width(); ++x) {
            QRgb pixel = line[x];
            
            // Get RGB values
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);
            int a = qAlpha(pixel);
            
            // Convert to grayscale if it's not already
            int gray = qGray(pixel);
            
            // For blueprint effect: darker lines on white background
            // Invert the grayscale value so dark lines stay dark
            int invertedGray = 255 - gray;
            
            // Create white background with dark lines
            // If pixel is light (gray > threshold), make it white/transparent
            // If pixel is dark (gray <= threshold), keep it dark
            if (gray > 200) {
                // Light areas become white/transparent
                line[x] = qRgba(255, 255, 255, 50); // Very light white
            } else {
                // Dark areas stay dark (blueprint lines)
                line[x] = qRgba(0, 0, 0, a); // Keep as dark lines
            }
        }
    }
    
    qDebug() << "Processed image for white background";
}

void BlueprintManager::initializeGL()
{
    initializeOpenGLFunctions();
    
    m_glInitialized = true;
    
    // Create texture if we have an image loaded
    if (m_hasBlueprint) {
        createTexture();
    }
    
    qDebug() << "BlueprintManager OpenGL initialized";
}


void BlueprintManager::createTexture()
{
    if (m_processedImage.isNull()) return;
    
    m_texture = std::make_unique<QOpenGLTexture>(m_processedImage.flipped(Qt::Vertical));
    m_texture->setMinificationFilter(QOpenGLTexture::Linear);
    m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    
    qDebug() << "Blueprint texture created, size:" << m_processedImage.size();
}


void BlueprintManager::render(const QMatrix4x4 &projectionMatrix, const QMatrix4x4 &viewMatrix)
{
    if (!m_hasBlueprint || !m_visible || !m_texture) {
        return;
    }
    
    // Use simple OpenGL rendering without shaders for better compatibility
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    
    // Bind texture
    m_texture->bind();
    
    // Set color with opacity
    glColor4f(1.0f, 1.0f, 1.0f, m_opacity);
    
    // Calculate quad vertices - use a reasonable size for testing
    float width = 400.0f * m_scale; // Fixed size for testing
    float height = 200.0f * m_scale;
    
    // Center the blueprint at the position (0,0 should be center of screen)
    float x1 = m_position.x() - width / 2.0f;
    float y1 = m_position.y() - height / 2.0f;
    float x2 = m_position.x() + width / 2.0f;
    float y2 = m_position.y() + height / 2.0f;
    
    qDebug() << "Rendering blueprint at" << m_position << "size:" << width << "x" << height;
    
    // Render textured quad using immediate mode
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x1, y1); // bottom left
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x2, y1); // bottom right
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x2, y2); // top right
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x1, y2); // top left
    glEnd();
    
    // Restore default color
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}