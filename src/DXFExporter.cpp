#include "DXFExporter.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "Layer.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cmath>

QString DXFExporter::nextHandle()
{
    return QString::number(++m_handleCounter, 16).toUpper();
}

int DXFExporter::rgbToACI(const QColor &color)
{
    // Map to closest of the 7 standard ACI colors
    // 1=Red, 2=Yellow, 3=Green, 4=Cyan, 5=Blue, 6=Magenta, 7=White
    struct ACIColor { int index; int r, g, b; };
    static const ACIColor standardColors[] = {
        {1, 255, 0, 0},     // Red
        {2, 255, 255, 0},   // Yellow
        {3, 0, 255, 0},     // Green
        {4, 0, 255, 255},   // Cyan
        {5, 0, 0, 255},     // Blue
        {6, 255, 0, 255},   // Magenta
        {7, 255, 255, 255}, // White
    };

    int r = color.red(), g = color.green(), b = color.blue();
    int bestIdx = 7;
    double bestDist = 1e9;

    for (const auto &aci : standardColors) {
        double dist = std::sqrt(
            (r - aci.r) * (r - aci.r) +
            (g - aci.g) * (g - aci.g) +
            (b - aci.b) * (b - aci.b));
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = aci.index;
        }
    }
    return bestIdx;
}

void DXFExporter::writeHeader(QString &dxf, Units units,
                               double minX, double minY,
                               double maxX, double maxY)
{
    dxf += "  0\nSECTION\n  2\nHEADER\n";

    // DXF version
    dxf += "  9\n$ACADVER\n  1\nAC1015\n";

    // Measurement system (0=English, 1=Metric)
    int measurement = (units == Units::Inches) ? 0 : 1;
    dxf += QString("  9\n$MEASUREMENT\n 70\n%1\n").arg(measurement);

    // Insert units
    int insunits = 0;
    switch (units) {
    case Units::Inches: insunits = 1; break;
    case Units::Millimeters: insunits = 4; break;
    case Units::Centimeters: insunits = 5; break;
    }
    dxf += QString("  9\n$INSUNITS\n 70\n%1\n").arg(insunits);

    // Drawing extents
    dxf += QString("  9\n$EXTMIN\n 10\n%1\n 20\n%2\n 30\n0.0\n")
               .arg(minX, 0, 'f', 6).arg(minY, 0, 'f', 6);
    dxf += QString("  9\n$EXTMAX\n 10\n%1\n 20\n%2\n 30\n0.0\n")
               .arg(maxX, 0, 'f', 6).arg(maxY, 0, 'f', 6);

    dxf += "  0\nENDSEC\n";
}

void DXFExporter::writeLayerTable(QString &dxf,
                                   const std::vector<std::unique_ptr<Layer>> &layers,
                                   bool visibleOnly)
{
    dxf += "  0\nSECTION\n  2\nTABLES\n";
    dxf += "  0\nTABLE\n  2\nLAYER\n";

    int count = 0;
    for (const auto &layer : layers) {
        if (visibleOnly && !layer->isVisible()) continue;
        count++;
    }
    dxf += QString(" 70\n%1\n").arg(count);

    for (const auto &layer : layers) {
        if (visibleOnly && !layer->isVisible()) continue;

        QString layerName = layer->name();
        layerName.replace(' ', '_'); // DXF doesn't like spaces in layer names

        dxf += "  0\nLAYER\n";
        dxf += QString("  5\n%1\n").arg(nextHandle());
        dxf += "100\nAcDbSymbolTableRecord\n";
        dxf += "100\nAcDbLayerTableRecord\n";
        dxf += QString("  2\n%1\n").arg(layerName);
        dxf += " 70\n0\n"; // flags (0 = normal)
        dxf += QString(" 62\n%1\n").arg(rgbToACI(layer->color()));
        dxf += "  6\nContinuous\n"; // linetype
    }

    dxf += "  0\nENDTAB\n";
    dxf += "  0\nENDSEC\n";
}

void DXFExporter::writeEntitiesStart(QString &dxf)
{
    dxf += "  0\nSECTION\n  2\nENTITIES\n";
}

void DXFExporter::writeEntitiesEnd(QString &dxf)
{
    dxf += "  0\nENDSEC\n";
}

void DXFExporter::writeEOF(QString &dxf)
{
    dxf += "  0\nEOF\n";
}

