#pragma once

#include <QObject>
#include <QImage>
#include <QPolygonF>
#include <QPointF>
#include <QVector>
#include <QString>
#include <QRectF>

struct ContourPoint {
    QPointF position;
    float curvature;
    bool isCorner;
    
    ContourPoint(const QPointF &pos = QPointF(), float curv = 0.0f, bool corner = false)
        : position(pos), curvature(curv), isCorner(corner) {}
};

struct GuitarOutline {
    QVector<ContourPoint> bodyOutline;
    QVector<ContourPoint> neckOutline;
    QVector<ContourPoint> headstockOutline;
    QRectF boundingRect;
    
    // Optional component areas detected
    QVector<QRectF> pickupCavities;
    QVector<QPointF> bridgePosts;
    QVector<QPointF> controlPositions;
    QPointF nutPosition;
    QPointF bridgePosition;
    
    bool hasValidBody() const { return !bodyOutline.isEmpty(); }
    bool hasValidNeck() const { return !neckOutline.isEmpty(); }
    bool hasValidHeadstock() const { return !headstockOutline.isEmpty(); }
};

class BlueprintParser : public QObject
{
    Q_OBJECT

public:
    explicit BlueprintParser(QObject *parent = nullptr);
    ~BlueprintParser() = default;

    // Main parsing function
    GuitarOutline parseBlueprint(const QImage &blueprintImage);
    
    // Configuration
    void setEdgeThreshold(int threshold) { m_edgeThreshold = threshold; }
    void setMinContourLength(int length) { m_minContourLength = length; }
    void setCornerThreshold(float threshold) { m_cornerThreshold = threshold; }
    void setSimplificationTolerance(float tolerance) { m_simplificationTolerance = tolerance; }
    
    // Analysis settings
    void setAnalyzeComponents(bool analyze) { m_analyzeComponents = analyze; }
    
    // Get processing details
    QImage getEdgeMap() const { return m_edgeMap; }
    QImage getProcessedImage() const { return m_processedImage; }
    QString getLastError() const { return m_lastError; }

signals:
    void parseProgress(int percentage);
    void parseComplete(const GuitarOutline &outline);
    void parseError(const QString &error);

private:
    // Image processing steps
    QImage preprocessImage(const QImage &input);
    QImage detectEdges(const QImage &input);
    QVector<QPolygonF> findContours(const QImage &edgeMap);
    QVector<ContourPoint> processContour(const QPolygonF &contour);
    
    // Guitar-specific analysis
    GuitarOutline analyzeGuitarShape(const QVector<QPolygonF> &contours);
    QPolygonF identifyBodyContour(const QVector<QPolygonF> &contours);
    QPolygonF identifyNeckContour(const QVector<QPolygonF> &contours, const QPolygonF &body);
    QPolygonF identifyHeadstockContour(const QVector<QPolygonF> &contours, const QPolygonF &neck);
    
    // Component detection
    void detectComponents(GuitarOutline &outline, const QImage &processedImage);
    QVector<QRectF> detectPickupCavities(const QImage &image, const QPolygonF &bodyContour);
    QVector<QPointF> detectBridgePosts(const QImage &image, const QPolygonF &bodyContour);
    QVector<QPointF> detectControls(const QImage &image, const QPolygonF &bodyContour);
    QPointF detectNutPosition(const QPolygonF &neckContour);
    QPointF detectBridgePosition(const QPolygonF &bodyContour);
    
    // Utility functions
    float calculateCurvature(const QPolygonF &contour, int index, int windowSize = 3);
    bool isCornerPoint(const QPolygonF &contour, int index, float threshold);
    QPolygonF simplifyContour(const QPolygonF &contour, float tolerance);
    QPolygonF smoothContour(const QPolygonF &contour, int iterations = 1);
    
    // Contour analysis
    QRectF calculateBoundingRect(const QVector<QPolygonF> &contours);
    float calculateContourArea(const QPolygonF &contour);
    QPointF calculateContourCentroid(const QPolygonF &contour);
    
    // Edge detection utilities
    QImage applySobelFilter(const QImage &input);
    QImage applyCannyEdgeDetection(const QImage &input, int lowThreshold, int highThreshold);
    QImage applyGaussianBlur(const QImage &input, float sigma);
    
    // Configuration parameters
    int m_edgeThreshold;
    int m_minContourLength;
    float m_cornerThreshold;
    float m_simplificationTolerance;
    bool m_analyzeComponents;
    
    // Processing results
    QImage m_processedImage;
    QImage m_edgeMap;
    QString m_lastError;
};