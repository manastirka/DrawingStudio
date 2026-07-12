#include "ShadowGenerator.h"
#include "DrawingPrimitive.h"
#include <QPainter>
#include <QtMath>
#include <QDebug>

LightingAnalysis ShadowGenerator::analyzeLighting(const QImage& image) {
    LightingAnalysis result;
    
    result.lightDirection = calculateLightDirection(image);
    result.lightIntensity = calculateAverageIntensity(image);
    result.ambientColor = calculateAmbientColor(image);
    result.contrast = calculateContrast(image);
    result.isTopLit = result.lightDirection.y() < 0;  // Negative Y means light from top
    
    qDebug() << "Lighting Analysis:";
    qDebug() << "  Direction:" << result.lightDirection;
    qDebug() << "  Intensity:" << result.lightIntensity;
    qDebug() << "  Contrast:" << result.contrast;
    qDebug() << "  Top-lit:" << result.isTopLit;
    
    return result;
}

QVector2D ShadowGenerator::calculateLightDirection(const QImage& image) {
    // Analyze brightness gradients to determine light direction
    int width = image.width();
    int height = image.height();
    
    float horizontalGradient = 0.0f;
    float verticalGradient = 0.0f;
    int samples = 0;
    
    // Sample image in a grid pattern
    int step = qMax(10, qMin(width, height) / 20);
    
    for (int y = step; y < height - step; y += step) {
        for (int x = step; x < width - step; x += step) {
            QRgb center = image.pixel(x, y);
            QRgb right = image.pixel(qMin(x + step, width - 1), y);
            QRgb left = image.pixel(qMax(x - step, 0), y);
            QRgb bottom = image.pixel(x, qMin(y + step, height - 1));
            QRgb top = image.pixel(x, qMax(y - step, 0));
            
            // Calculate brightness
            auto brightness = [](QRgb pixel) {
                return (qRed(pixel) + qGreen(pixel) + qBlue(pixel)) / 3.0f;
            };
            
            horizontalGradient += brightness(right) - brightness(left);
            verticalGradient += brightness(bottom) - brightness(top);
            samples++;
        }
    }
    
    if (samples > 0) {
        horizontalGradient /= samples;
        verticalGradient /= samples;
    }
    
    // Normalize direction
    QVector2D direction(horizontalGradient, verticalGradient);
    if (direction.length() > 0.01f) {
        direction.normalize();
    } else {
        // Default: light from top-left
        direction = QVector2D(-0.5f, -0.7f);
        direction.normalize();
    }
    
    return direction;
}

float ShadowGenerator::calculateAverageIntensity(const QImage& image) {
    int width = image.width();
    int height = image.height();
    
    qint64 totalBrightness = 0;
    int samples = 0;
    
    // Sample every 10th pixel for performance
    for (int y = 0; y < height; y += 10) {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.scanLine(y));
        for (int x = 0; x < width; x += 10) {
            totalBrightness += qRed(line[x]) + qGreen(line[x]) + qBlue(line[x]);
            samples++;
        }
    }
    
    float avgBrightness = samples > 0 ? (totalBrightness / (samples * 3.0f * 255.0f)) : 0.5f;
    return qBound(0.0f, avgBrightness, 1.0f);
}

QColor ShadowGenerator::calculateAmbientColor(const QImage& image) {
    int width = image.width();
    int height = image.height();
    
    qint64 totalR = 0, totalG = 0, totalB = 0;
    int samples = 0;
    
    // Sample every 15th pixel
    for (int y = 0; y < height; y += 15) {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.scanLine(y));
        for (int x = 0; x < width; x += 15) {
            totalR += qRed(line[x]);
            totalG += qGreen(line[x]);
            totalB += qBlue(line[x]);
            samples++;
        }
    }
    
    if (samples > 0) {
        return QColor(totalR / samples, totalG / samples, totalB / samples);
    }
    
    return QColor(128, 128, 128);
}

