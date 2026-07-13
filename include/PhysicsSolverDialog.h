#pragma once

#include "PhysicsCatalog.h"

#include <QDialog>
#include <QHash>
#include <QImage>

class QListWidget;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QFormLayout;
class QWidget;
class QDoubleSpinBox;
class QSpinBox;
class QTabWidget;
class QGroupBox;

/**
 * Physics problem solver: point solutions + dependency graphs over a range.
 * Results insert as math formula and/or math graph on the canvas.
 */
class PhysicsSolverDialog : public QDialog
{
    Q_OBJECT

public:
    enum class InsertMode { Formula, Graph, Both };

    struct Result {
        QImage formulaImage;
        QImage graphImage;
        QString latex;
        QString summary;
        float canvasWidth = 420.0f;
        float graphCanvasWidth = 520.0f;
        InsertMode insertMode = InsertMode::Formula;
    };

    explicit PhysicsSolverDialog(QWidget *parent = nullptr);

    Result result() const { return m_result; }

private slots:
    void onFieldChanged(int index);
    void onEquationSelected();
    void onUnknownChanged(int index);
    void onIndepChanged(int index);
    void onModeTabChanged(int index);
    void solveNow();
    void plotDependency();
    void acceptInsert();

private:
    void rebuildEquationList();
    void rebuildVariableForm();
    void rebuildDependencyCombos();
    void updateFormulaPreview();
    void updateInsertButton();
    void suggestIndepRange();
    QHash<QString, double> collectKnownValues(QString *errorOut,
                                              const QString &skipA,
                                              const QString &skipB = {}) const;
    PhysicsVariable findVar(const QString &id) const;
    static QString rewriteIndepAsX(const QString &formula, const QString &indepId);
    static QString formatNumberLatex(double x);
    static QString formatUnitLatex(const QString &unit);

    Result m_result;
    QComboBox *m_field = nullptr;
    QListWidget *m_eqList = nullptr;
    QLabel *m_eqTitle = nullptr;
    QLabel *m_eqLatex = nullptr;
    QLabel *m_eqDesc = nullptr;

    QTabWidget *m_modeTabs = nullptr;

    // Point solve
    QComboBox *m_unknown = nullptr;
    QWidget *m_varsHost = nullptr;
    QFormLayout *m_varsForm = nullptr;
    QHash<QString, QLineEdit *> m_inputs;
    QPushButton *m_solveBtn = nullptr;
    QLabel *m_resultText = nullptr;
    QLabel *m_resultPreview = nullptr;

    // Dependency graph
    QComboBox *m_depY = nullptr;
    QComboBox *m_depX = nullptr;
    QDoubleSpinBox *m_xMin = nullptr;
    QDoubleSpinBox *m_xMax = nullptr;
    QSpinBox *m_samples = nullptr;
    QPushButton *m_plotBtn = nullptr;
    QLabel *m_graphPreview = nullptr;
    QLabel *m_graphInfo = nullptr;

    QComboBox *m_insertWhat = nullptr;
    QLabel *m_error = nullptr;
    QPushButton *m_okBtn = nullptr;

    PhysicsEquation m_current;
    bool m_hasCurrent = false;
    QImage m_previewImage;
    QImage m_graphImage;
};
