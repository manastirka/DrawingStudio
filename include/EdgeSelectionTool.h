#pragma once

#include <QObject>
#include <QImage>
#include <QPointF>
#include <QVector2D>
#include <vector>

class EdgeSelectionTool : public QObject
{
    Q_OBJECT

public:
    explicit EdgeSelectionTool(QObject *parent = nullptr);
    
    struct SelectionResult {
        std::vector<QPointF> contour;
        QImage mask;
        float confidence;
        bool success;
    };
    
    // Intelligent edge-based selection
    SelectionResult selectByEdges(const QImage& image, const QPointF& seedPoint);
    
    // Configuration
    void setEdgeThreshold(float threshold) { m_edgeThreshold = threshold; }
    void setGrowthTolerance(float tolerance) { m_growthTolerance = tolerance; }
    void setMinContourSize(int size) { m_minContourSize = size; }
    
signals:
    void selectionComplete(const SelectionResult& result);
    void selectionFailed(const QString& error);

private:
    // Edge detection and region growing
    QImage detectEdges(const QImage& image);
    QImage regionGrow(const QImage& image, const QPointF& seedPoint);
    std::vector<QPointF> extractContour(const QImage& mask);
    
    // Intelligent preprocessing
    QImage enhanceContrast(const QImage& image);
    QImage gaussianBlur(const QImage& image, float sigma);
    QImage sobelEdgeDetection(const QImage& image);
    
    // Region growing helpers
    bool isValidSeed(const QImage& image, const QPointF& point);
    float colorDistance(const QColor& c1, const QColor& c2);
    void floodFill(const QImage& image, QImage& mask, const QPoint& start, 
                   const QColor& targetColor, float tolerance);
    
    // Parameters
    float m_edgeThreshold;
    float m_growthTolerance;
    int m_minContourSize;
};