void DXFExporter::writeLine(QString &dxf, const QString &layerName, int colorIndex,
                             double x1, double y1, double x2, double y2, double lineWidth)
{
    dxf += "  0\nLINE\n";
    dxf += QString("  5\n%1\n").arg(nextHandle());
    dxf += "100\nAcDbEntity\n";
    dxf += QString("  8\n%1\n").arg(layerName);
    dxf += QString(" 62\n%1\n").arg(colorIndex);
    if (lineWidth > 0) {
        dxf += QString("370\n%1\n").arg(static_cast<int>(lineWidth * 100)); // hundredths of mm
    }
    dxf += "100\nAcDbLine\n";
    dxf += QString(" 10\n%1\n 20\n%2\n 30\n0.0\n").arg(x1, 0, 'f', 6).arg(y1, 0, 'f', 6);
    dxf += QString(" 11\n%1\n 21\n%2\n 31\n0.0\n").arg(x2, 0, 'f', 6).arg(y2, 0, 'f', 6);
}

void DXFExporter::writeCircle(QString &dxf, const QString &layerName, int colorIndex,
                               double cx, double cy, double radius, double lineWidth)
{
    dxf += "  0\nCIRCLE\n";
    dxf += QString("  5\n%1\n").arg(nextHandle());
    dxf += "100\nAcDbEntity\n";
    dxf += QString("  8\n%1\n").arg(layerName);
    dxf += QString(" 62\n%1\n").arg(colorIndex);
    if (lineWidth > 0) {
        dxf += QString("370\n%1\n").arg(static_cast<int>(lineWidth * 100));
    }
    dxf += "100\nAcDbCircle\n";
    dxf += QString(" 10\n%1\n 20\n%2\n 30\n0.0\n").arg(cx, 0, 'f', 6).arg(cy, 0, 'f', 6);
    dxf += QString(" 40\n%1\n").arg(radius, 0, 'f', 6);
}

void DXFExporter::writeArc(QString &dxf, const QString &layerName, int colorIndex,
                            double cx, double cy, double radius,
                            double startAngle, double endAngle, double lineWidth)
{
    dxf += "  0\nARC\n";
    dxf += QString("  5\n%1\n").arg(nextHandle());
    dxf += "100\nAcDbEntity\n";
    dxf += QString("  8\n%1\n").arg(layerName);
    dxf += QString(" 62\n%1\n").arg(colorIndex);
    if (lineWidth > 0) {
        dxf += QString("370\n%1\n").arg(static_cast<int>(lineWidth * 100));
    }
    dxf += "100\nAcDbCircle\n";
    dxf += QString(" 10\n%1\n 20\n%2\n 30\n0.0\n").arg(cx, 0, 'f', 6).arg(cy, 0, 'f', 6);
    dxf += QString(" 40\n%1\n").arg(radius, 0, 'f', 6);
    dxf += "100\nAcDbArc\n";
    dxf += QString(" 50\n%1\n").arg(startAngle, 0, 'f', 6);
    dxf += QString(" 51\n%1\n").arg(endAngle, 0, 'f', 6);
}

void DXFExporter::writeText(QString &dxf, const QString &layerName, int colorIndex,
                             double x, double y, double height, const QString &text)
{
    dxf += "  0\nTEXT\n";
    dxf += QString("  5\n%1\n").arg(nextHandle());
    dxf += "100\nAcDbEntity\n";
    dxf += QString("  8\n%1\n").arg(layerName);
    dxf += QString(" 62\n%1\n").arg(colorIndex);
    dxf += "100\nAcDbText\n";
    dxf += QString(" 10\n%1\n 20\n%2\n 30\n0.0\n").arg(x, 0, 'f', 6).arg(y, 0, 'f', 6);
    dxf += QString(" 40\n%1\n").arg(height, 0, 'f', 6);
    dxf += QString("  1\n%1\n").arg(text);
    dxf += "100\nAcDbText\n";
}

void DXFExporter::writePolyline(QString &dxf, const QString &layerName, int colorIndex,
                                 const std::vector<QPointF> &points, bool closed,
                                 double lineWidth)
{
    if (points.size() < 2) return;

    dxf += "  0\nLWPOLYLINE\n";
    dxf += QString("  5\n%1\n").arg(nextHandle());
    dxf += "100\nAcDbEntity\n";
    dxf += QString("  8\n%1\n").arg(layerName);
    dxf += QString(" 62\n%1\n").arg(colorIndex);
    dxf += "100\nAcDbPolyline\n";
    dxf += QString(" 90\n%1\n").arg(static_cast<int>(points.size()));
    dxf += QString(" 70\n%1\n").arg(closed ? 1 : 0);
    if (lineWidth > 0) {
        dxf += QString(" 43\n%1\n").arg(lineWidth, 0, 'f', 6);
    }

    for (const auto &pt : points) {
        dxf += QString(" 10\n%1\n 20\n%2\n").arg(pt.x(), 0, 'f', 6).arg(pt.y(), 0, 'f', 6);
    }
}

