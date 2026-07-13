#include "tools/MeasureTool.h"

#include "DrawingPrimitive.h"

#include <QDebug>

namespace {
// Match legacy measure stroke (kept on commit — not replaced by defaultColor)
const QColor kMeasureColor(255, 200, 0);

void applyUnits(DimensionPrimitive *dim, const ToolHost &host)
{
    if (!dim) {
        return;
    }
    if (host.unitsString) {
        dim->setUnitsString(host.unitsString());
    }
    if (host.pixelsPerUnit) {
        dim->setPixelsPerUnit(host.pixelsPerUnit());
    }
    dim->recalculateMeasurement();
}
} // namespace

void MeasureTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.drawStart || !host.drawCurrent ||
        !host.currentPrimitive) {
        return;
    }
    if (*host.isDrawing || event->button() != Qt::LeftButton) {
        return;
    }

    qDebug() << "MeasureTool::onPress";

    QVector2D worldPos =
        host.screenToWorld ? host.screenToWorld(event->pos()) : QVector2D();
    *host.drawStart =
        host.snapToGrid ? host.snapToGrid(worldPos) : worldPos;
    *host.drawCurrent = *host.drawStart;

    auto dimension =
        std::make_unique<DimensionPrimitive>(*host.drawStart, *host.drawCurrent);
    applyUnits(dimension.get(), host);
    dimension->setColor(kMeasureColor);
    dimension->setSelected(true);

    *host.currentPrimitive = std::move(dimension);
    *host.isDrawing = true;

    if (host.setSmartHint) {
        host.setSmartHint(QStringLiteral(
            "Measuring — drag to set length, release to place"));
    }
    if (host.lastSmartHint) {
        *host.lastSmartHint = QStringLiteral(
            "Measuring — drag to set length, release to place");
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void MeasureTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.drawCurrent ||
        !host.currentPrimitive || !*host.currentPrimitive) {
        return;
    }

    QVector2D worldPos =
        host.screenToWorld ? host.screenToWorld(event->pos()) : QVector2D();
    *host.drawCurrent =
        host.snapToGrid ? host.snapToGrid(worldPos) : worldPos;

    QString smartHint;
    if (host.smartConstraints) {
        *host.drawCurrent = host.smartConstraints(
            *host.drawCurrent, event->modifiers(), &smartHint);
    }

    if (auto *dim =
            dynamic_cast<DimensionPrimitive *>(host.currentPrimitive->get())) {
        QVector2D snappedEnd = host.snapToEndpoint
                                   ? host.snapToEndpoint(*host.drawCurrent)
                                   : *host.drawCurrent;
        dim->setEndPoint(snappedEnd);
        applyUnits(dim, host);
        // Prefer live measurement over axis-lock coaching hint
        smartHint =
            QStringLiteral("Measuring: %1").arg(dim->getDisplayText());
    }

    if (host.lastSmartHint && host.setSmartHint &&
        smartHint != *host.lastSmartHint) {
        *host.lastSmartHint = smartHint;
        host.setSmartHint(smartHint);
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void MeasureTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.currentPrimitive) {
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    bool shouldAdd = false;
    if (auto *dim =
            dynamic_cast<DimensionPrimitive *>(host.currentPrimitive->get())) {
        if (host.drawCurrent) {
            QVector2D snappedEnd = host.snapToEndpoint
                                       ? host.snapToEndpoint(*host.drawCurrent)
                                       : *host.drawCurrent;
            dim->setEndPoint(snappedEnd);
        }
        applyUnits(dim, host);
        const float len = (dim->endPoint() - dim->startPoint()).length();
        shouldAdd = len >= 2.0f;
    }

    if (shouldAdd && host.commitPrimitive && *host.currentPrimitive) {
        // Keep measure color (do not overwrite with default stroke)
        (*host.currentPrimitive)->setSelected(true);
        host.commitPrimitive(std::move(*host.currentPrimitive));
    } else if (host.currentPrimitive) {
        host.currentPrimitive->reset();
    }

    *host.isDrawing = false;

    if (host.lastSmartHint && !host.lastSmartHint->isEmpty() &&
        host.setSmartHint) {
        host.lastSmartHint->clear();
        host.setSmartHint(QString());
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}
