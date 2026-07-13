#include "DrawingCanvas.h"

#include "AlignmentGuides.h"
#include "CanvasCoords.h"
#include "CanvasPaintPipeline.h"
#include "CanvasPrimitiveOps.h"
#include "CanvasRenderer.h"
#include "DrawingPrimitive.h"
#include "GridManager.h"
#include "LayerManager.h"
#include "OverlayRenderer.h"
#include "PaperModel.h"
#include "RulerRenderer.h"
#include "SelectionManager.h"
#include "SelectionPolicy.h"
#include "SelectionTransform.h"
#include "TextRenderer.h"
#include "ToolCursor.h"
#include "UnitsConverter.h"

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QUuid>
#include <cmath>
#include <vector>

// Paint / render passes (refactor E12) — keeps DrawingCanvas.cpp on document ops.

void DrawingCanvas::setupWorldTransform(QPainter &painter)
{
    CanvasCoords::applyWorldTransform(painter, width(), height(), m_zoomLevel,
                                      m_viewCenter);
}

void DrawingCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    CanvasPaintPipeline::clearBackground(painter, rect(), m_backgroundColor);

    m_renderer->setPainter(&painter);

    renderPaper(painter);

    painter.setRenderHint(QPainter::Antialiasing, false);
    renderGrid(painter);
    if (m_rulersVisible) {
        renderRulers(painter);
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    renderObjects(painter);
    renderSelection(painter);

    if (selectedObjects().empty()) {
        m_alignmentGuides.clear();
        m_snapIndicatorActive = false;
    }

    renderAlignmentGuides(painter);
    renderTool(painter);
    renderLassoOverlay(painter);
    renderDimensionTexts(painter);
    renderTextPrimitives(painter);
    renderSnapIndicator(painter);
    renderCursorPreview(painter);

    if (m_isRotatingText && m_rotatingTextPrimitive) {
        const float rotationDegrees =
            m_rotatingTextPrimitive->rotation() * 180.0f /
            static_cast<float>(M_PI);
        const QRectF bounds = m_rotatingTextPrimitive->boundingRect();
        const float centerX =
            static_cast<float>((bounds.left() + bounds.right()) / 2.0);
        const float rotateHandleY = static_cast<float>(bounds.top() - 20.0);
        const QPoint screenPos =
            worldToScreen(QVector2D(centerX, rotateHandleY - 15.0f));
        CanvasPaintPipeline::drawRotationAngleHud(painter, screenPos,
                                                  rotationDegrees);
    }

    if (m_rulersVisible) {
        renderRulerTexts(painter);
    }

    m_renderer->setPainter(nullptr);
}

void DrawingCanvas::renderGrid(QPainter &painter)
{
    if (!m_renderer) {
        return;
    }

    painter.save();
    setupWorldTransform(painter);

    m_gridManager->setPaperSize(m_paper.sizeMm());
    m_gridManager->setPixelsPerMM(m_unitsConverter.pixelsPerMM());
    m_gridManager->render(m_renderer.get());

    painter.restore();
}

void DrawingCanvas::renderRulers(QPainter &painter)
{
    if (!m_rulersVisible) {
        return;
    }
    RulerRenderer::renderTicks(painter, width(), height(), m_zoomLevel,
                               m_unitsConverter.units());
}

void DrawingCanvas::renderObjects(QPainter &painter)
{
    painter.save();
    setupWorldTransform(painter);
    CanvasPaintPipeline::renderWorldObjects(
        painter, m_layerManager, m_primitives, m_currentPrimitive.get(),
        [this]() { return selectedObjects(); },
        [this](const DrawingPrimitive *o) {
            return showsGeometryControlPoints(o);
        },
        [this](QPainter &p, const QVector2D &pt, bool hi) {
            renderControlPoint(p, pt, hi);
        });
    painter.restore();
}

void DrawingCanvas::renderSelection(QPainter &painter)
{
    if (!m_renderer) {
        return;
    }

    painter.save();
    setupWorldTransform(painter);
    m_selectionManager->render(m_renderer.get());

    if (m_isEditingControlPoints && m_editingPrimitive) {
        for (const auto &point : m_editingPrimitive->getControlPoints()) {
            renderControlPoint(painter, point, true);
        }
    }

    for (auto *obj : selectedObjects()) {
        if (!obj) {
            continue;
        }
        SelectionTransform::drawHandlesForObject(
            painter, obj->boundingRect(), m_zoomLevel,
            SelectionPolicy::supportsRotationHandle(obj),
            SelectionPolicy::usesExternalRotation(obj),
            obj->rotationDegrees());
    }

    painter.restore();
}

void DrawingCanvas::renderTool(QPainter &painter)
{
    if ((m_isBrushing || m_isBlurring) && !m_brushStroke.empty()) {
        painter.save();
        setupWorldTransform(painter);
        static const std::vector<float> kNoParticles;
        const bool useParticles =
            m_isBrushing && !m_brushParticleScale.empty() &&
            m_brushParticleScale.size() == m_brushStroke.size() &&
            m_brushParticleAlpha.size() == m_brushStroke.size();
        CanvasPaintPipeline::renderBrushStrokePreview(
            painter, m_brushStroke,
            useParticles ? m_brushParticleScale : kNoParticles,
            useParticles ? m_brushParticleAlpha : kNoParticles, m_brushSize,
            m_brushHardness, m_defaultDrawingColor, m_isBlurring);
        painter.restore();
        return;
    }

    const bool angleBaselineVisible =
        m_currentTool == DrawingTool::AngleLine && m_angleLineStage >= 1;
    if (!m_isDrawing && !angleBaselineVisible) {
        return;
    }

    painter.save();
    setupWorldTransform(painter);

    if (angleBaselineVisible) {
        CanvasPaintPipeline::renderAngleBaseline(
            painter, m_angleBaselineStart, m_angleBaselineEnd, m_zoomLevel);
    }

    if (m_isDrawing && m_currentPrimitive) {
        CanvasPaintPipeline::renderDrawingPrimitivePreview(
            painter, m_currentPrimitive.get(), m_defaultDrawingColor);
    }

    painter.restore();
}

