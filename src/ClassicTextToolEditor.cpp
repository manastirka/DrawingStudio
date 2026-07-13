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

// Overlay editor + property sync (refactor E19).

void ClassicTextTool::setupTextEditor(const QVector2D& position, const QString& initialText)
{
    if (!m_canvas) return;

    cleanupTextEditor();

    // Convert world position to screen coordinates
    QPoint screenPos = m_canvas->worldToScreen(position);
    float zoom = m_canvas->zoomLevel();

    m_textEditor = new QTextEdit(m_canvas);
    m_textEditor->setPlainText(initialText);

    // Size editor: use existing text box dimensions when editing, otherwise default
    int editorWidth, editorHeight;
    if (m_editingText && m_editingText->textBoxWidth() > 0 && m_editingText->textBoxHeight() > 0) {
        editorWidth = qMax(120, static_cast<int>(m_editingText->textBoxWidth() * zoom));
        editorHeight = qMax(40, static_cast<int>(m_editingText->textBoxHeight() * zoom));
    } else if (m_editingText) {
        // Estimate size from existing text content
        QFont font(m_fontFamily, qMax(8, static_cast<int>(m_fontSize * zoom)));
        font.setBold(m_bold);
        font.setItalic(m_italic);
        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(initialText.isEmpty() ? "Placeholder" : initialText);
        int textHeight = fm.height() * qMax(1, initialText.count('\n') + 1);
        editorWidth = qMax(200, textWidth + 20);
        editorHeight = qMax(60, textHeight + 20);
    } else {
        editorWidth = qMax(200, static_cast<int>(150.0f * zoom));
        editorHeight = qMax(60, static_cast<int>(80.0f * zoom));
    }

    // Clamp to canvas bounds
    QRect canvasRect = m_canvas->rect();
    if (screenPos.x() + editorWidth > canvasRect.width()) {
        editorWidth = qMax(120, canvasRect.width() - screenPos.x() - 4);
    }
    if (screenPos.y() + editorHeight > canvasRect.height()) {
        editorHeight = qMax(40, canvasRect.height() - screenPos.y() - 4);
    }

    m_textEditor->setGeometry(screenPos.x(), screenPos.y(), editorWidth, editorHeight);

    // Apply current font settings, scaled by zoom
    QFont font(m_fontFamily, qMax(8, static_cast<int>(m_fontSize * zoom)));
    font.setBold(m_bold);
    font.setItalic(m_italic);
    font.setUnderline(m_underline);
    m_textEditor->setFont(font);

    // Configure for multi-line text; wrap within the editor (text area) width
    const bool hasFixedBox = m_editingText
        && m_editingText->textBoxWidth() > 0
        && m_editingText->textBoxHeight() > 0;
    m_textEditor->setWordWrapMode(QTextOption::WordWrap);
    m_textEditor->setAcceptRichText(false);
    m_textEditor->setLineWrapMode(QTextEdit::WidgetWidth);
    applyAlignmentToEditor();

    // WYSIWYG: transparent background, no border — text renders inline on canvas
    QString styleSheet = QString(
        "QTextEdit {"
        "  color: %1;"
        "  background-color: transparent;"
        "  border: 1px dashed rgba(100, 149, 237, 120);"
        "  padding: 0px;"
        "  margin: 0px;"
        "}"
    ).arg(m_textColor.name());
    m_textEditor->setStyleSheet(styleSheet);
    m_textEditor->viewport()->setAutoFillBackground(false);

    // Auto-resize editor as user types — keep fixed text-box width when present
    // so alignment stays scoped to the selected text area.
    connect(m_textEditor, &QTextEdit::textChanged, this, [this, hasFixedBox, editorWidth]() {
        if (!m_textEditor || !m_canvas) return;
        QTextDocument *doc = m_textEditor->document();
        float zoom = m_canvas->zoomLevel();
        int newW = editorWidth;
        int newH = qMax(static_cast<int>(30 * zoom),
                        static_cast<int>(doc->size().height()) + 10);
        if (!hasFixedBox) {
            int minW = qMax(100, static_cast<int>(60.0f * zoom));
            int docW = static_cast<int>(doc->idealWidth()) + 20;
            newW = qMax(minW, docW);
        }
        // Clamp to canvas
        QRect cr = m_canvas->rect();
        QPoint pos = m_textEditor->pos();
        newW = qMin(newW, cr.width() - pos.x() - 4);
        newH = qMin(newH, cr.height() - pos.y() - 4);
        m_textEditor->setFixedSize(newW, newH);
    });

    // Install event filter for key handling
    m_textEditor->installEventFilter(this);

    m_textEditor->setFocus();
    if (!initialText.isEmpty()) {
        m_textEditor->selectAll();
    }
    m_textEditor->show();

    m_editorPosition = position;
}

void ClassicTextTool::cleanupTextEditor()
{
    if (m_textEditor) {
        m_textEditor->removeEventFilter(this);
        m_textEditor->disconnect();
        m_textEditor->deleteLater();
        m_textEditor = nullptr;
    }
}

