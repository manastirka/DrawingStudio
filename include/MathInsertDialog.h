#pragma once

#include "MathFormulaRenderer.h"
#include "MathGraphRenderer.h"

#include <QDialog>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QColor>
#include <QFont>
#include <QTextBrowser>
#include <QScrollArea>

/**
 * Insert math formula (rendered) or function graph onto the canvas.
 * Includes a tutorial tab for how to enter expressions.
 */
class MathInsertDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Formula, Graph };
    enum class StartTab { Formula = 0, Graph = 1, Tutorial = 2 };

    struct Result {
        Mode mode = Mode::Formula;
        QImage image;
        QString expression; // source text / f(x)
        float canvasWidth = 400.0f;
    };

    explicit MathInsertDialog(StartTab startTab = StartTab::Formula,
                              QWidget *parent = nullptr);

    Result result() const { return m_result; }
    void setStartTab(StartTab tab);

private slots:
    void refreshPreview();
    void acceptInsert();
    void pickCurveColor();
    void pickFormulaColor();

private:
    void buildFormulaTab(QWidget *page);
    void buildGraphTab(QWidget *page);
    void buildTutorialTab(QWidget *page);
    void addExampleChip(QLayout *layout, const QString &label, const QString &payload,
                        Mode mode);
    void updateOkButton();

    Result m_result;
    QTabWidget *m_tabs = nullptr;
    QPushButton *m_okButton = nullptr;

    // Formula
    QPlainTextEdit *m_formulaEdit = nullptr;
    QLabel *m_formulaPreview = nullptr;
    QLabel *m_formulaError = nullptr;
    QSpinBox *m_formulaSize = nullptr;
    QCheckBox *m_formulaBg = nullptr;
    QColor m_formulaColor = QColor(20, 20, 24);

    // Graph
    QLineEdit *m_graphExpr = nullptr;
    QLabel *m_graphPreview = nullptr;
    QLabel *m_graphError = nullptr;
    QDoubleSpinBox *m_xMin = nullptr;
    QDoubleSpinBox *m_xMax = nullptr;
    QDoubleSpinBox *m_yMin = nullptr;
    QDoubleSpinBox *m_yMax = nullptr;
    QCheckBox *m_autoY = nullptr;
    QSpinBox *m_samples = nullptr;
    QSpinBox *m_graphW = nullptr;
    QSpinBox *m_graphH = nullptr;
    QCheckBox *m_showAxes = nullptr;
    QCheckBox *m_showGrid = nullptr;
    QCheckBox *m_showTicks = nullptr;
    QCheckBox *m_showLabels = nullptr;
    QCheckBox *m_showTitle = nullptr;
    QDoubleSpinBox *m_curveWidth = nullptr;
    QColor m_curveColor = QColor(30, 110, 220);
    QPushButton *m_curveColorBtn = nullptr;
    QPushButton *m_formulaColorBtn = nullptr;

    QImage m_previewFormula;
    QImage m_previewGraph;
};