float ShadowGenerator::calculateContrast(const QImage& image) {
    int width = image.width();
    int height = image.height();
    
    QVector<float> brightnesses;
    brightnesses.reserve((width * height) / 100);
    
    // Sample brightness values
    for (int y = 0; y < height; y += 10) {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.scanLine(y));
        for (int x = 0; x < width; x += 10) {
            float b = (qRed(line[x]) + qGreen(line[x]) + qBlue(line[x])) / (3.0f * 255.0f);
            brightnesses.append(b);
        }
    }
    
    if (brightnesses.isEmpty()) return 0.5f;
    
    // Calculate standard deviation
    float mean = 0.0f;
    for (float b : brightnesses) {
        mean += b;
    }
    mean /= brightnesses.size();
    
    float variance = 0.0f;
    for (float b : brightnesses) {
        float diff = b - mean;
        variance += diff * diff;
    }
    variance /= brightnesses.size();
    
    float stdDev = qSqrt(variance);
    return qBound(0.0f, stdDev * 2.0f, 1.0f);  // Scale to 0-1 range
}

ShadowParameters ShadowGenerator::generateShadowParameters(const LightingAnalysis& lighting,
                                                          const QImage& objectImage) {
    ShadowParameters params;
    
    // Calculate shadow angle from light direction (opposite direction)
    float angle = qAtan2(-lighting.lightDirection.y(), -lighting.lightDirection.x());
    params.angle = qRadiansToDegrees(angle);
    
    // Shadow distance based on light intensity (brighter = longer shadow)
    params.distance = 20.0f + lighting.lightIntensity * 30.0f;
    
    // Calculate offset from angle and distance
    params.offset = QVector2D(
        qCos(angle) * params.distance,
        qSin(angle) * params.distance
    );
    
    // Blur based on distance and contrast
    params.blur = 5.0f + params.distance * 0.3f + (1.0f - lighting.contrast) * 10.0f;
    
    // Opacity based on light intensity and contrast
    params.opacity = 0.3f + lighting.lightIntensity * 0.3f + lighting.contrast * 0.2f;
    params.opacity = qBound(0.2f, params.opacity, 0.8f);
    
    // Shadow color - darker version of ambient with slight blue tint
    int r = qMax(0, lighting.ambientColor.red() - 80);
    int g = qMax(0, lighting.ambientColor.green() - 70);
    int b = qMax(0, lighting.ambientColor.blue() - 50);  // Less reduction for blue
    params.color = QColor(r, g, b);
    
    qDebug() << "Shadow Parameters:";
    qDebug() << "  Angle:" << params.angle << "degrees";
    qDebug() << "  Distance:" << params.distance;
    qDebug() << "  Blur:" << params.blur;
    qDebug() << "  Opacity:" << params.opacity;
    
    return params;
}

QImage ShadowGenerator::createShadow(const QImage& objectImage,
                                    const QImage& objectMask,
                                    const ShadowParameters& params) {
    if (objectImage.isNull() || objectMask.isNull()) {
        return QImage();
    }
    
    // Create shadow base from mask
    QImage shadow(objectMask.size(), QImage::Format_ARGB32);
    shadow.fill(Qt::transparent);
    
    QColor shadowColor = params.color;
    shadowColor.setAlphaF(params.opacity);
    QRgb shadowRgba = shadowColor.rgba();
    
    // Fill shadow with shadow color where mask is active (optimized with scanLine)
    for (int y = 0; y < shadow.height(); ++y) {
        QRgb* shadowLine = reinterpret_cast<QRgb*>(shadow.scanLine(y));
        const uchar* maskLine = objectMask.scanLine(y);
        
        for (int x = 0; x < shadow.width(); ++x) {
            if (maskLine[x] > 128) {
                shadowLine[x] = shadowRgba;
            }
        }
    }
    
    // Apply fast separable box blur if needed
    if (params.blur > 0.5f) {
        int radius = qRound(params.blur);
        
        // Horizontal pass
        QImage temp(shadow.size(), QImage::Format_ARGB32);
        temp.fill(Qt::transparent);
        
        for (int y = 0; y < shadow.height(); ++y) {
            const QRgb* srcLine = reinterpret_cast<const QRgb*>(shadow.scanLine(y));
            QRgb* dstLine = reinterpret_cast<QRgb*>(temp.scanLine(y));
            
            for (int x = 0; x < shadow.width(); ++x) {
                int r = 0, g = 0, b = 0, a = 0, count = 0;
                
                int xStart = qMax(0, x - radius);
                int xEnd = qMin(shadow.width() - 1, x + radius);
                
                for (int sx = xStart; sx <= xEnd; ++sx) {
                    QRgb pixel = srcLine[sx];
                    r += qRed(pixel);
                    g += qGreen(pixel);
                    b += qBlue(pixel);
                    a += qAlpha(pixel);
                    count++;
                }
                
                if (count > 0) {
                    dstLine[x] = qRgba(r/count, g/count, b/count, a/count);
                }
            }
        }
        
        // Vertical pass
        shadow.fill(Qt::transparent);
        
        for (int y = 0; y < shadow.height(); ++y) {
            QRgb* dstLine = reinterpret_cast<QRgb*>(shadow.scanLine(y));
            
            for (int x = 0; x < shadow.width(); ++x) {
                int r = 0, g = 0, b = 0, a = 0, count = 0;
                
                int yStart = qMax(0, y - radius);
                int yEnd = qMin(shadow.height() - 1, y + radius);
                
                for (int sy = yStart; sy <= yEnd; ++sy) {
                    const QRgb* srcLine = reinterpret_cast<const QRgb*>(temp.scanLine(sy));
                    QRgb pixel = srcLine[x];
                    r += qRed(pixel);
                    g += qGreen(pixel);
                    b += qBlue(pixel);
                    a += qAlpha(pixel);
                    count++;
                }
                
                if (count > 0) {
                    dstLine[x] = qRgba(r/count, g/count, b/count, a/count);
                }
            }
        }
    }
    
    return shadow;
}