void DrawingCanvas::renderCursorPreview(QPainter &painter)
{
    if (!m_showCursorPreview) {
        return;
    }
    const float radius =
        ToolCursor::previewRadiusWorld(m_currentTool, m_brushSize, m_eraserSize);
    if (radius <= 0.0f) {
        return;
    }

    painter.save();
    painter.resetTransform();
    OverlayRenderer::drawCursorPreviewRing(
        painter, worldToScreen(m_cursorPreviewPos), radius * m_zoomLevel);
    painter.restore();
}

void DrawingCanvas::renderSnapIndicator(QPainter &painter)
{
    if (!m_snapIndicatorActive) {
        return;
    }

    painter.save();
    painter.resetTransform();
    OverlayRenderer::drawSnapIndicator(painter,
                                       worldToScreen(m_snapIndicatorPos));
    painter.restore();
}

void DrawingCanvas::renderLassoOverlay(QPainter &painter)
{
    if (!m_selectionManager->isSelecting() ||
        m_selectionMode != SelectionMode::Lasso || m_lassoPoints.size() < 2) {
        return;
    }

    const QPolygonF screenPoly = OverlayRenderer::lassoScreenPolygon(
        m_lassoPoints, [this](const QVector2D &w) { return worldToScreen(w); });
    if (screenPoly.size() < 2) {
        return;
    }

    painter.save();
    painter.resetTransform();
    OverlayRenderer::drawLassoPolygon(painter, screenPoly);
    painter.restore();
}

void DrawingCanvas::renderDimensionTexts(QPainter &painter)
{
    painter.save();
    painter.resetTransform();
    QFont font(QStringLiteral("Arial"), 10);
    painter.setFont(font);
    painter.setPen(QColor(255, 200, 0));
    TextRenderer::renderAllDimensions(
        painter, m_layerManager, m_primitives, m_currentPrimitive.get(),
        [this](const QVector2D &w) { return worldToScreen(w); });
    painter.restore();
}

void DrawingCanvas::renderTextPrimitives(QPainter &painter)
{
    painter.save();
    painter.resetTransform();
    painter.setRenderHint(QPainter::TextAntialiasing);
    TextRenderer::renderAllTexts(
        painter, m_layerManager, m_primitives, m_zoomLevel,
        [this](const QVector2D &w) { return worldToScreen(w); },
        [this](const QUuid &id) {
            return dynamic_cast<SplinePrimitive *>(findPrimitiveById(id));
        });
    painter.restore();
}

void DrawingCanvas::renderDimensionText(QPainter *painter,
                                        DimensionPrimitive *dimension)
{
    TextRenderer::drawDimensionLabel(
        painter, dimension,
        [this](const QVector2D &w) { return worldToScreen(w); });
}

void DrawingCanvas::renderRulerTexts(QPainter &painter)
{
    if (!m_rulersVisible) {
        return;
    }
    RulerRenderer::renderTexts(
        painter, width(), height(), m_unitsConverter.units(),
        [this](const QVector2D &world) { return worldToScreen(world); });
}

void DrawingCanvas::renderControlPoint(QPainter &painter, const QVector2D &point,
                                       bool highlighted)
{
    OverlayRenderer::drawControlPoint(painter, point, m_zoomLevel, highlighted);
}

void DrawingCanvas::renderPaper(QPainter &painter)
{
    if (!m_renderer) {
        return;
    }
    painter.save();
    setupWorldTransform(painter);
    m_paper.render(m_renderer.get(), m_unitsConverter.pixelsPerMM());
    painter.restore();
}

void DrawingCanvas::renderAlignmentGuides(QPainter &painter)
{
    if (m_alignmentGuides.empty()) {
        return;
    }

    const auto lines = OverlayRenderer::guideLinesFrom(m_alignmentGuides);
    if (lines.empty()) {
        return;
    }

    painter.save();
    setupWorldTransform(painter);
    OverlayRenderer::drawAlignmentGuides(painter, lines);
    painter.restore();
}

void DrawingCanvas::renderTextOnSpline(const TextPrimitive *textPrim,
                                       QPainter &painter)
{
    TextRenderer::drawTextPrimitive(
        painter, const_cast<TextPrimitive *>(textPrim), m_zoomLevel,
        [this](const QVector2D &w) { return worldToScreen(w); },
        [this](const QUuid &id) {
            return dynamic_cast<SplinePrimitive *>(findPrimitiveById(id));
        });
}

void DrawingCanvas::renderFormattedText(QPainter *painter,
                                        const TextPrimitive *textPrim,
                                        const QPoint &pos, const QFont &font,
                                        bool showBox)
{
    TextRenderer::drawFormatted(painter, textPrim, pos, font, m_zoomLevel,
                                showBox);
}
