#pragma once

#include <QObject>
#include <QImage>
#include <QPointF>
#include <vector>

/**
 * Floor-plan aware line extractor.
 *
 * Primary path (floorPlanMode): ink binarization → H/V morphological wall
 * filter → run-length segments → junction snap / merge.
 * Fallback: Sobel + probabilistic Hough (generic drawings).
 */
class LineExtractor : public QObject {
    Q_OBJECT

public:
    explicit LineExtractor(QObject *parent = nullptr);

    struct ExtractedLine {
        QPointF start;
        QPointF end;
        float angle = 0.f;
        float length = 0.f;
        int votes = 0;
    };

    struct Parameters {
        /** Prefer architectural wall pipeline (ink + H/V morph). */
        bool floorPlanMode = true;

        /** Ink threshold 0–255; -1 = Otsu auto. Used in floor-plan mode. */
        int inkThreshold = -1;

        int edgeThreshold = 50;        // Sobel path only
        int houghThreshold = 80;
        double minLineLength = 30.0;
        double maxLineGap = 12.0;
        double angleSnapDegrees = 2.0;
        bool mergeCollinear = true;
        bool invert = false;
        bool deskew = true;
        bool morphCleanup = true;
        double duplicateDist = 6.0;
        bool scaleAwareDefaults = true;

        /** Junction snap radius in pixels (0 = off). */
        double junctionSnap = 6.0;

        /** Wall morph kernel length; 0 = auto (~2% of min side, clamped). */
        int wallKernelLength = 0;
        int wallKernelThickness = 2;
    };

    std::vector<ExtractedLine> extractLines(const QImage &image, const Parameters &params);

    /** Preview of the working binary / wall mask (for dialog). */
    QImage getEdgePreview(const QImage &image, int threshold, bool invert = false,
                          bool morphCleanup = true, bool floorPlanMode = true);

    static int suggestEdgeThreshold(const QImage &image, bool invert = false);
    static int suggestInkThreshold(const QImage &image, bool invert = false);
    static void suggestScaleDefaults(int width, int height,
                                     double *minLineLengthOut,
                                     double *maxLineGapOut);

signals:
    void progressChanged(int percent);

private:
    QImage toGrayscale(const QImage &image);
    QImage invertGray(const QImage &gray);
    QImage gaussianBlur(const QImage &gray);
    QImage sobelEdgeDetection(const QImage &gray);
    QImage thresholdImage(const QImage &src, int threshold, bool inv = false);
    QImage otsuBinarizeInk(const QImage &gray); // walls=255
    QImage adaptiveBinarizeInk(const QImage &gray);
    QImage morphologicalOpen(const QImage &binary);
    QImage morphologicalClose(const QImage &binary);
    QImage morphOpenRect(const QImage &binary, int kw, int kh);
    QImage morphDilate(const QImage &binary, int iters);
    QImage deskewImage(const QImage &gray, double *skewDegreesOut);
    QImage extractWallMask(const QImage &inkBinary, int kernLen, int thickness);

    std::vector<ExtractedLine> extractAxisAlignedRuns(const QImage &wallMask,
                                                      double minLength,
                                                      double maxGap);
    std::vector<ExtractedLine> houghLinesP(const QImage &binary,
                                           int houghThreshold,
                                           double minLineLength,
                                           double maxLineGap);

    void mergeCollinearSegments(std::vector<ExtractedLine> &lines, double maxGap,
                                double angleTol = 5.0);
    void snapToAxisAligned(std::vector<ExtractedLine> &lines, double angleTolerance);
    void snapJunctions(std::vector<ExtractedLine> &lines, double snapDist);
    void quantizeAxes(std::vector<ExtractedLine> &lines, double band);
    void extendToMeetJunctions(std::vector<ExtractedLine> &lines, double maxExtend);
    void recoverPartitionWalls(const QImage &ink, std::vector<ExtractedLine> &lines,
                               double minLength);
    void collapseParallelWalls(std::vector<ExtractedLine> &lines, double band);
    void removeDuplicates(std::vector<ExtractedLine> &lines, double distThreshold = 5.0);
    void removeShortRemnants(std::vector<ExtractedLine> &lines, double minLength);
    void removeParallelOffsetDuplicates(std::vector<ExtractedLine> &lines,
                                        double offsetBand);
    void filterByInkSupport(std::vector<ExtractedLine> &lines, const QImage &ink,
                            double minCoverage);
    void trimAllToInk(std::vector<ExtractedLine> &lines, const QImage &ink,
                      double minKeepRatio);
    void roundToPixels(std::vector<ExtractedLine> &lines);
    static double inkCoverage(const QImage &ink, const ExtractedLine &line,
                              int halfBand = 1);
    static ExtractedLine trimToInk(const QImage &ink, const ExtractedLine &line,
                                   int halfBand = 1);
    static ExtractedLine makeLine(QPointF a, QPointF b, int votes = 1);
};
