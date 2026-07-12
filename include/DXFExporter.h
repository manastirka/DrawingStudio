#pragma once

#include <QString>
#include <QPointF>
#include <QColor>
#include <vector>
#include <memory>

class Layer;
class DrawingPrimitive;

class DXFExporter {
public:
    enum class Units { Inches = 1, Millimeters = 4, Centimeters = 5 };

    struct Options {
        Units units = Units::Millimeters;
        bool exportVisibleOnly = true;
        double scaleFactor = 1.0;
    };

    bool exportToFile(const QString &filePath,
                      const std::vector<std::unique_ptr<Layer>> &layers,
                      const Options &options);

    bool exportLines(const QString &filePath,
                     const std::vector<std::pair<QPointF, QPointF>> &lines,
                     Units units = Units::Millimeters);

private:
    void writeHeader(QString &dxf, Units units, double minX, double minY,
                     double maxX, double maxY);
    void writeLayerTable(QString &dxf,
                         const std::vector<std::unique_ptr<Layer>> &layers,
                         bool visibleOnly);
    void writeEntitiesStart(QString &dxf);
    void writeEntitiesEnd(QString &dxf);
    void writeEOF(QString &dxf);

    void writeLine(QString &dxf, const QString &layerName, int colorIndex,
                   double x1, double y1, double x2, double y2, double lineWidth);
    void writeCircle(QString &dxf, const QString &layerName, int colorIndex,
                     double cx, double cy, double radius, double lineWidth);
    void writeArc(QString &dxf, const QString &layerName, int colorIndex,
                  double cx, double cy, double radius,
                  double startAngle, double endAngle, double lineWidth);
    void writeText(QString &dxf, const QString &layerName, int colorIndex,
                   double x, double y, double height, const QString &text);
    void writePolyline(QString &dxf, const QString &layerName, int colorIndex,
                       const std::vector<QPointF> &points, bool closed, double lineWidth);

    void exportPrimitive(QString &dxf, const DrawingPrimitive *prim,
                         const QString &layerName, double scale);

    static int rgbToACI(const QColor &color);
    int m_handleCounter = 0;
    QString nextHandle();
};
