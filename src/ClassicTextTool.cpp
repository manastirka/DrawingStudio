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
#include <QTextOption>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QtGlobal>
#include <cmath>

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

void ClassicTextTool::mousePress(const QVector2D& worldPos)
{
    if (!m_active || !m_canvas) return;

    m_lastMousePos = worldPos;
    m_dragStartPos = worldPos;

    // Check if we're clicking on a resize handle of selected text
    if (m_selectedText) {
        int handleIndex = findHandleAtPosition(worldPos, m_selectedText);
        if (handleIndex >= 0) {
            m_isResizing = true;
            m_resizeHandle = handleIndex;

            // Capture start state so we can scale font proportionally while
            // keeping the opposite (anchor) handle fixed in world space.
            m_resizeStartBounds = m_selectedText->boundingRect();
            m_resizeStartFontSize = m_selectedText->fontSize();

            float fx, fy;
            handleFraction(handleIndex, fx, fy);
            const float afx = 1.0f - fx;
            const float afy = 1.0f - fy;
            m_resizeAnchorWorld = QVector2D(
                m_resizeStartBounds.left() + afx * static_cast<float>(m_resizeStartBounds.width()),
                m_resizeStartBounds.top() + afy * static_cast<float>(m_resizeStartBounds.height()));
            return;
        }
    }

    // Check if clicking on existing text
    TextPrimitive* clickedText = findTextAtPosition(worldPos);

    if (clickedText) {
        // Select the text and prepare for dragging
        if (m_isEditing) {
            finishEditing();
        }
        selectText(clickedText);
        m_isDragging = true;
    } else {
        // Click on empty space: show editor for new text
        if (m_isEditing) {
            finishEditing();
        }

        deselectText();
        m_isNewText = true;
        m_editingText = nullptr;
        m_isEditing = true;
        setupTextEditor(worldPos, "");
    }
}

void ClassicTextTool::mouseMove(const QVector2D& worldPos)
{
    if (!m_active || !m_canvas) return;

    if (m_isResizing && m_selectedText) {
        // Scale font size proportionally from the dragged handle
        applyFontScaleResize(worldPos);
        m_canvas->update();
    } else if (m_isDragging && m_selectedText) {
        // Move selected text
        QVector2D delta = worldPos - m_lastMousePos;
        m_selectedText->translate(delta);
        m_canvas->update();
    }

    m_lastMousePos = worldPos;

    // Update the cursor based on what is under the mouse (only when idle)
    if (!m_isDragging && !m_isResizing) {
        updateCursorForPosition(worldPos);
    }
}

void ClassicTextTool::mouseRelease(const QVector2D& worldPos)
{
    if (!m_active || !m_canvas) return;

    bool wasResizing = m_isResizing;

    m_isDragging = false;
    m_isResizing = false;
    m_resizeHandle = -1;
    m_lastMousePos = worldPos;

    if (wasResizing && m_selectedText) {
        updatePropertiesFromText(m_selectedText);
    }
}

void ClassicTextTool::mouseDoubleClick(const QVector2D& worldPos)
{
    if (!m_active || !m_canvas) return;

    // Find text at position and start editing
    TextPrimitive* clickedText = findTextAtPosition(worldPos);

    if (clickedText) {
        if (m_isEditing) {
            finishEditing();
        }
        selectText(clickedText);
        startEditing(clickedText);
    }
}

// ============================================================================
// Text Creation and Editing
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

void ClassicTextTool::startEditing(TextPrimitive* textPrimitive)
{
    if (!textPrimitive || m_isEditing) return;

    m_editingText = textPrimitive;
    m_isEditing = true;
    m_isNewText = false;

    // Capture pre-edit state for undo (before hiding, so visibility is restored
    // correctly on cancel).
    m_editingOldState = textPrimitive->toJson();

    // Update properties from the text being edited
    updatePropertiesFromText(textPrimitive);

    // Hide the original while editing so it doesn't render underneath the
    // WYSIWYG overlay (restored on commit/cancel).
    textPrimitive->setVisible(false);

    // Setup text editor
    setupTextEditor(textPrimitive->position(), textPrimitive->text());

    if (m_canvas) {
        m_canvas->update();
    }

    emit editingStarted(textPrimitive);
}