void ClassicTextTool::applyAlignmentToEditor()
{
    if (!m_textEditor) {
        return;
    }

    Qt::Alignment qtAlign = Qt::AlignLeft;
    switch (m_alignment) {
    case Alignment::Left:
        qtAlign = Qt::AlignLeft;
        break;
    case Alignment::Center:
        qtAlign = Qt::AlignHCenter;
        break;
    case Alignment::Right:
        qtAlign = Qt::AlignRight;
        break;
    case Alignment::Justify:
        qtAlign = Qt::AlignJustify;
        break;
    }

    QTextCursor cursor = m_textEditor->textCursor();
    cursor.select(QTextCursor::Document);
    QTextBlockFormat blockFormat = cursor.blockFormat();
    blockFormat.setAlignment(qtAlign);
    cursor.mergeBlockFormat(blockFormat);
    m_textEditor->setAlignment(qtAlign);
}

void ClassicTextTool::applyPropertiesToText(TextPrimitive* text)
{
    if (!text) return;

    text->setFontFamily(m_fontFamily);
    text->setFontSize(m_fontSize);
    text->setBold(m_bold);
    text->setItalic(m_italic);
    text->setUnderline(m_underline);
    TextPrimitive::BaselineShift baseline = TextPrimitive::BaselineShift::Normal;
    switch (m_baselineShift) {
        case BaselineShift::Normal: baseline = TextPrimitive::BaselineShift::Normal; break;
        case BaselineShift::Subscript: baseline = TextPrimitive::BaselineShift::Subscript; break;
        case BaselineShift::Superscript: baseline = TextPrimitive::BaselineShift::Superscript; break;
    }
    text->setBaselineShift(baseline);
    text->setColor(m_textColor);

    // Convert alignment
    TextPrimitive::TextAlignment textAlign;
    switch (m_alignment) {
        case Alignment::Left: textAlign = TextPrimitive::TextAlignment::Left; break;
        case Alignment::Center: textAlign = TextPrimitive::TextAlignment::Center; break;
        case Alignment::Right: textAlign = TextPrimitive::TextAlignment::Right; break;
        case Alignment::Justify: textAlign = TextPrimitive::TextAlignment::Justify; break;
    }
    text->setAlignment(textAlign);

    // Apply shadow properties
    text->setShadowEnabled(m_shadowEnabled);
    text->setShadowColor(m_shadowColor);
    text->setShadowOffsetX(m_shadowOffsetX);
    text->setShadowOffsetY(m_shadowOffsetY);
    text->setShadowBlur(m_shadowBlur);

    // Apply stroke properties
    text->setStrokeEnabled(m_strokeEnabled);
    text->setStrokeColor(m_strokeColor);
    text->setStrokeWidth(m_strokeWidth);

    // Apply gradient properties
    text->setGradientEnabled(m_gradientEnabled);
    text->setGradientStartColor(m_gradientStartColor);
    text->setGradientEndColor(m_gradientEndColor);
    text->setGradientAngle(m_gradientAngle);
}

void ClassicTextTool::updatePropertiesFromText(TextPrimitive* text)
{
    if (!text) return;

    m_fontFamily = text->fontFamily();
    m_fontSize = text->fontSize();
    m_bold = text->isBold();
    m_italic = text->isItalic();
    m_underline = text->isUnderline();
    m_textColor = text->color();

    // Convert alignment
    switch (text->alignment()) {
        case TextPrimitive::TextAlignment::Left: m_alignment = Alignment::Left; break;
        case TextPrimitive::TextAlignment::Center: m_alignment = Alignment::Center; break;
        case TextPrimitive::TextAlignment::Right: m_alignment = Alignment::Right; break;
        case TextPrimitive::TextAlignment::Justify: m_alignment = Alignment::Justify; break;
    }

    switch (text->baselineShift()) {
        case TextPrimitive::BaselineShift::Normal:
            m_baselineShift = BaselineShift::Normal;
            break;
        case TextPrimitive::BaselineShift::Subscript:
            m_baselineShift = BaselineShift::Subscript;
            break;
        case TextPrimitive::BaselineShift::Superscript:
            m_baselineShift = BaselineShift::Superscript;
            break;
    }

    // Update shadow properties
    m_shadowEnabled = text->shadowEnabled();
    m_shadowColor = text->shadowColor();
    m_shadowOffsetX = text->shadowOffsetX();
    m_shadowOffsetY = text->shadowOffsetY();
    m_shadowBlur = text->shadowBlur();

    // Update stroke properties
    m_strokeEnabled = text->strokeEnabled();
    m_strokeColor = text->strokeColor();
    m_strokeWidth = text->strokeWidth();

    // Update gradient properties
    m_gradientEnabled = text->gradientEnabled();
    m_gradientStartColor = text->gradientStartColor();
    m_gradientEndColor = text->gradientEndColor();
    m_gradientAngle = text->gradientAngle();

    emit propertyChanged();
}

