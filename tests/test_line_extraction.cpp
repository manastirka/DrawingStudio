#include <QApplication>
#include <QImage>
#include <QFile>
#include <QDebug>
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

    // Test 2: Line extraction
    LineExtractor::Parameters params;
    params.edgeThreshold = 50;
    params.houghThreshold = 60;
    params.minLineLength = 30.0;
    params.maxLineGap = 10.0;
    params.mergeCollinear = true;
    params.angleSnapDegrees = 2.0;

    auto lines = extractor.extractLines(image, params);
    qDebug() << "PASS: Extracted" << lines.size() << "lines";

    if (lines.empty()) {
        qCritical() << "FAIL: No lines detected from floor plan";
        return 1;
    }

    // Print some sample lines
    int count = 0;
    for (const auto &line : lines) {
        if (count++ < 5) {
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
