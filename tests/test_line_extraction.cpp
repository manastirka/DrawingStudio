#include <QApplication>
#include <QImage>
#include <QFile>
#include <QDebug>
#include <cmath>
#include "LineExtractor.h"
#include "DXFExporter.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Load the test floor plan
    QString imagePath = QString::fromUtf8(SOURCE_DIR) + "/../test_floor_plan.png";
    QImage image(imagePath);
    if (image.isNull()) {
        qCritical() << "FAIL: Could not load test image:" << imagePath;
        return 1;
    }
    qDebug() << "Loaded image:" << image.size();

    // Test 1: Edge preview
    LineExtractor extractor;
    QImage edgePreview = extractor.getEdgePreview(image, 50);
    if (edgePreview.isNull() || edgePreview.size() != image.size()) {
        qCritical() << "FAIL: Edge preview failed";
        return 1;
    }
    qDebug() << "PASS: Edge preview generated," << edgePreview.size();

    // Test 2: Line extraction (floor-plan mode)
    LineExtractor::Parameters params;
    params.floorPlanMode = true;
    params.inkThreshold = -1;
    params.edgeThreshold = 50;
    params.houghThreshold = 50;
    params.minLineLength = 25.0;
    params.maxLineGap = 12.0;
    params.mergeCollinear = true;
    params.angleSnapDegrees = 2.0;
    params.deskew = false; // test plan is already axis-aligned
    params.morphCleanup = true;
    params.invert = false;
    params.scaleAwareDefaults = true;
    params.junctionSnap = 6.0;

    auto lines = extractor.extractLines(image, params);
    qDebug() << "PASS: Extracted" << lines.size() << "lines";

    // Also require that door-split walls stay split: count segments with gaps
    int splitPairs = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        for (size_t j = i + 1; j < lines.size(); ++j) {
            const float ai = std::abs(lines[i].angle);
            const float aj = std::abs(lines[j].angle);
            const bool bothH = (ai < 15 || ai > 165) && (aj < 15 || aj > 165);
            if (!bothH)
                continue;
            const double yi = 0.5 * (lines[i].start.y() + lines[i].end.y());
            const double yj = 0.5 * (lines[j].start.y() + lines[j].end.y());
            if (std::abs(yi - yj) > 4)
                continue;
            const double ai0 = std::min(lines[i].start.x(), lines[i].end.x());
            const double ai1 = std::max(lines[i].start.x(), lines[i].end.x());
            const double aj0 = std::min(lines[j].start.x(), lines[j].end.x());
            const double aj1 = std::max(lines[j].start.x(), lines[j].end.x());
            const double gap = std::max(aj0 - ai1, ai0 - aj1);
            if (gap > 20 && gap < 200)
                ++splitPairs;
        }
    }
    qDebug() << "  Door-gap split pairs:" << splitPairs;
    if (splitPairs < 2) {
        qCritical() << "FAIL: Expected door openings to remain as gaps in walls";
        return 1;
    }

    if (lines.size() < 16) {
        qCritical() << "FAIL: Too few lines for test floor plan (got"
                    << lines.size() << ", expected >= 16)";
        return 1;
    }

    // Count H vs V
    int nH = 0, nV = 0;
    for (const auto &l : lines) {
        const float a = std::abs(l.angle);
        if (a < 15.f || a > 165.f)
            ++nH;
        else if (std::abs(a - 90.f) < 15.f)
            ++nV;
    }
    qDebug() << "  Horizontal:" << nH << "Vertical:" << nV;
    if (nH < 6 || nV < 6) {
        qCritical() << "FAIL: Expected both H and V walls (H>=6 V>=6)";
        return 1;
    }

    // Stair-side partitions at x≈510 and x≈590 must be recovered
    auto hasVertNear = [&](double xTarget, double minLen = 80.0) {
        for (const auto &l : lines) {
            const float a = std::abs(l.angle);
            if (std::abs(a - 90.f) > 15.f)
                continue;
            const double x = 0.5 * (l.start.x() + l.end.x());
            if (std::abs(x - xTarget) <= 4.0 && l.length >= minLen)
                return true;
        }
        return false;
    };
    if (!hasVertNear(510) || !hasVertNear(590)) {
        qCritical() << "FAIL: Missing stair-side vertical walls near x=510/590";
        return 1;
    }
    qDebug() << "PASS: Stair-side verticals present";

    // Vertical wall at x≈500 must stay split by the door (not one full-height line)
    int segsAt500 = 0;
    for (const auto &l : lines) {
        const float a = std::abs(l.angle);
        if (std::abs(a - 90.f) > 15.f)
            continue;
        const double x = 0.5 * (l.start.x() + l.end.x());
        if (std::abs(x - 500.0) <= 4.0)
            ++segsAt500;
    }
    if (segsAt500 < 2) {
        qCritical() << "FAIL: Expected door gap on vertical wall near x=500 (got"
                    << segsAt500 << "segments)";
        return 1;
    }
    qDebug() << "PASS: Door-split vertical at x=500 (" << segsAt500 << "segs)";

    // Endpoints should be integer pixels after precision pass
    int nonInt = 0;
    for (const auto &l : lines) {
        auto isInt = [](double v) { return std::abs(v - std::round(v)) < 1e-6; };
        if (!isInt(l.start.x()) || !isInt(l.start.y()) || !isInt(l.end.x()) ||
            !isInt(l.end.y()))
            ++nonInt;
    }
    if (nonInt > 0) {
        qCritical() << "FAIL: Expected integer endpoints, got" << nonInt
                    << "non-integer lines";
        return 1;
    }
    qDebug() << "PASS: Integer endpoints";

    // Test 2b: Auto ink threshold
    const int autoT = LineExtractor::suggestInkThreshold(image, false);
    qDebug() << "PASS: Suggested ink threshold:" << autoT;
    if (autoT < 10 || autoT > 250) {
        qCritical() << "FAIL: Auto threshold out of range";
        return 1;
    }

    // Print some sample lines
    int count = 0;
    for (const auto &line : lines) {
        if (count++ < 8) {
            qDebug() << "  Line:" << line.start << "->" << line.end
                     << "angle:" << line.angle << "len:" << line.length
                     << "votes:" << line.votes;
        }
    }

    // Test 3: DXF export of extracted lines
    QString dxfPath = QString::fromUtf8(SOURCE_DIR) + "/../test_output.dxf";
    std::vector<std::pair<QPointF, QPointF>> linePairs;
    for (const auto &line : lines) {
        linePairs.emplace_back(line.start, line.end);
    }

    DXFExporter exporter;
    bool exported = exporter.exportLines(dxfPath, linePairs, DXFExporter::Units::Millimeters);
    if (!exported) {
        qCritical() << "FAIL: DXF export failed";
        return 1;
    }
    qDebug() << "PASS: Exported" << lines.size() << "lines to" << dxfPath;

    // Test 4: Verify DXF file exists and has content
    QFile dxfFile(dxfPath);
    if (!dxfFile.exists() || dxfFile.size() == 0) {
        qCritical() << "FAIL: DXF file is empty or missing";
        return 1;
    }
    qDebug() << "PASS: DXF file size:" << dxfFile.size() << "bytes";

    // Test 5: Check DXF has expected sections
    dxfFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString dxfContent = dxfFile.readAll();
    dxfFile.close();

    bool hasHeader = dxfContent.contains("HEADER");
    bool hasEntities = dxfContent.contains("ENTITIES");
    bool hasLine = dxfContent.contains("LINE");
    bool hasEOF = dxfContent.contains("EOF");

    qDebug() << "DXF sections - HEADER:" << hasHeader
             << "ENTITIES:" << hasEntities
             << "LINE:" << hasLine
             << "EOF:" << hasEOF;

    if (!hasHeader || !hasEntities || !hasLine || !hasEOF) {
        qCritical() << "FAIL: DXF missing required sections";
        return 1;
    }
    qDebug() << "PASS: DXF structure validated";

    qDebug() << "\n=== ALL TESTS PASSED ===";
    return 0;
}
