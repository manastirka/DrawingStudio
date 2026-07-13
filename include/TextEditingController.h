#pragma once

#include <QObject>
#include <QString>
#include <functional>

class AdvancedTextEditor;
class ClassicTextTool;
class CommandManager;
class DrawingCanvas;
class TextPrimitive;
class QWidget;

/**
 * Text formatting dialogs, alignment, advanced editor, math/physics insert.
 * Extracted from MainWindow (refactor A9).
 */
class TextEditingController : public QObject {
    Q_OBJECT
public:
    struct Host {
        DrawingCanvas *canvas = nullptr;
        CommandManager *commandManager = nullptr;
        ClassicTextTool *classicTextTool = nullptr;
        QWidget *dialogParent = nullptr;
        std::function<void(const QString &)> setStatusText;
        std::function<void()> syncModifiedFlag;
        std::function<void(const QString &, int)> showStatusMessage;
    };

    explicit TextEditingController(QObject *parent = nullptr);
    void setHost(Host host);
    const Host &host() const { return m_host; }

    void commitTextPropertyEdits(const QString &description,
                                 const std::function<void(TextPrimitive *)> &mutate);

    void setTextAlignLeft();
    void setTextAlignCenter();
    void setTextAlignRight();
    void setTextAlignJustify();

    void showFontFamilyDialog();
    void showLetterSpacingDialog();
    void showLineSpacingDialog();
    void showTrackingDialog();
    void showShadowDialog();
    void showStrokeDialog();
    void showGradientDialog();
    void showTextBoxDialog();
    void showAdvancedTextEditor();
    void addAutoShadow();

    void insertMathFormula();
    void insertMathGraph();
    void showMathTutorial();
    void insertPhysicsSolver();

private:
    void setStatus(const QString &msg);
    QWidget *dialogParent() const;

    Host m_host;
    AdvancedTextEditor *m_advancedTextEditor = nullptr;
};
