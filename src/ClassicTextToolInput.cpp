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

// Mouse / editing session (refactor E19).

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

