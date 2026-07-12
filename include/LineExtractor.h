#pragma once

#include <QObject>
#include <QImage>
#include <QPointF>
#include <vector>

class LineExtractor : public QObject {
    Q_OBJECT

public:
    explicit LineExtractor(QObject *parent = nullptr);

    struct ExtractedLine {
        QPointF start;
        QPointF end;
        float angle;   // degrees
        float length;  // pixels
        int votes;     // Hough accumulator strength
    };

    struct Parameters {
        int edgeThreshold = 50;        // 0-255 for binary edge threshold
        int houghThreshold = 80;       // min votes to detect a line
        double minLineLength = 30.0;   // min segment length in pixels
        double maxLineGap = 10.0;      // max gap to merge collinear segments
        double angleSnapDegrees = 2.0; // snap to H/V within this tolerance
        bool mergeCollinear = true;
    };

    std::vector<ExtractedLine> extractLines(const QImage &image, const Parameters &params);
    QImage getEdgePreview(const QImage &image, int threshold);

signals:
    void progressChanged(int percent);

private:
    QImage toGrayscale(const QImage &image);
    QImage gaussianBlur(const QImage &gray);
    QImage sobelEdgeDetection(const QImage &gray);
    QImage thresholdImage(const QImage &edges, int threshold);

    // Probabilistic Hough Line Transform
    std::vector<ExtractedLine> houghLinesP(const QImage &binary,
                                           int houghThreshold,
                                           double minLineLength,
                                           double maxLineGap);

    // Post-processing
    void mergeCollinearSegments(std::vector<ExtractedLine> &lines, double maxGap);
    void snapToAxisAligned(std::vector<ExtractedLine> &lines, double angleTolerance);
    void removeDuplicates(std::vector<ExtractedLine> &lines, double distThreshold = 5.0);
};