void ClassicTextTool::finishEditing()
{
    // Guard against reentrancy: committing changes focus (FocusOut) and creates
    // primitives, either of which could re-enter this method and double-commit
    // or double-delete the editor.
    if (!m_isEditing || !m_textEditor || m_finishing) return;

    m_finishing = true;

    // Detach the editor up-front so no further events (FocusOut / textChanged)
    // route back into the tool while we commit.
    QTextEdit* editor = m_textEditor;
    m_textEditor = nullptr;
    editor->removeEventFilter(this);
    editor->blockSignals(true);

    const QString newText = editor->toPlainText();

    if (m_isNewText) {
        // Creating new text (empty -> nothing is created)
        if (!newText.isEmpty()) {
            TextPrimitive* newPrim = createText(m_editorPosition, newText);
            selectText(newPrim);
        }
    } else if (m_editingText) {
        // Restore visibility hidden during editing before committing.
        m_editingText->setVisible(true);

        if (!newText.isEmpty()) {
            m_editingText->setText(newText);

            // Create undo command
            QJsonObject newState = m_editingText->toJson();
            auto cmd = new EditTextCommand(m_editingText, "Edit Text");
            cmd->storeOldState(m_editingOldState);
            cmd->storeNewState(newState);
            emit commandCreated(cmd);

            emit editingFinished(m_editingText);
        } else {
            // Empty text: revert to the pre-edit state.
            m_editingText->fromJson(m_editingOldState);
        }
    }

    editor->deleteLater();

    m_isEditing = false;
    m_isNewText = false;
    m_editingText = nullptr;
    m_editingOldState = QJsonObject();
    m_finishing = false;

    if (m_canvas) {
        m_canvas->update();
    }
}

void ClassicTextTool::cancelEditing()
{
    if (!m_isEditing || m_finishing) return;

    m_finishing = true;

    // Detach and destroy the editor safely.
    if (m_textEditor) {
        QTextEdit* editor = m_textEditor;
        m_textEditor = nullptr;
        editor->removeEventFilter(this);
        editor->blockSignals(true);
        editor->deleteLater();
    }

    // If editing existing text, revert to old state (also restores visibility).
    if (!m_isNewText && m_editingText && !m_editingOldState.isEmpty()) {
        m_editingText->fromJson(m_editingOldState);
    }
    // If new text, nothing to revert (no primitive was created).

    m_isEditing = false;
    m_isNewText = false;
    m_editingText = nullptr;
    m_editingOldState = QJsonObject();
    m_finishing = false;

    if (m_canvas) {
        m_canvas->update();
    }
}

// ============================================================================
// Event Filter (for QTextEdit key handling)
// ============================================================================

bool ClassicTextTool::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_textEditor) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

            // Escape -> commit; if the editor is empty this effectively cancels
            // (no primitive created / revert to old state).
            if (keyEvent->key() == Qt::Key_Escape) {
                if (m_textEditor->toPlainText().isEmpty()) {
                    cancelEditing();
                } else {
                    finishEditing();
                }
                return true;
            }

            // Ctrl+Enter / Cmd+Enter -> commit
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                && (keyEvent->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))) {
                finishEditing();
                return true;
            }

            // All other keys pass through (Enter = newline in QTextEdit)
        }

        if (event->type() == QEvent::FocusOut) {
            // Clicking outside the editor commits the text (like Photoshop)
            finishEditing();
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

// ============================================================================
// Selection Management
// ============================================================================

void ClassicTextTool::selectText(TextPrimitive* text)
{
    if (m_selectedText == text) return;

    deselectText();

    m_selectedText = text;
    if (m_selectedText) {
        m_selectedText->setSelected(true);
        if (m_canvas) {
            m_canvas->addToSelection(m_selectedText);
        }
        updatePropertiesFromText(m_selectedText);
        emit textSelected(text);
    }

    if (m_canvas) {
        m_canvas->update();
    }
}

void ClassicTextTool::deselectText()
{
    if (m_selectedText) {
        m_selectedText->setSelected(false);
        m_selectedText = nullptr;
        if (m_canvas) {
            m_canvas->clearSelection();
        }
        emit textDeselected();

        if (m_canvas) {
            m_canvas->update();
        }
    }
}

// ============================================================================
// Private Slots
// ============================================================================

void ClassicTextTool::onTextEditingFinished()
{
    finishEditing();
}

// ============================================================================
// Helper Methods
// ============================================================================

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

int ClassicTextTool::findHandleAtPosition(const QVector2D& worldPos, TextPrimitive* text)
{
    if (!text) return -1;

    int handleIndex = -1;
    if (text->isPointOnHandle(worldPos, &handleIndex)) {
        return handleIndex;
    }

    return -1;
}

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

void ClassicTextTool::applyCurrentPropertiesToSelectedText()
{
    if (!m_selectedText)
        return;

    applyPropertiesToText(m_selectedText);

    if (m_canvas) {
        m_canvas->update();
    }
}