void DXFExporter::exportPrimitive(QString &dxf, const DrawingPrimitive *prim,
                                   const QString &layerName, double scale)
{
    if (!prim || !prim->isVisible()) return;

    int aci = rgbToACI(prim->color());
    double lw = prim->lineWidth() * scale;

    switch (prim->type()) {
    case PrimitiveType::Line: {
        auto *line = static_cast<const LinePrimitive *>(prim);
        writeLine(dxf, layerName, aci,
                  line->startPoint().x() * scale, line->startPoint().y() * scale,
                  line->endPoint().x() * scale, line->endPoint().y() * scale, lw);
        break;
    }
    case PrimitiveType::Circle: {
        auto *circle = static_cast<const CirclePrimitive *>(prim);
        writeCircle(dxf, layerName, aci,
                    circle->center().x() * scale, circle->center().y() * scale,
                    circle->radius() * scale, lw);
        break;
    }
    case PrimitiveType::Arc: {
        auto *arc = static_cast<const ArcPrimitive *>(prim);
        writeArc(dxf, layerName, aci,
                 arc->center().x() * scale, arc->center().y() * scale,
                 arc->radius() * scale,
                 arc->startAngle(), arc->endAngle(), lw);
        break;
    }
    case PrimitiveType::Text: {
        auto *text = static_cast<const TextPrimitive *>(prim);
        writeText(dxf, layerName, aci,
                  text->position().x() * scale, text->position().y() * scale,
                  text->fontSize() * scale, text->text());
        break;
    }
    case PrimitiveType::Rectangle: {
        auto *rect = static_cast<const RectanglePrimitive *>(prim);
        double x1 = rect->topLeft().x() * scale;
        double y1 = rect->topLeft().y() * scale;
        double x2 = rect->bottomRight().x() * scale;
        double y2 = rect->bottomRight().y() * scale;
        std::vector<QPointF> pts = {{x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}};
        writePolyline(dxf, layerName, aci, pts, true, lw);
        break;
    }
    case PrimitiveType::Ellipse: {
        auto *ellipse = static_cast<const EllipsePrimitive *>(prim);
        // Approximate ellipse as polyline
        std::vector<QPointF> pts;
        int segments = 64;
        for (int i = 0; i < segments; ++i) {
            double angle = 2.0 * M_PI * i / segments;
            double x = ellipse->center().x() * scale + ellipse->radiusX() * scale * std::cos(angle);
            double y = ellipse->center().y() * scale + ellipse->radiusY() * scale * std::sin(angle);
            pts.emplace_back(x, y);
        }
        writePolyline(dxf, layerName, aci, pts, true, lw);
        break;
    }
    case PrimitiveType::Polygon: {
        auto *poly = static_cast<const PolygonPrimitive *>(prim);
        std::vector<QPointF> pts;
        for (const auto &pt : poly->points()) {
            pts.emplace_back(pt.x() * scale, pt.y() * scale);
        }
        writePolyline(dxf, layerName, aci, pts, poly->isClosed(), lw);
        break;
    }
    case PrimitiveType::Curve: {
        auto *curve = static_cast<const CurvePrimitive *>(prim);
        std::vector<QPointF> pts;
        for (const auto &pt : curve->controlPoints()) {
            pts.emplace_back(pt.x() * scale, pt.y() * scale);
        }
        if (pts.size() >= 2) {
            writePolyline(dxf, layerName, aci, pts, curve->isClosed(), lw);
        }
        break;
    }
    case PrimitiveType::Spline: {
        auto *spline = static_cast<const SplinePrimitive *>(prim);
        std::vector<QPointF> pts;
        for (const auto &pt : spline->points()) {
            pts.emplace_back(pt.x() * scale, pt.y() * scale);
        }
        if (pts.size() >= 2) {
            writePolyline(dxf, layerName, aci, pts, spline->isClosed(), lw);
        }
        break;
    }
    case PrimitiveType::BezierCurve: {
        auto *bezier = static_cast<const BezierCurvePrimitive *>(prim);
        // Approximate bezier as polyline by sampling
        std::vector<QPointF> pts;
        int steps = 50;
        for (int i = 0; i <= steps; ++i) {
            float t = static_cast<float>(i) / steps;
            QVector2D p = bezier->evaluateAt(t);
            pts.emplace_back(p.x() * scale, p.y() * scale);
        }
        writePolyline(dxf, layerName, aci, pts, false, lw);
        break;
    }
    case PrimitiveType::Dimension: {
        // Export as a line with the two endpoints
        auto *dim = static_cast<const DimensionPrimitive *>(prim);
        writeLine(dxf, layerName, aci,
                  dim->startPoint().x() * scale, dim->startPoint().y() * scale,
                  dim->endPoint().x() * scale, dim->endPoint().y() * scale, lw);
        break;
    }
    case PrimitiveType::Image:
        // Images cannot be exported to DXF
        break;
    }
}

