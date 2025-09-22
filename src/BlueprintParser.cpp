#include "BlueprintParser.h"
#include <QDebug>
#include <QtMath>
#include <QColor>
#include <algorithm>
#include <cmath>

BlueprintParser::BlueprintParser(QObject *parent)
    : QObject(parent)
    , m_edgeThreshold(50)
    , m_minContourLength(50)
    , m_cornerThreshold(0.8f)
    , m_simplificationTolerance(2.0f)
    , m_analyzeComponents(true)
{
}

GuitarOutline BlueprintParser::parseBlueprint(const QImage &blueprintImage)
{
    GuitarOutline outline;
    m_lastError.clear();
    
    if (blueprintImage.isNull()) {
        m_lastError = "Invalid input image";
        emit parseError(m_lastError);
        return outline;
    }
    
    qDebug() << "Starting blueprint parsing...";
    emit parseProgress(10);
    
    // Step 1: Preprocess the image
    m_processedImage = preprocessImage(blueprintImage);
    emit parseProgress(20);
    
    // Step 2: Detect edges
    m_edgeMap = detectEdges(m_processedImage);
    emit parseProgress(40);
    
    // Step 3: Find contours
    QVector<QPolygonF> contours = findContours(m_edgeMap);
    emit parseProgress(60);
    
    if (contours.isEmpty()) {
        m_lastError = "No contours found in image";
        emit parseError(m_lastError);
        return outline;
    }
    
    qDebug() << "Found" << contours.size() << "contours";
    
    // Step 4: Analyze guitar shape
    outline = analyzeGuitarShape(contours);
    emit parseProgress(80);
    
    // Step 5: Detect components if enabled
    if (m_analyzeComponents && outline.hasValidBody()) {
        detectComponents(outline, m_processedImage);
    }
    
    emit parseProgress(100);
    emit parseComplete(outline);
    
    qDebug() << "Blueprint parsing complete";
    qDebug() << "Body outline points:" << outline.bodyOutline.size();
    qDebug() << "Neck outline points:" << outline.neckOutline.size();
    qDebug() << "Headstock outline points:" << outline.headstockOutline.size();
    
    return outline;
}

QImage BlueprintParser::preprocessImage(const QImage &input)
{
    // Convert to grayscale if needed
    QImage gray = input.convertToFormat(QImage::Format_Grayscale8);
    
    // Apply Gaussian blur to reduce noise
    QImage blurred = applyGaussianBlur(gray, 1.0f);
    
    return blurred;
}

QImage BlueprintParser::detectEdges(const QImage &input)
{
    // Use Canny edge detection for better results
    return applyCannyEdgeDetection(input, 50, 150);
}

QImage BlueprintParser::applyCannyEdgeDetection(const QImage &input, int lowThreshold, int highThreshold)
{
    if (input.format() != QImage::Format_Grayscale8) {
        return QImage();
    }
    
    QImage result(input.size(), QImage::Format_Grayscale8);
    
    // Simplified Canny edge detection
    // Step 1: Apply Sobel filter
    QImage sobel = applySobelFilter(input);
    
    // Step 2: Apply thresholding
    for (int y = 0; y < sobel.height(); ++y) {
        const uchar *sobelLine = sobel.constScanLine(y);
        uchar *resultLine = result.scanLine(y);
        
        for (int x = 0; x < sobel.width(); ++x) {
            int intensity = sobelLine[x];
            
            if (intensity > highThreshold) {
                resultLine[x] = 255; // Strong edge
            } else if (intensity > lowThreshold) {
                resultLine[x] = 128; // Weak edge
            } else {
                resultLine[x] = 0;   // No edge
            }
        }
    }
    
    // Step 3: Edge tracking by hysteresis (simplified)
    for (int y = 1; y < result.height() - 1; ++y) {
        uchar *line = result.scanLine(y);
        for (int x = 1; x < result.width() - 1; ++x) {
            if (line[x] == 128) { // Weak edge
                // Check if connected to strong edge
                bool connectedToStrong = false;
                for (int dy = -1; dy <= 1; ++dy) {
                    const uchar *neighborLine = result.constScanLine(y + dy);
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (neighborLine[x + dx] == 255) {
                            connectedToStrong = true;
                            break;
                        }
                    }
                    if (connectedToStrong) break;
                }
                line[x] = connectedToStrong ? 255 : 0;
            }
        }
    }
    
    return result;
}

