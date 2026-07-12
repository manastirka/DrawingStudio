#include "EdgeSelectionTool.h"
#include <QDebug>
#include <QColor>
#include <cmath>
#include <queue>
#include <algorithm>

EdgeSelectionTool::EdgeSelectionTool(QObject *parent)
    : QObject(parent)
    , m_edgeThreshold(0.3f)
    , m_growthTolerance(0.15f)
    , m_minContourSize(20)
{
}

EdgeSelectionTool::SelectionResult EdgeSelectionTool::selectByEdges(const QImage& image, const QPointF& seedPoint)
{
    SelectionResult result;
    result.success = false;
    result.confidence = 0.0f;
    
    try {
        qDebug() << "EdgeSelectionTool: Starting intelligent edge-based selection at" << seedPoint;
        
        if (image.isNull() || !isValidSeed(image, seedPoint)) {
            emit selectionFailed("Invalid seed point or image");
            return result;
        }
        
        // Step 1: Enhance image contrast for better edge detection
        QImage enhanced = enhanceContrast(image);
        
        // Step 2: Apply slight blur to reduce noise
        QImage blurred = gaussianBlur(enhanced, 1.0f);
        
        // Step 3: Detect edges using Sobel operator
        QImage edges = sobelEdgeDetection(blurred);
        
        // Step 4: Perform intelligent region growing
        QImage mask = regionGrow(blurred, seedPoint);
        
        // Step 5: Refine mask using edge information
        // Combine region growing with edge detection for better boundaries
        for (int y = 1; y < mask.height() - 1; ++y) {
            for (int x = 1; x < mask.width() - 1; ++x) {
                if (mask.pixelIndex(x, y) == 1) { // If pixel is in region
                    // Check if we're near a strong edge
                    float edgeStrength = edges.pixelColor(x, y).redF();
                    if (edgeStrength > m_edgeThreshold) {
                        // Strong edge - keep the boundary precise
                        continue;
                    }
                    
                    // Weak edge - check neighbors for refinement
                    int neighborCount = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            if (mask.pixelIndex(x + dx, y + dy) == 1) {
                                neighborCount++;
                            }
                        }
                    }
                    
                    // Remove isolated pixels or weak connections
                    if (neighborCount < 3) {
                        mask.setPixel(x, y, 0);
                    }
                }
            }
        }
        
        // Step 6: Extract contour from refined mask
        result.contour = extractContour(mask);
        
        if (result.contour.size() >= m_minContourSize) {
            result.mask = mask;
            result.success = true;
            
            // Calculate confidence based on contour quality
            float contourLength = 0;
            for (size_t i = 1; i < result.contour.size(); ++i) {
                QPointF diff = result.contour[i] - result.contour[i-1];
                contourLength += std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
            }
            
            // Higher confidence for smoother, longer contours
            result.confidence = std::min(1.0f, contourLength / (image.width() + image.height()));
            
            qDebug() << "EdgeSelectionTool: Success! Contour points:" << result.contour.size() 
                     << "Confidence:" << result.confidence;
            
            emit selectionComplete(result);
        } else {
            emit selectionFailed("Contour too small or invalid");
        }
        
    } catch (const std::exception& e) {
        qDebug() << "EdgeSelectionTool: Error:" << e.what();
        emit selectionFailed(QString("Selection failed: %1").arg(e.what()));
    }
    
    return result;
}

