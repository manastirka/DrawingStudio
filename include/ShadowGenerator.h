#ifndef SHADOWGENERATOR_H
#define SHADOWGENERATOR_H

#include <QImage>
#include <QVector2D>
#include <QColor>

struct LightingAnalysis {
    QVector2D lightDirection;  // Normalized direction vector
    float lightIntensity;      // 0.0 to 1.0
    QColor ambientColor;       // Average ambient color
    float contrast;            // Image contrast level
    bool isTopLit;            // True if light comes from above
};

struct ShadowParameters {
    QVector2D offset;         // Shadow offset in pixels
    float blur;               // Blur radius
    float opacity;            // Shadow opacity (0.0 to 1.0)
    QColor color;             // Shadow color
    float angle;              // Shadow angle in degrees
    float distance;           // Shadow distance
};

class ShadowGenerator {
public:
    // Analyze lighting in an image
    static LightingAnalysis analyzeLighting(const QImage& image);
    
    // Generate shadow parameters based on lighting analysis
    static ShadowParameters generateShadowParameters(const LightingAnalysis& lighting, 
                                                     const QImage& objectImage);
    
    // Create a shadow image for an object
    static QImage createShadow(const QImage& objectImage, 
                              const QImage& objectMask,
                              const ShadowParameters& params);
    
    // Apply shadow to background image
    static QImage applyShadow(const QImage& background,
                             const QImage& shadowImage,
                             const QPoint& position);
    
    // Create shadow from a drawing primitive (text, shapes, etc.)
    static QImage createShadowFromPrimitive(class DrawingPrimitive* primitive,
                                           const ShadowParameters& params,
                                           const QSize& canvasSize);

private:
    static QVector2D calculateLightDirection(const QImage& image);
    static float calculateAverageIntensity(const QImage& image);
    static QColor calculateAmbientColor(const QImage& image);
    static float calculateContrast(const QImage& image);
};

#endif // SHADOWGENERATOR_H