QImage BlueprintParser::applySobelFilter(const QImage &input)
{
    if (input.format() != QImage::Format_Grayscale8) {
        return QImage();
    }
    
    QImage result(input.size(), QImage::Format_Grayscale8);
    
    // Sobel kernels
    int sobelX[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int sobelY[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
    
    for (int y = 1; y < input.height() - 1; ++y) {
        uchar *resultLine = result.scanLine(y);
        
        for (int x = 1; x < input.width() - 1; ++x) {
            int gx = 0, gy = 0;
            
            // Apply Sobel kernels
            for (int ky = -1; ky <= 1; ++ky) {
                const uchar *inputLine = input.constScanLine(y + ky);
                for (int kx = -1; kx <= 1; ++kx) {
                    int pixel = inputLine[x + kx];
                    gx += pixel * sobelX[ky + 1][kx + 1];
                    gy += pixel * sobelY[ky + 1][kx + 1];
                }
            }
            
            // Calculate magnitude
            int magnitude = static_cast<int>(sqrt(gx * gx + gy * gy));
            resultLine[x] = static_cast<uchar>(qBound(0, magnitude, 255));
        }
    }
    
    return result;
}

QImage BlueprintParser::applyGaussianBlur(const QImage &input, float sigma)
{
    if (input.format() != QImage::Format_Grayscale8) {
        return input;
    }
    
    int kernelSize = static_cast<int>(6 * sigma + 1);
    if (kernelSize % 2 == 0) kernelSize++;
    
    QVector<float> kernel(kernelSize);
    float sum = 0.0f;
    int center = kernelSize / 2;
    
    // Generate Gaussian kernel
    for (int i = 0; i < kernelSize; ++i) {
        float x = i - center;
        kernel[i] = exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    
    // Normalize kernel
    for (int i = 0; i < kernelSize; ++i) {
        kernel[i] /= sum;
    }
    
    QImage temp(input.size(), QImage::Format_Grayscale8);
    QImage result(input.size(), QImage::Format_Grayscale8);
    
    // Horizontal pass
    for (int y = 0; y < input.height(); ++y) {
        const uchar *inputLine = input.constScanLine(y);
        uchar *tempLine = temp.scanLine(y);
        
        for (int x = 0; x < input.width(); ++x) {
            float value = 0.0f;
            for (int k = 0; k < kernelSize; ++k) {
                int px = qBound(0, x + k - center, input.width() - 1);
                value += inputLine[px] * kernel[k];
            }
            tempLine[x] = static_cast<uchar>(qBound(0.0f, value, 255.0f));
        }
    }
    
    // Vertical pass
    for (int y = 0; y < temp.height(); ++y) {
        uchar *resultLine = result.scanLine(y);
        
        for (int x = 0; x < temp.width(); ++x) {
            float value = 0.0f;
            for (int k = 0; k < kernelSize; ++k) {
                int py = qBound(0, y + k - center, temp.height() - 1);
                const uchar *tempLine = temp.constScanLine(py);
                value += tempLine[x] * kernel[k];
            }
            resultLine[x] = static_cast<uchar>(qBound(0.0f, value, 255.0f));
        }
    }
    
    return result;
}

QVector<QPolygonF> BlueprintParser::findContours(const QImage &edgeMap)
{
    QVector<QPolygonF> contours;
    
    if (edgeMap.format() != QImage::Format_Grayscale8) {
        return contours;
    }
    
    QImage processed = edgeMap.copy();
    
    // Simple contour following algorithm
    for (int y = 1; y < processed.height() - 1; ++y) {
        for (int x = 1; x < processed.width() - 1; ++x) {
            const uchar *line = processed.constScanLine(y);
            if (line[x] == 255) { // Found edge pixel
                
                QPolygonF contour;
                QVector<QPoint> stack;
                stack.push_back(QPoint(x, y));
                
                // Follow the contour
                while (!stack.isEmpty()) {
                    QPoint current = stack.takeLast();
                    int cx = current.x();
                    int cy = current.y();
                    
                    if (cx < 0 || cx >= processed.width() || 
                        cy < 0 || cy >= processed.height()) continue;
                    
                    uchar *currentLine = processed.scanLine(cy);
                    if (currentLine[cx] != 255) continue;
                    
                    // Add to contour and mark as visited
                    contour << QPointF(cx, cy);
                    currentLine[cx] = 0; // Mark as visited
                    
                    // Add neighboring edge pixels
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            
                            int nx = cx + dx;
                            int ny = cy + dy;
                            
                            if (nx >= 0 && nx < processed.width() && 
                                ny >= 0 && ny < processed.height()) {
                                const uchar *neighborLine = processed.constScanLine(ny);
                                if (neighborLine[nx] == 255) {
                                    stack.push_back(QPoint(nx, ny));
                                }
                            }
                        }
                    }
                }
                
                // Add contour if it's long enough
                if (contour.size() >= m_minContourLength) {
                    contours.append(contour);
                }
            }
        }
    }
    
    qDebug() << "Found" << contours.size() << "valid contours";
    return contours;
}

GuitarOutline BlueprintParser::analyzeGuitarShape(const QVector<QPolygonF> &contours)
{
    GuitarOutline outline;
    
    if (contours.isEmpty()) {
        return outline;
    }
    
    // Calculate bounding rect
    outline.boundingRect = calculateBoundingRect(contours);
    
    // Find the largest contour (likely the body)
    QPolygonF bodyContour = identifyBodyContour(contours);
    if (!bodyContour.isEmpty()) {
        outline.bodyOutline = processContour(bodyContour);
    }
    
    // Find neck contour
    QPolygonF neckContour = identifyNeckContour(contours, bodyContour);
    if (!neckContour.isEmpty()) {
        outline.neckOutline = processContour(neckContour);
    }
    
    // Find headstock contour
    QPolygonF headstockContour = identifyHeadstockContour(contours, neckContour);
    if (!headstockContour.isEmpty()) {
        outline.headstockOutline = processContour(headstockContour);
    }
    
    return outline;
}

QPolygonF BlueprintParser::identifyBodyContour(const QVector<QPolygonF> &contours)
{
    if (contours.isEmpty()) return QPolygonF();
    
    // Find the largest contour by area
    float maxArea = 0;
    int bodyIndex = -1;
    
    for (int i = 0; i < contours.size(); ++i) {
        float area = calculateContourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            bodyIndex = i;
        }
    }
    
    if (bodyIndex >= 0) {
        qDebug() << "Body contour identified with area:" << maxArea;
        return contours[bodyIndex];
    }
    
    return QPolygonF();
}