bool DXFExporter::exportToFile(const QString &filePath,
                                const std::vector<std::unique_ptr<Layer>> &layers,
                                const Options &options)
{
    m_handleCounter = 0;
    QString dxf;
    dxf.reserve(1024 * 64);

    // Calculate extents
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (const auto &layer : layers) {
        if (options.exportVisibleOnly && !layer->isVisible()) continue;
        for (const auto &prim : layer->primitives()) {
            if (!prim->isVisible()) continue;
            QRectF bounds = prim->boundingRect();
            minX = std::min(minX, bounds.left() * options.scaleFactor);
            minY = std::min(minY, bounds.top() * options.scaleFactor);
            maxX = std::max(maxX, bounds.right() * options.scaleFactor);
            maxY = std::max(maxY, bounds.bottom() * options.scaleFactor);
        }
    }

    if (minX > maxX) { minX = 0; minY = 0; maxX = 1000; maxY = 1000; }

    writeHeader(dxf, options.units, minX, minY, maxX, maxY);
    writeLayerTable(dxf, layers, options.exportVisibleOnly);
    writeEntitiesStart(dxf);

    for (const auto &layer : layers) {
        if (options.exportVisibleOnly && !layer->isVisible()) continue;

        QString layerName = layer->name();
        layerName.replace(' ', '_');

        for (const auto &prim : layer->primitives()) {
            exportPrimitive(dxf, prim.get(), layerName, options.scaleFactor);
        }
    }

    writeEntitiesEnd(dxf);
    writeEOF(dxf);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "DXFExporter: Failed to open file:" << filePath;
        return false;
    }

    QTextStream stream(&file);
    stream << dxf;
    file.close();

    qDebug() << "DXFExporter: Exported to" << filePath;
    return true;
}

bool DXFExporter::exportLines(const QString &filePath,
                               const std::vector<std::pair<QPointF, QPointF>> &lines,
                               Units units)
{
    m_handleCounter = 0;
    QString dxf;
    dxf.reserve(1024 * 32);

    // Calculate extents
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (const auto &line : lines) {
        minX = std::min({minX, line.first.x(), line.second.x()});
        minY = std::min({minY, line.first.y(), line.second.y()});
        maxX = std::max({maxX, line.first.x(), line.second.x()});
        maxY = std::max({maxY, line.first.y(), line.second.y()});
    }
    if (minX > maxX) { minX = 0; minY = 0; maxX = 1000; maxY = 1000; }

    // Write a minimal DXF with just header, one layer, and line entities
    writeHeader(dxf, units, minX, minY, maxX, maxY);

    // Minimal layer table with just "ExtractedLines"
    dxf += "  0\nSECTION\n  2\nTABLES\n";
    dxf += "  0\nTABLE\n  2\nLAYER\n 70\n1\n";
    dxf += "  0\nLAYER\n";
    dxf += QString("  5\n%1\n").arg(nextHandle());
    dxf += "100\nAcDbSymbolTableRecord\n100\nAcDbLayerTableRecord\n";
    dxf += "  2\nExtractedLines\n 70\n0\n 62\n7\n  6\nContinuous\n";
    dxf += "  0\nENDTAB\n  0\nENDSEC\n";

    writeEntitiesStart(dxf);

    for (const auto &line : lines) {
        writeLine(dxf, "ExtractedLines", 7,
                  line.first.x(), line.first.y(),
                  line.second.x(), line.second.y(), 0);
    }

    writeEntitiesEnd(dxf);
    writeEOF(dxf);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "DXFExporter: Failed to open file:" << filePath;
        return false;
    }

    QTextStream stream(&file);
    stream << dxf;
    file.close();

    qDebug() << "DXFExporter: Exported" << lines.size() << "lines to" << filePath;
    return true;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