QImage EdgeSelectionTool::enhanceContrast(const QImage& image)
{
    QImage result = image.convertToFormat(QImage::Format_RGB888);
    
    // Simple contrast enhancement using histogram stretching
    int minR = 255, maxR = 0, minG = 255, maxG = 0, minB = 255, maxB = 0;
    
    // Find min/max values
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            QColor color = result.pixelColor(x, y);
            minR = std::min(minR, color.red());
            maxR = std::max(maxR, color.red());
            minG = std::min(minG, color.green());
            maxG = std::max(maxG, color.green());
            minB = std::min(minB, color.blue());
            maxB = std::max(maxB, color.blue());
        }
    }
    
    // Stretch histogram
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            QColor color = result.pixelColor(x, y);
            
            int newR = maxR > minR ? 255 * (color.red() - minR) / (maxR - minR) : color.red();
            int newG = maxG > minG ? 255 * (color.green() - minG) / (maxG - minG) : color.green();
            int newB = maxB > minB ? 255 * (color.blue() - minB) / (maxB - minB) : color.blue();
            
            result.setPixelColor(x, y, QColor(newR, newG, newB));
        }
    }
    
    return result;
}

QImage EdgeSelectionTool::gaussianBlur(const QImage& image, float sigma)
{
    // Simple 3x3 Gaussian blur approximation
    QImage result = image.convertToFormat(QImage::Format_RGB888);
    QImage temp = result;
    
    // Gaussian kernel (3x3)
    float kernel[3][3] = {
        {1, 2, 1},
        {2, 4, 2},
        {1, 2, 1}
    };
    float kernelSum = 16.0f;
    
    for (int y = 1; y < result.height() - 1; ++y) {
        for (int x = 1; x < result.width() - 1; ++x) {
            float r = 0, g = 0, b = 0;
            
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    QColor color = temp.pixelColor(x + kx, y + ky);
                    float weight = kernel[ky + 1][kx + 1];
                    r += color.red() * weight;
                    g += color.green() * weight;
                    b += color.blue() * weight;
                }
            }
            
            result.setPixelColor(x, y, QColor(
                std::min(255, (int)(r / kernelSum)),
                std::min(255, (int)(g / kernelSum)),
                std::min(255, (int)(b / kernelSum))
            ));
        }
    }
    
    return result;
}

QImage EdgeSelectionTool::sobelEdgeDetection(const QImage& image)
{
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    QImage result(gray.size(), QImage::Format_Grayscale8);
    
    // Sobel operators
    int sobelX[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int sobelY[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
    
    for (int y = 1; y < gray.height() - 1; ++y) {
        for (int x = 1; x < gray.width() - 1; ++x) {
            int gx = 0, gy = 0;
            
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    int pixel = gray.pixelColor(x + kx, y + ky).red();
                    gx += pixel * sobelX[ky + 1][kx + 1];
                    gy += pixel * sobelY[ky + 1][kx + 1];
                }
            }
            
            int magnitude = std::min(255, (int)std::sqrt(gx * gx + gy * gy));
            result.setPixel(x, y, qRgb(magnitude, magnitude, magnitude));
        }
    }
    
    return result;
}

QImage EdgeSelectionTool::regionGrow(const QImage& image, const QPointF& seedPoint)
{
    QImage mask(image.size(), QImage::Format_Mono);
    mask.fill(0);
    
    QPoint seed(seedPoint.x(), seedPoint.y());
    if (seed.x() < 0 || seed.x() >= image.width() || seed.y() < 0 || seed.y() >= image.height()) {
        return mask;
    }
    
    QColor targetColor = image.pixelColor(seed);
    floodFill(image, mask, seed, targetColor, m_growthTolerance);
    
    return mask;
}

void EdgeSelectionTool::floodFill(const QImage& image, QImage& mask, const QPoint& start, 
                                  const QColor& targetColor, float tolerance)
{
    std::queue<QPoint> queue;
    queue.push(start);
    mask.setPixel(start.x(), start.y(), 1);
    
    while (!queue.empty()) {
        QPoint current = queue.front();
        queue.pop();
        
        // Check 4-connected neighbors
        QPoint neighbors[4] = {
            {current.x() + 1, current.y()},
            {current.x() - 1, current.y()},
            {current.x(), current.y() + 1},
            {current.x(), current.y() - 1}
        };
        
        for (const QPoint& neighbor : neighbors) {
            if (neighbor.x() >= 0 && neighbor.x() < image.width() &&
                neighbor.y() >= 0 && neighbor.y() < image.height() &&
                mask.pixelIndex(neighbor.x(), neighbor.y()) == 0) {
                
                QColor neighborColor = image.pixelColor(neighbor);
                if (colorDistance(targetColor, neighborColor) <= tolerance) {
                    mask.setPixel(neighbor.x(), neighbor.y(), 1);
                    queue.push(neighbor);
                }
            }
        }
    }
}

