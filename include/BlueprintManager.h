#pragma once

#include <QObject>
#include <QPixmap>
#include <QImage>
#include <QString>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>
#include <QPointF>
#include <QSizeF>
#include <QRectF>
#include <memory>

class QOpenGLShaderProgram;

class BlueprintManager : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit BlueprintManager(QObject *parent = nullptr);
    ~BlueprintManager();

    // Blueprint loading and management
    bool loadBlueprint(const QString &filePath);
    void clearBlueprint();
    bool hasBlueprint() const { return m_hasBlueprint; }

    // Display properties
    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }
    
    void setOpacity(float opacity);
    float getOpacity() const { return m_opacity; }
    
    void setScale(float scale);
    float getScale() const { return m_scale; }
    
    void setPosition(const QPointF &position) { m_position = position; }
    QPointF getPosition() const { return m_position; }

    // Background processing for white background
    void setWhiteBackground(bool enabled) { m_whiteBackground = enabled; }
    bool hasWhiteBackground() const { return m_whiteBackground; }

    // Rendering
    void initializeGL();
    void render(const QMatrix4x4 &projectionMatrix, const QMatrix4x4 &viewMatrix);
    
    // Blueprint information
    QSizeF getBlueprintSize() const { return m_blueprintSize; }
    QString getCurrentFile() const { return m_currentFile; }

signals:
    void blueprintLoaded(const QString &fileName);
    void blueprintCleared();
    void blueprintError(const QString &error);

private:
    void processImageForWhiteBackground();
    void createTexture();
    
    // Blueprint data
    QImage m_originalImage;
    QImage m_processedImage;
    std::unique_ptr<QOpenGLTexture> m_texture;
    QString m_currentFile;
    QSizeF m_blueprintSize;
    bool m_hasBlueprint;
    
    // Display properties
    bool m_visible;
    float m_opacity;
    float m_scale;
    QPointF m_position;
    bool m_whiteBackground;
    
    // OpenGL resources
    bool m_glInitialized;
};