QPolygonF BlueprintParser::identifyNeckContour(const QVector<QPolygonF> &contours, const QPolygonF &body)
{
    if (contours.isEmpty() || body.isEmpty()) return QPolygonF();
    
    QRectF bodyRect = body.boundingRect();
    
    // Look for elongated contours near the body
    for (const QPolygonF &contour : contours) {
        QRectF rect = contour.boundingRect();
        
        // Check if it's elongated (neck-like)
        float aspectRatio = rect.width() / rect.height();
        bool isElongated = (aspectRatio > 2.0f || aspectRatio < 0.5f);
        
        // Check if it's connected to or near the body
        bool nearBody = rect.intersects(bodyRect.adjusted(-50, -50, 50, 50));
        
        if (isElongated && nearBody && calculateContourArea(contour) > 1000) {
            qDebug() << "Neck contour identified with aspect ratio:" << aspectRatio;
            return contour;
        }
    }
    
    return QPolygonF();
}

QPolygonF BlueprintParser::identifyHeadstockContour(const QVector<QPolygonF> &contours, const QPolygonF &neck)
{
    if (contours.isEmpty() || neck.isEmpty()) return QPolygonF();
    
    QRectF neckRect = neck.boundingRect();
    
    // Look for smaller contours at the end of the neck
    for (const QPolygonF &contour : contours) {
        QRectF rect = contour.boundingRect();
        
        // Should be smaller than neck but significant size
        float area = calculateContourArea(contour);
        bool rightSize = (area > 500 && area < calculateContourArea(neck) * 0.5f);
        
        // Should be near the end of the neck
        bool nearNeckEnd = rect.intersects(neckRect.adjusted(-30, -30, 30, 30));
        
        if (rightSize && nearNeckEnd) {
            qDebug() << "Headstock contour identified with area:" << area;
            return contour;
        }
    }
    
    return QPolygonF();
}