std::vector<QPointF> EdgeSelectionTool::extractContour(const QImage& mask)
{
    std::vector<QPointF> contour;

    // Find first boundary pixel (pixel in mask with at least one non-mask neighbor)
    QPoint start(-1, -1);
    for (int y = 0; y < mask.height() && start.x() == -1; ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            if (mask.pixelIndex(x, y) == 1) {
                // Check if it's a boundary pixel (has a non-mask neighbor or is on image edge)
                bool isBoundary = (x == 0 || y == 0 || x == mask.width()-1 || y == mask.height()-1);
                if (!isBoundary) {
                    if (mask.pixelIndex(x-1, y) == 0 || mask.pixelIndex(x+1, y) == 0 ||
                        mask.pixelIndex(x, y-1) == 0 || mask.pixelIndex(x, y+1) == 0) {
                        isBoundary = true;
                    }
                }
                if (isBoundary) {
                    start = QPoint(x, y);
                    break;
                }
            }
        }
    }

    if (start.x() == -1) return contour;

    // Moore neighborhood tracing (clockwise: E, SE, S, SW, W, NW, N, NE)
    const QPoint dirs[8] = {
        {1, 0}, {1, 1}, {0, 1}, {-1, 1},
        {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
    };

    auto isMask = [&](const QPoint& p) -> bool {
        return p.x() >= 0 && p.x() < mask.width() &&
               p.y() >= 0 && p.y() < mask.height() &&
               mask.pixelIndex(p.x(), p.y()) == 1;
    };

    QPoint current = start;
    // backtrack starts from the left of start (coming from west)
    int backtrackDir = 4; // W direction as initial backtrack

    int maxIter = mask.width() * mask.height();
    int iter = 0;
    bool firstStep = true;

    do {
        contour.push_back(QPointF(current.x(), current.y()));

        // Start scanning clockwise from the direction after backtrack
        int startDir = (backtrackDir + 1) % 8;
        bool found = false;

        for (int i = 0; i < 8; ++i) {
            int d = (startDir + i) % 8;
            QPoint next = current + dirs[d];
            if (isMask(next)) {
                // Backtrack direction for next iteration is opposite of how we entered
                backtrackDir = (d + 4) % 8;
                current = next;
                found = true;
                break;
            }
        }

        if (!found) break;
        if (++iter > maxIter) break;

        if (firstStep) {
            firstStep = false;
        }
    } while (current != start);

    // Subsample if contour is very large (reduce to ~500 points max)
    if (contour.size() > 1000) {
        std::vector<QPointF> sampled;
        int step = contour.size() / 500;
        if (step < 1) step = 1;
        for (size_t i = 0; i < contour.size(); i += step) {
            sampled.push_back(contour[i]);
        }
        contour = sampled;
    }

    return contour;
}

bool EdgeSelectionTool::isValidSeed(const QImage& image, const QPointF& point)
{
    int x = point.x();
    int y = point.y();
    return x >= 0 && x < image.width() && y >= 0 && y < image.height();
}

float EdgeSelectionTool::colorDistance(const QColor& c1, const QColor& c2)
{
    // Euclidean distance in RGB space, normalized to [0,1]
    float dr = (c1.red() - c2.red()) / 255.0f;
    float dg = (c1.green() - c2.green()) / 255.0f;
    float db = (c1.blue() - c2.blue()) / 255.0f;
    return std::sqrt(dr * dr + dg * dg + db * db);
}
