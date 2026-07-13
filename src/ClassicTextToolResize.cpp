#include "ClassicTextTool.h"
#include "DrawingPrimitive.h"
#include "DrawingCanvas.h"
#include "Commands.h"
#include "MainWindow.h"
#include "LayerManager.h"
#include <QFontMetrics>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QKeyEvent>
#include <QKeySequence>
#include <QTextOption>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QTextEdit>
#include <QtGlobal>
#include <cmath>

// Resize handles + cursor (refactor E19).

int ClassicTextTool::findHandleAtPosition(const QVector2D& worldPos, TextPrimitive* text)
{
    if (!text) return -1;

    int handleIndex = -1;
    if (text->isPointOnHandle(worldPos, &handleIndex)) {
        return handleIndex;
    }

    return -1;
}

void ClassicTextTool::updateCursor()
{
    if (!m_canvas) return;

    if (m_active) {
        // Default to text cursor
        m_canvas->setCursor(Qt::IBeamCursor);
    } else {
        m_canvas->setCursor(Qt::ArrowCursor);
    }
}

void ClassicTextTool::updateCursorForPosition(const QVector2D& worldPos)
{
    if (!m_canvas || !m_active) return;

    // Over a resize handle of the selected text -> resize cursor
    if (m_selectedText) {
        int handleIndex = findHandleAtPosition(worldPos, m_selectedText);
        if (handleIndex >= 0) {
            switch (handleIndex) {
                case 0: case 4: m_canvas->setCursor(Qt::SizeFDiagCursor); return;
                case 2: case 6: m_canvas->setCursor(Qt::SizeBDiagCursor); return;
                case 1: case 5: m_canvas->setCursor(Qt::SizeVerCursor); return;
                case 3: case 7: m_canvas->setCursor(Qt::SizeHorCursor); return;
                default: break;
            }
        }
    }

    // Over existing text -> move cursor
    if (findTextAtPosition(worldPos)) {
        m_canvas->setCursor(Qt::SizeAllCursor);
        return;
    }

    // Empty canvas -> text insertion cursor
    m_canvas->setCursor(Qt::IBeamCursor);
}

void ClassicTextTool::handleFraction(int handleIndex, float& fx, float& fy)
{
    // Handle layout as QRectF fractions:
    // 0 TL, 1 TC, 2 TR, 3 RC, 4 BR, 5 BC, 6 BL, 7 LC
    static const float fxs[8] = {0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f};
    static const float fys[8] = {0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.5f};
    if (handleIndex < 0 || handleIndex > 7) {
        fx = 0.5f;
        fy = 0.5f;
        return;
    }
    fx = fxs[handleIndex];
    fy = fys[handleIndex];
}

void ClassicTextTool::applyFontScaleResize(const QVector2D& worldPos)
{
    if (!m_selectedText) return;

    float fx, fy;
    handleFraction(m_resizeHandle, fx, fy);

    // Vector from the fixed anchor to the original dragged handle, and to the
    // current cursor position.
    const QVector2D origHandle(
        m_resizeStartBounds.left() + fx * static_cast<float>(m_resizeStartBounds.width()),
        m_resizeStartBounds.top() + fy * static_cast<float>(m_resizeStartBounds.height()));
    const QVector2D origVec = origHandle - m_resizeAnchorWorld;
    const QVector2D curVec = worldPos - m_resizeAnchorWorld;

    float ratio;
    const bool corner = (m_resizeHandle == 0 || m_resizeHandle == 2 ||
                         m_resizeHandle == 4 || m_resizeHandle == 6);
    if (corner) {
        // Project the cursor onto the original diagonal for a stable ratio.
        const float denom = QVector2D::dotProduct(origVec, origVec);
        if (denom < 1e-3f) return;
        ratio = QVector2D::dotProduct(curVec, origVec) / denom;
    } else if (m_resizeHandle == 3 || m_resizeHandle == 7) {
        // Left/right edges -> horizontal ratio
        if (std::abs(origVec.x()) < 1e-3f) return;
        ratio = curVec.x() / origVec.x();
    } else {
        // Top/bottom edges -> vertical ratio
        if (std::abs(origVec.y()) < 1e-3f) return;
        ratio = curVec.y() / origVec.y();
    }

    float newFontSize = m_resizeStartFontSize * ratio;
    newFontSize = qBound(8.0f, newFontSize, 400.0f);

    // Scale the font and let the bounding box auto-size to content.
    m_selectedText->setFontSize(newFontSize);
    m_selectedText->setTextBoxWidth(0);
    m_selectedText->setTextBoxHeight(0);

    // Reposition so the anchor handle stays fixed in world space.
    const float afx = 1.0f - fx;
    const float afy = 1.0f - fy;
    const QRectF nb = m_selectedText->boundingRect();
    const float newLeft = m_resizeAnchorWorld.x() - afx * static_cast<float>(nb.width());
    const float newTop  = m_resizeAnchorWorld.y() - afy * static_cast<float>(nb.height());
    // m_position corresponds to (left, bottom) in QRectF terms.
    m_selectedText->setPosition(QVector2D(newLeft, newTop + static_cast<float>(nb.height())));
}