QVector<ContourPoint> BlueprintParser::processContour(const QPolygonF &contour)
{
    QVector<ContourPoint> points;
    
    if (contour.isEmpty()) return points;
    
    // Simplify and smooth the contour
    QPolygonF simplified = simplifyContour(contour, m_simplificationTolerance);
    QPolygonF smoothed = smoothContour(simplified);
    
    // Process each point
    for (int i = 0; i < smoothed.size(); ++i) {
        ContourPoint point;
        point.position = smoothed[i];
        point.curvature = calculateCurvature(smoothed, i);
        point.isCorner = isCornerPoint(smoothed, i, m_cornerThreshold);
        points.append(point);
    }
    
    return points;
}

QPolygonF BlueprintParser::simplifyContour(const QPolygonF &contour, float tolerance)
{
    // Douglas-Peucker algorithm (simplified implementation)
    if (contour.size() < 3) return contour;
    
    QPolygonF result;
    result.append(contour.first());
    
    // Simple decimation for now
    float toleranceSquared = tolerance * tolerance;
    for (int i = 1; i < contour.size() - 1; ++i) {
        QPointF current = contour[i];
        QPointF last = result.last();
        
        float distSquared = QPointF::dotProduct(current - last, current - last);
        if (distSquared > toleranceSquared) {
            result.append(current);
        }
    }
    
    result.append(contour.last());
    return result;
}

float BlueprintParser::calculateContourArea(const QPolygonF &contour)
{
    if (contour.size() < 3) return 0.0f;
    
    float area = 0.0f;
    for (int i = 0; i < contour.size(); ++i) {
        int j = (i + 1) % contour.size();
        area += contour[i].x() * contour[j].y();
        area -= contour[j].x() * contour[i].y();
    }
    
    return qAbs(area) / 2.0f;
}

QRectF BlueprintParser::calculateBoundingRect(const QVector<QPolygonF> &contours)
{
    if (contours.isEmpty()) return QRectF();
    
    QRectF boundingRect;
    for (const QPolygonF &contour : contours) {
        boundingRect = boundingRect.united(contour.boundingRect());
    }
    
    return boundingRect;
}

float BlueprintParser::calculateCurvature(const QPolygonF &contour, int index, int windowSize)
{
    if (contour.size() < 3) return 0.0f;
    
    int size = contour.size();
    int prev = (index - windowSize + size) % size;
    int next = (index + windowSize) % size;
    
    QPointF p1 = contour[prev];
    QPointF p2 = contour[index];
    QPointF p3 = contour[next];
    
    // Calculate vectors
    QPointF v1 = p2 - p1;
    QPointF v2 = p3 - p2;
    
    // Calculate angle between vectors
    float dot = QPointF::dotProduct(v1, v2);
    float len1 = sqrt(QPointF::dotProduct(v1, v1));
    float len2 = sqrt(QPointF::dotProduct(v2, v2));
    
    if (len1 < 0.001f || len2 < 0.001f) return 0.0f;
    
    float cosAngle = dot / (len1 * len2);
    cosAngle = qBound(-1.0f, cosAngle, 1.0f);
    
    return 1.0f - cosAngle; // Higher values indicate more curvature
}

