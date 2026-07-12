#pragma once

#include <QObject>
#include <QVector2D>
#include <QFont>
#include <QColor>
#include <QRectF>
#include <QTextEdit>
#include <QString>
#include <QJsonObject>
#include <memory>

class TextPrimitive;
class DrawingCanvas;
class Command;

/**
 * @brief Classic text tool similar to Photoshop/CorelDRAW text tools
 *
 * Features:
 * - Click to create new text
 * - Double-click to edit existing text
 * - Drag to move selected text
 * - Resize handles for scaling
 * - Simple properties panel integration
 */
class ClassicTextTool : public QObject
{
    Q_OBJECT

public:
    explicit ClassicTextTool(QObject* parent = nullptr);
    virtual ~ClassicTextTool();

    // Tool activation/deactivation
    void activate();
    void deactivate();
    bool isActive() const { return m_active; }
    bool isEditing() const { return m_isEditing; }

    // Canvas interaction
    void setCanvas(DrawingCanvas* canvas);
    DrawingCanvas* canvas() const { return m_canvas; }

    // Mouse event handlers
    void mousePress(const QVector2D& worldPos);
    void mouseMove(const QVector2D& worldPos);
    void mouseRelease(const QVector2D& worldPos);
    void mouseDoubleClick(const QVector2D& worldPos);

    // Text creation and editing
    TextPrimitive* createText(const QVector2D& position, const QString& text);
    void startEditing(TextPrimitive* textPrimitive);
    void finishEditing();
    void cancelEditing();

    // Current properties (applied to new text)
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString& family);

    int fontSize() const { return m_fontSize; }
    void setFontSize(int size);

    bool isBold() const { return m_bold; }
    void setBold(bool bold);

    bool isItalic() const { return m_italic; }
    void setItalic(bool italic);

    bool isUnderline() const { return m_underline; }
    void setUnderline(bool underline);

    enum class BaselineShift {
        Normal,
        Subscript,
        Superscript
    };

    BaselineShift baselineShift() const { return m_baselineShift; }
    void setBaselineShift(BaselineShift shift);
    bool isSubscript() const { return m_baselineShift == BaselineShift::Subscript; }
    bool isSuperscript() const { return m_baselineShift == BaselineShift::Superscript; }
    void setSubscript(bool enabled);
    void setSuperscript(bool enabled);

    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor& color);

    // Shadow properties
    bool shadowEnabled() const { return m_shadowEnabled; }
    void setShadowEnabled(bool enabled);

    QColor shadowColor() const { return m_shadowColor; }
    void setShadowColor(const QColor& color);

    double shadowOffsetX() const { return m_shadowOffsetX; }
    void setShadowOffsetX(double offset);

    double shadowOffsetY() const { return m_shadowOffsetY; }
    void setShadowOffsetY(double offset);

    double shadowBlur() const { return m_shadowBlur; }
    void setShadowBlur(double blur);

    // Stroke properties
    bool strokeEnabled() const { return m_strokeEnabled; }
    void setStrokeEnabled(bool enabled);

    QColor strokeColor() const { return m_strokeColor; }
    void setStrokeColor(const QColor& color);

    double strokeWidth() const { return m_strokeWidth; }
    void setStrokeWidth(double width);

    // Gradient properties
    bool gradientEnabled() const { return m_gradientEnabled; }
    void setGradientEnabled(bool enabled);

    QColor gradientStartColor() const { return m_gradientStartColor; }
    void setGradientStartColor(const QColor& color);

    QColor gradientEndColor() const { return m_gradientEndColor; }
    void setGradientEndColor(const QColor& color);

    double gradientAngle() const { return m_gradientAngle; }
    void setGradientAngle(double angle);

    // Text alignment
    enum class Alignment { Left, Center, Right, Justify };

    Alignment alignment() const { return m_alignment; }
    void setAlignment(Alignment align);

    // Selection management
    TextPrimitive* selectedText() const { return m_selectedText; }
    void selectText(TextPrimitive* text);
    void deselectText();

signals:
    // Tool events
    void textCreated(TextPrimitive* text);
    void textSelected(TextPrimitive* text);
    void textDeselected();
    void editingStarted(TextPrimitive* text);
    void editingFinished(TextPrimitive* text);

    // Property changes
    void propertyChanged();

    // Undo support
    void commandCreated(Command* command);

public slots:
    // Quick formatting
    void alignLeft() { setAlignment(Alignment::Left); }
    void alignCenter() { setAlignment(Alignment::Center); }
    void alignRight() { setAlignment(Alignment::Right); }
    void alignJustify() { setAlignment(Alignment::Justify); }

    void toggleBold() { setBold(!m_bold); }
    void toggleItalic() { setItalic(!m_italic); }

    void increaseFontSize() { setFontSize(m_fontSize + 2); }
    void decreaseFontSize() { setFontSize(m_fontSize - 2); }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onTextEditingFinished();

private:
    // Helper methods
    TextPrimitive* findTextAtPosition(const QVector2D& worldPos);
    int findHandleAtPosition(const QVector2D& worldPos, TextPrimitive* text);
    void setupTextEditor(const QVector2D& position, const QString& initialText = "");
    void cleanupTextEditor();
    void applyAlignmentToEditor();
    void applyPropertiesToText(TextPrimitive* text);
    void updatePropertiesFromText(TextPrimitive* text);
    void updateCursor();
    void updateCursorForPosition(const QVector2D& worldPos);
    void applyFontScaleResize(const QVector2D& worldPos);
    static void handleFraction(int handleIndex, float& fx, float& fy);
    void applyCurrentPropertiesToSelectedText();

    // Canvas reference
    DrawingCanvas* m_canvas;

    // Tool state
    bool m_active;
    bool m_isEditing;
    bool m_isDragging;
    bool m_isResizing;
    bool m_isNewText;

    // Current operation
    TextPrimitive* m_selectedText;
    TextPrimitive* m_editingText;
    int m_resizeHandle; // -1 = none, 0-7 = handle index

    // Undo state
    QJsonObject m_editingOldState;

    // Reentrancy guard for commit/cancel (prevents double-commit / double-delete)
    bool m_finishing;

    // Resize state for font-proportional scaling
    QRectF m_resizeStartBounds;
    float m_resizeStartFontSize;
    QVector2D m_resizeAnchorWorld;

    // Mouse tracking
    QVector2D m_lastMousePos;
    QVector2D m_dragStartPos;

    // Text editor
    QTextEdit* m_textEditor;
    QVector2D m_editorPosition;

    // Current text properties
    QString m_fontFamily;
    int m_fontSize;
    bool m_bold;
    bool m_italic;
    bool m_underline;
    BaselineShift m_baselineShift;
    QColor m_textColor;
    Alignment m_alignment;

    // Shadow properties
    bool m_shadowEnabled;
    QColor m_shadowColor;
    double m_shadowOffsetX;
    double m_shadowOffsetY;
    double m_shadowBlur;

    // Stroke properties
    bool m_strokeEnabled;
    QColor m_strokeColor;
    double m_strokeWidth;

    // Gradient properties
    bool m_gradientEnabled;
    QColor m_gradientStartColor;
    QColor m_gradientEndColor;
    double m_gradientAngle;

    // Default values
    static constexpr int DEFAULT_FONT_SIZE = 20;
};
