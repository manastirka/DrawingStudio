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

// ClassicTextTool core + property setters (refactor E19).

ClassicTextTool::ClassicTextTool(QObject* parent)
    : QObject(parent)
    , m_canvas(nullptr)
    , m_active(false)
    , m_isEditing(false)
    , m_isDragging(false)
    , m_isResizing(false)
    , m_isNewText(false)
    , m_selectedText(nullptr)
    , m_editingText(nullptr)
    , m_resizeHandle(-1)
    , m_finishing(false)
    , m_resizeStartFontSize(0.0f)
    , m_textEditor(nullptr)
    , m_fontFamily("Arial")
    , m_fontSize(DEFAULT_FONT_SIZE)
    , m_bold(false)
    , m_italic(false)
    , m_underline(false)
    , m_baselineShift(BaselineShift::Normal)
    , m_textColor(Qt::black)
    , m_alignment(Alignment::Left)
    , m_shadowEnabled(false)
    , m_shadowColor(QColor(0, 0, 0, 128))
    , m_shadowOffsetX(0.0)
    , m_shadowOffsetY(0.0)
    , m_shadowBlur(0.0)
    , m_strokeEnabled(false)
    , m_strokeColor(Qt::black)
    , m_strokeWidth(2.0)
    , m_gradientEnabled(false)
    , m_gradientStartColor(Qt::white)
    , m_gradientEndColor(Qt::black)
    , m_gradientAngle(0.0)
{
}

ClassicTextTool::~ClassicTextTool()
{
    cleanupTextEditor();
}

// ============================================================================
// Tool Activation/Deactivation
// ============================================================================

void ClassicTextTool::activate()
{
    if (m_active) return;

    m_active = true;
    updateCursor();
}

void ClassicTextTool::deactivate()
{
    if (!m_active) return;

    // Finish any ongoing editing
    if (m_isEditing) {
        finishEditing();
    }

    deselectText();
    m_active = false;
}

void ClassicTextTool::setCanvas(DrawingCanvas* canvas)
{
    m_canvas = canvas;
}

void ClassicTextTool::setFontFamily(const QString& family)
{
    if (family == m_fontFamily)
        return;

    m_fontFamily = family;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setFontSize(int size)
{
    int clamped = qBound(8, size, 200);
    if (clamped == m_fontSize)
        return;

    m_fontSize = clamped;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setBold(bool bold)
{
    if (m_bold == bold)
        return;

    m_bold = bold;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setItalic(bool italic)
{
    if (m_italic == italic)
        return;

    m_italic = italic;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setUnderline(bool underline)
{
    if (m_underline == underline)
        return;

    m_underline = underline;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setBaselineShift(BaselineShift shift)
{
    if (m_baselineShift == shift)
        return;

    m_baselineShift = shift;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setSubscript(bool enabled)
{
    if (enabled) {
        setBaselineShift(BaselineShift::Subscript);
    } else if (m_baselineShift == BaselineShift::Subscript) {
        setBaselineShift(BaselineShift::Normal);
    }
}

void ClassicTextTool::setSuperscript(bool enabled)
{
    if (enabled) {
        setBaselineShift(BaselineShift::Superscript);
    } else if (m_baselineShift == BaselineShift::Superscript) {
        setBaselineShift(BaselineShift::Normal);
    }
}

void ClassicTextTool::setTextColor(const QColor& color)
{
    if (m_textColor == color)
        return;

    m_textColor = color;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setAlignment(Alignment align)
{
    if (m_alignment == align)
        return;

    m_alignment = align;
    applyCurrentPropertiesToSelectedText();
    applyAlignmentToEditor();
    emit propertyChanged();
}

void ClassicTextTool::setShadowEnabled(bool enabled)
{
    if (m_shadowEnabled == enabled)
        return;

    m_shadowEnabled = enabled;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setShadowColor(const QColor& color)
{
    if (m_shadowColor == color)
        return;

    m_shadowColor = color;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setShadowOffsetX(double offset)
{
    if (qFuzzyCompare(m_shadowOffsetX, offset))
        return;

    m_shadowOffsetX = offset;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setShadowOffsetY(double offset)
{
    if (qFuzzyCompare(m_shadowOffsetY, offset))
        return;

    m_shadowOffsetY = offset;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setShadowBlur(double blur)
{
    if (qFuzzyCompare(m_shadowBlur, blur))
        return;

    m_shadowBlur = blur;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setStrokeEnabled(bool enabled)
{
    if (m_strokeEnabled == enabled)
        return;

    m_strokeEnabled = enabled;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setStrokeColor(const QColor& color)
{
    if (m_strokeColor == color)
        return;

    m_strokeColor = color;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setStrokeWidth(double width)
{
    if (qFuzzyCompare(m_strokeWidth, width))
        return;

    m_strokeWidth = width;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setGradientEnabled(bool enabled)
{
    if (m_gradientEnabled == enabled)
        return;

    m_gradientEnabled = enabled;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setGradientStartColor(const QColor& color)
{
    if (m_gradientStartColor == color)
        return;

    m_gradientStartColor = color;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setGradientEndColor(const QColor& color)
{
    if (m_gradientEndColor == color)
        return;

    m_gradientEndColor = color;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

void ClassicTextTool::setGradientAngle(double angle)
{
    if (qFuzzyCompare(m_gradientAngle, angle))
        return;

    m_gradientAngle = angle;
    applyCurrentPropertiesToSelectedText();
    emit propertyChanged();
}

// ============================================================================
// Mouse Event Handlers
// ============================================================================

TextPrimitive* ClassicTextTool::createText(const QVector2D& position, const QString& text)
{
    TextPrimitive* textPrimitive = new TextPrimitive(position, text);

    // Don't set textBoxWidth/Height — let boundingRect auto-size from text content.
    // The user can explicitly resize later to set a fixed box with word wrap.
    textPrimitive->setTextBoxWidth(0);
    textPrimitive->setTextBoxHeight(0);

    // Apply current tool properties
    applyPropertiesToText(textPrimitive);

    emit textCreated(textPrimitive);
    return textPrimitive;
}

TextPrimitive* ClassicTextTool::findTextAtPosition(const QVector2D& worldPos)
{
    if (!m_canvas) return nullptr;

    auto layerManager = m_canvas->layerManager();

    auto containsPoint = [&](DrawingPrimitive* primitive) -> TextPrimitive* {
        if (!primitive || primitive->type() != PrimitiveType::Text) {
            return nullptr;
        }

        auto* text = static_cast<TextPrimitive*>(primitive);
        if (!text->isVisible()) {
            return nullptr;
        }

        return text->containsPoint(worldPos) ? text : nullptr;
    };

    if (layerManager) {
        auto primitives = layerManager->getAllPrimitives();
        for (auto it = primitives.rbegin(); it != primitives.rend(); ++it) {
            if (auto* text = containsPoint(*it)) {
                return text;
            }
        }
    } else {
        const auto& primitives = m_canvas->primitives();
        for (auto it = primitives.rbegin(); it != primitives.rend(); ++it) {
            if (auto* text = containsPoint(it->get())) {
                return text;
            }
        }
    }

    return nullptr;
}

void ClassicTextTool::applyCurrentPropertiesToSelectedText()
{
    if (!m_selectedText)
        return;

    applyPropertiesToText(m_selectedText);

    if (m_canvas) {
        m_canvas->update();
    }
}