bool BlueprintParser::isCornerPoint(const QPolygonF &contour, int index, float threshold)
{
    float curvature = calculateCurvature(contour, index, 2);
    return curvature > threshold;
}

QPolygonF BlueprintParser::smoothContour(const QPolygonF &contour, int iterations)
{
    if (contour.size() < 3) return contour;
    
    QPolygonF result = contour;
    
    for (int iter = 0; iter < iterations; ++iter) {
        QPolygonF temp = result;
        
        for (int i = 1; i < temp.size() - 1; ++i) {
            QPointF prev = temp[i - 1];
            QPointF current = temp[i];
            QPointF next = temp[i + 1];
            
            // Simple averaging
            result[i] = QPointF(
                (prev.x() + current.x() + next.x()) / 3.0f,
                (prev.y() + current.y() + next.y()) / 3.0f
            );
        }
    }
    
    return result;
}

void BlueprintParser::detectComponents(GuitarOutline &outline, const QImage &processedImage)
{
    if (!outline.hasValidBody()) return;
    
    // Convert body outline to polygon for area checking
    QPolygonF bodyPoly;
    for (const ContourPoint &point : outline.bodyOutline) {
        bodyPoly << point.position;
    }
    
    // Detect various components
    outline.pickupCavities = detectPickupCavities(processedImage, bodyPoly);
    outline.bridgePosts = detectBridgePosts(processedImage, bodyPoly);
    outline.controlPositions = detectControls(processedImage, bodyPoly);
    
    if (outline.hasValidNeck()) {
        QPolygonF neckPoly;
        for (const ContourPoint &point : outline.neckOutline) {
            neckPoly << point.position;
        }
        outline.nutPosition = detectNutPosition(neckPoly);
    }
    
    outline.bridgePosition = detectBridgePosition(bodyPoly);
    
    qDebug() << "Component detection complete:";
    qDebug() << "Pickup cavities:" << outline.pickupCavities.size();
    qDebug() << "Bridge posts:" << outline.bridgePosts.size();
    qDebug() << "Controls:" << outline.controlPositions.size();
}

QVector<QRectF> BlueprintParser::detectPickupCavities(const QImage &image, const QPolygonF &bodyContour)
{
    QVector<QRectF> cavities;
    
    // Look for rectangular dark areas within the body
    QRectF bodyRect = bodyContour.boundingRect();
    
    // This is a simplified implementation
    // In practice, you'd use more sophisticated template matching
    
    return cavities;
}

QVector<QPointF> BlueprintParser::detectBridgePosts(const QImage &image, const QPolygonF &bodyContour)
{
    QVector<QPointF> posts;
    
    // Look for small circular dark spots in the bridge area
    // This would typically be in the lower part of the body
    
    return posts;
}

QVector<QPointF> BlueprintParser::detectControls(const QImage &image, const QPolygonF &bodyContour)
{
    QVector<QPointF> controls;
    
    // Look for small circular areas (knobs, switches)
    // Usually on the upper bout of the body
    
    return controls;
}

QPointF BlueprintParser::detectNutPosition(const QPolygonF &neckContour)
{
    if (neckContour.isEmpty()) return QPointF();
    
    // Nut is typically at the body end of the neck
    QRectF neckRect = neckContour.boundingRect();
    
    // Return approximate position
    return QPointF(neckRect.center().x(), neckRect.bottom());
}

QPointF BlueprintParser::detectBridgePosition(const QPolygonF &bodyContour)
{
    if (bodyContour.isEmpty()) return QPointF();
    
    // Bridge is typically in the lower center of the body
    QRectF bodyRect = bodyContour.boundingRect();
    
    // Return approximate position
    return QPointF(bodyRect.center().x(), bodyRect.bottom() - bodyRect.height() * 0.3f);
}