QImage ShadowGenerator::applyShadow(const QImage& background,
                                   const QImage& shadowImage,
                                   const QPoint& position) {
    if (background.isNull() || shadowImage.isNull()) {
        return background;
    }
    
    QImage result = background.copy();
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    
    painter.drawImage(position, shadowImage);
    painter.end();
    
    return result;
}

QImage ShadowGenerator::createShadowFromPrimitive(DrawingPrimitive* primitive,
                                                  const ShadowParameters& params,
                                                  const QSize& canvasSize) {
    if (!primitive) {
        return QImage();
    }
    
    // Get primitive bounding box
    QRectF bounds = primitive->boundingRect();
    
    // Expand bounds to include shadow offset and blur
    int expansion = qRound(params.blur * 2 + qAbs(params.offset.x()) + qAbs(params.offset.y()) + 20);
    QRectF expandedBounds = bounds.adjusted(-expansion, -expansion, expansion, expansion);
    
    // Clamp to canvas size
    expandedBounds = expandedBounds.intersected(QRectF(0, 0, canvasSize.width(), canvasSize.height()));
    
    if (expandedBounds.isEmpty()) {
        return QImage();
    }
    
    // Create image for primitive
    QImage primitiveImage(expandedBounds.size().toSize(), QImage::Format_ARGB32);
    primitiveImage.fill(Qt::transparent);
    
    QPainter primitivePainter(&primitiveImage);
    primitivePainter.setRenderHint(QPainter::Antialiasing);
    primitivePainter.translate(-expandedBounds.topLeft());
    
    // Render primitive (DrawingPrimitive::render() uses OpenGL, so we can't directly render to QImage)
    // For now, return empty - this would need a different approach
    // TODO: Implement proper primitive-to-image rendering
    primitivePainter.end();
    
    qDebug() << "Warning: Shadow from primitive rendering not yet fully implemented";
    return QImage();
    
    // Create mask from alpha channel
    QImage mask(primitiveImage.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < primitiveImage.height(); ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(primitiveImage.scanLine(y));
        uchar* maskLine = mask.scanLine(y);
        
        for (int x = 0; x < primitiveImage.width(); ++x) {
            maskLine[x] = qAlpha(srcLine[x]);
        }
    }
    
    // Create shadow
    QImage shadowImage = createShadow(primitiveImage, mask, params);
    
    // Create final image with shadow offset
    QImage result(expandedBounds.size().toSize(), QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawImage(QPoint(params.offset.x(), params.offset.y()), shadowImage);
    painter.end();
    
    return result;
}
