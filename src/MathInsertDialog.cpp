#include "MathInsertDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QSignalBlocker>
#include <QPixmap>

MathInsertDialog::MathInsertDialog(StartTab startTab, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Insert Math Formula / Graph"));
    setMinimumSize(720, 620);
    resize(780, 680);

    auto *root = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    auto *formulaPage = new QWidget(this);
    auto *graphPage = new QWidget(this);
    auto *tutorialPage = new QWidget(this);
    buildFormulaTab(formulaPage);
    buildGraphTab(graphPage);
    buildTutorialTab(tutorialPage);
    m_tabs->addTab(formulaPage, QStringLiteral("Formula"));
    m_tabs->addTab(graphPage, QStringLiteral("Graph y = f(x)"));
    m_tabs->addTab(tutorialPage, QStringLiteral("Tutorial"));
    root->addWidget(m_tabs, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    m_okButton->setText(QStringLiteral("Insert on Canvas"));
    connect(buttons, &QDialogButtonBox::accepted, this, &MathInsertDialog::acceptInsert);
    connect(buttons, &QDialogButtonBox::rejected, this, &MathInsertDialog::reject);
    root->addWidget(buttons);

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        refreshPreview();
        updateOkButton();
        Q_UNUSED(idx);
    });

    // Defaults
    m_formulaEdit->setPlainText(QStringLiteral("\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}"));
    m_graphExpr->setText(QStringLiteral("sin(x)"));
    setStartTab(startTab);
    refreshPreview();
    updateOkButton();
}

void MathInsertDialog::setStartTab(StartTab tab)
{
    if (!m_tabs)
        return;
    m_tabs->setCurrentIndex(static_cast<int>(tab));
    updateOkButton();
}

void MathInsertDialog::updateOkButton()
{
    if (!m_okButton || !m_tabs)
        return;
    if (m_tabs->currentIndex() == static_cast<int>(StartTab::Tutorial)) {
        m_okButton->setText(QStringLiteral("Close"));
        setWindowTitle(QStringLiteral("Math Formula Tutorial"));
    } else if (m_tabs->currentIndex() == static_cast<int>(StartTab::Graph)) {
        m_okButton->setText(QStringLiteral("Insert Graph on Canvas"));
        setWindowTitle(QStringLiteral("Insert Function Graph"));
    } else {
        m_okButton->setText(QStringLiteral("Insert Formula on Canvas"));
        setWindowTitle(QStringLiteral("Insert Math Formula"));
    }
}

void MathInsertDialog::addExampleChip(QLayout *layout, const QString &label,
                                      const QString &payload, Mode mode)
{
    auto *btn = new QPushButton(label, this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        QStringLiteral("QPushButton { padding: 4px 10px; border-radius: 4px; "
                       "border: 1px solid #3a3f47; background: #2a2f36; color: #e8eaed; }"
                       "QPushButton:hover { border-color: #2a82da; }"));
    connect(btn, &QPushButton::clicked, this, [this, payload, mode]() {
        if (mode == Mode::Formula) {
            m_tabs->setCurrentIndex(0);
            m_formulaEdit->setPlainText(payload);
        } else {
            m_tabs->setCurrentIndex(1);
            m_graphExpr->setText(payload);
        }
        refreshPreview();
    });
    layout->addWidget(btn);
}

void MathInsertDialog::buildFormulaTab(QWidget *page)
{
    auto *lay = new QVBoxLayout(page);

    auto *hint = new QLabel(
        QStringLiteral(
            "Enter a formula in plain text or light LaTeX "
            "(e.g. <code>x^2</code>, <code>\\frac{a}{b}</code>, "
            "<code>\\sqrt{x}</code>). See the Tutorial tab for full guide."),
        page);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    hint->setStyleSheet(QStringLiteral("color: #9ca3af; padding: 4px;"));
    lay->addWidget(hint);

    auto *exRow = new QHBoxLayout();
    exRow->addWidget(new QLabel(QStringLiteral("Examples:"), page));
    addExampleChip(exRow, QStringLiteral("E=mc²"), QStringLiteral("E = mc^2"),
                   Mode::Formula);
    addExampleChip(exRow, QStringLiteral("Quadratic"),
                   QStringLiteral("\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}"),
                   Mode::Formula);
    addExampleChip(exRow, QStringLiteral("Integral"),
                   QStringLiteral("\\int_0^1 x^2 dx"), Mode::Formula);
    addExampleChip(exRow, QStringLiteral("Sum"),
                   QStringLiteral("\\sum_{n=1}^{N} \\frac{1}{n^2}"), Mode::Formula);
    addExampleChip(exRow, QStringLiteral("Greek"),
                   QStringLiteral("\\alpha + \\beta = \\gamma"), Mode::Formula);
    exRow->addStretch();
    lay->addLayout(exRow);

    m_formulaEdit = new QPlainTextEdit(page);
    m_formulaEdit->setPlaceholderText(
        QStringLiteral("e.g. \\frac{a}{b}  or  sin(x)/x  or  a_{ij}"));
    m_formulaEdit->setMinimumHeight(90);
    lay->addWidget(m_formulaEdit);

    auto *opts = new QHBoxLayout();
    opts->addWidget(new QLabel(QStringLiteral("Font size:"), page));
    m_formulaSize = new QSpinBox(page);
    m_formulaSize->setRange(12, 96);
    m_formulaSize->setValue(32);
    opts->addWidget(m_formulaSize);
    m_formulaColorBtn = new QPushButton(QStringLiteral("Text color"), page);
    opts->addWidget(m_formulaColorBtn);
    m_formulaBg = new QCheckBox(QStringLiteral("White background"), page);
    opts->addWidget(m_formulaBg);
    opts->addStretch();
    lay->addLayout(opts);

    m_formulaError = new QLabel(page);
    m_formulaError->setStyleSheet(QStringLiteral("color: #f87171;"));
    m_formulaError->setWordWrap(true);
    lay->addWidget(m_formulaError);

    auto *previewBox = new QGroupBox(QStringLiteral("Preview"), page);
    auto *previewLay = new QVBoxLayout(previewBox);
    m_formulaPreview = new QLabel(previewBox);
    m_formulaPreview->setAlignment(Qt::AlignCenter);
    m_formulaPreview->setMinimumHeight(140);
    m_formulaPreview->setStyleSheet(
        QStringLiteral("background: #1a1d23; border-radius: 6px; padding: 8px;"));
    previewLay->addWidget(m_formulaPreview);
    lay->addWidget(previewBox, 1);

    connect(m_formulaEdit, &QPlainTextEdit::textChanged, this,
            &MathInsertDialog::refreshPreview);
    connect(m_formulaSize, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &MathInsertDialog::refreshPreview);
    connect(m_formulaBg, &QCheckBox::toggled, this, &MathInsertDialog::refreshPreview);
    connect(m_formulaColorBtn, &QPushButton::clicked, this,
            &MathInsertDialog::pickFormulaColor);
}

void MathInsertDialog::buildGraphTab(QWidget *page)
{
    auto *lay = new QVBoxLayout(page);

    auto *hint = new QLabel(
        QStringLiteral(
            "Enter <b>y = f(x)</b> using the expression language "
            "(radians for trig). Options control axes, grid, domain, and style."),
        page);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    hint->setStyleSheet(QStringLiteral("color: #9ca3af; padding: 4px;"));
    lay->addWidget(hint);

    auto *exRow = new QHBoxLayout();
    exRow->addWidget(new QLabel(QStringLiteral("Examples:"), page));
    addExampleChip(exRow, QStringLiteral("sin(x)"), QStringLiteral("sin(x)"),
                   Mode::Graph);
    addExampleChip(exRow, QStringLiteral("parabola"), QStringLiteral("x^2"),
                   Mode::Graph);
    addExampleChip(exRow, QStringLiteral("exp"), QStringLiteral("exp(-x^2)"),
                   Mode::Graph);
    addExampleChip(exRow, QStringLiteral("1/(1+x²)"), QStringLiteral("1/(1+x^2)"),
                   Mode::Graph);
    addExampleChip(exRow, QStringLiteral("sinc"), QStringLiteral("sin(x)/x"),
                   Mode::Graph);
    addExampleChip(exRow, QStringLiteral("|x|"), QStringLiteral("abs(x)"),
                   Mode::Graph);
    exRow->addStretch();
    lay->addLayout(exRow);

    auto *exprRow = new QHBoxLayout();
    exprRow->addWidget(new QLabel(QStringLiteral("y ="), page));
    m_graphExpr = new QLineEdit(page);
    m_graphExpr->setPlaceholderText(QStringLiteral("sin(x) + 0.3*cos(3*x)"));
    exprRow->addWidget(m_graphExpr, 1);
    lay->addLayout(exprRow);

    auto *domain = new QGroupBox(QStringLiteral("Domain & range"), page);
    auto *dform = new QFormLayout(domain);
    m_xMin = new QDoubleSpinBox(page);
    m_xMax = new QDoubleSpinBox(page);
    m_yMin = new QDoubleSpinBox(page);
    m_yMax = new QDoubleSpinBox(page);
    for (auto *s : {m_xMin, m_xMax, m_yMin, m_yMax}) {
        s->setRange(-1e6, 1e6);
        s->setDecimals(4);
        s->setSingleStep(0.5);
    }
    m_xMin->setValue(-6.2832);
    m_xMax->setValue(6.2832);
    m_yMin->setValue(-2);
    m_yMax->setValue(2);
    m_autoY = new QCheckBox(QStringLiteral("Auto Y range from data"), page);
    m_autoY->setChecked(true);
    dform->addRow(QStringLiteral("x min"), m_xMin);
    dform->addRow(QStringLiteral("x max"), m_xMax);
    dform->addRow(QStringLiteral("y min"), m_yMin);
    dform->addRow(QStringLiteral("y max"), m_yMax);
    dform->addRow(QString(), m_autoY);
    lay->addWidget(domain);

    auto *style = new QGroupBox(QStringLiteral("Display options"), page);
    auto *sgrid = new QHBoxLayout(style);
    auto *col1 = new QVBoxLayout();
    m_showAxes = new QCheckBox(QStringLiteral("Axes"), page);
    m_showGrid = new QCheckBox(QStringLiteral("Grid"), page);
    m_showTicks = new QCheckBox(QStringLiteral("Ticks"), page);
    m_showLabels = new QCheckBox(QStringLiteral("Axis labels"), page);
    m_showTitle = new QCheckBox(QStringLiteral("Title y = f(x)"), page);
    m_showAxes->setChecked(true);
    m_showGrid->setChecked(true);
    m_showTicks->setChecked(true);
    m_showLabels->setChecked(true);
    m_showTitle->setChecked(true);
    for (auto *c : {m_showAxes, m_showGrid, m_showTicks, m_showLabels, m_showTitle})
        col1->addWidget(c);
    sgrid->addLayout(col1);

    auto *col2 = new QFormLayout();
    m_samples = new QSpinBox(page);
    m_samples->setRange(50, 5000);
    m_samples->setValue(600);
    m_graphW = new QSpinBox(page);
    m_graphH = new QSpinBox(page);
    m_graphW->setRange(200, 2400);
    m_graphH->setRange(150, 1800);
    m_graphW->setValue(800);
    m_graphH->setValue(500);
    m_curveWidth = new QDoubleSpinBox(page);
    m_curveWidth->setRange(0.5, 12.0);
    m_curveWidth->setSingleStep(0.2);
    m_curveWidth->setValue(2.4);
    m_curveColorBtn = new QPushButton(QStringLiteral("Curve color"), page);
    col2->addRow(QStringLiteral("Samples"), m_samples);
    col2->addRow(QStringLiteral("Width px"), m_graphW);
    col2->addRow(QStringLiteral("Height px"), m_graphH);
    col2->addRow(QStringLiteral("Line width"), m_curveWidth);
    col2->addRow(QStringLiteral("Color"), m_curveColorBtn);
    sgrid->addLayout(col2);
    lay->addWidget(style);

    m_graphError = new QLabel(page);
    m_graphError->setStyleSheet(QStringLiteral("color: #f87171;"));
    m_graphError->setWordWrap(true);
    lay->addWidget(m_graphError);

    auto *previewBox = new QGroupBox(QStringLiteral("Preview"), page);
    auto *previewLay = new QVBoxLayout(previewBox);
    m_graphPreview = new QLabel(previewBox);
    m_graphPreview->setAlignment(Qt::AlignCenter);
    m_graphPreview->setMinimumHeight(180);
    m_graphPreview->setStyleSheet(
        QStringLiteral("background: #1a1d23; border-radius: 6px;"));
    previewLay->addWidget(m_graphPreview);
    lay->addWidget(previewBox, 1);

    auto refresh = [this]() { refreshPreview(); };
    connect(m_graphExpr, &QLineEdit::textChanged, this, refresh);
    for (auto *s : {m_xMin, m_xMax, m_yMin, m_yMax, m_curveWidth})
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                refresh);
    for (auto *s : {m_samples, m_graphW, m_graphH})
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, refresh);
    for (auto *c : {m_autoY, m_showAxes, m_showGrid, m_showTicks, m_showLabels,
                    m_showTitle})
        connect(c, &QCheckBox::toggled, this, refresh);
    connect(m_curveColorBtn, &QPushButton::clicked, this,
            &MathInsertDialog::pickCurveColor);
}

void MathInsertDialog::buildTutorialTab(QWidget *page)
{
    auto *lay = new QVBoxLayout(page);
    auto *browser = new QTextBrowser(page);
    browser->setOpenExternalLinks(false);
    browser->setHtml(MathExpression::tutorialMarkdown());
    browser->setStyleSheet(
        QStringLiteral("QTextBrowser { background: #1a1d23; color: #e8eaed; "
                       "border: 1px solid #2f343c; border-radius: 6px; padding: 12px; }"
                       "a { color: #60a5fa; }"));
    lay->addWidget(browser);
}

void MathInsertDialog::pickCurveColor()
{
    const QColor c = QColorDialog::getColor(m_curveColor, this, QStringLiteral("Curve color"));
    if (c.isValid()) {
        m_curveColor = c;
        refreshPreview();
    }
}

void MathInsertDialog::pickFormulaColor()
{
    const QColor c =
        QColorDialog::getColor(m_formulaColor, this, QStringLiteral("Formula color"));
    if (c.isValid()) {
        m_formulaColor = c;
        refreshPreview();
    }
}

void MathInsertDialog::refreshPreview()
{
    // Formula
    if (m_formulaEdit) {
        MathFormulaRenderer::Options fo;
        fo.font = QFont(QStringLiteral("Times New Roman"), m_formulaSize->value());
        fo.textColor = m_formulaColor;
        fo.drawBackground = m_formulaBg->isChecked();
        fo.background = Qt::white;
        fo.padding = 18;
        m_previewFormula =
            MathFormulaRenderer::render(m_formulaEdit->toPlainText(), fo);
        if (m_previewFormula.isNull()) {
            m_formulaError->setText(QStringLiteral("Could not render formula."));
            m_formulaPreview->setPixmap(QPixmap());
            m_formulaPreview->setText(QStringLiteral("(empty)"));
        } else {
            m_formulaError->clear();
            const QPixmap pm = QPixmap::fromImage(
                m_previewFormula.scaled(m_formulaPreview->width() > 40
                                            ? m_formulaPreview->width() - 16
                                            : 480,
                                        160, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation));
            m_formulaPreview->setPixmap(pm);
            m_formulaPreview->setText(QString());
        }
    }

    // Graph
    if (m_graphExpr) {
        MathGraphRenderer::Options go;
        go.expression = m_graphExpr->text();
        go.xMin = m_xMin->value();
        go.xMax = m_xMax->value();
        go.yMin = m_yMin->value();
        go.yMax = m_yMax->value();
        go.autoY = m_autoY->isChecked();
        go.samples = m_samples->value();
        go.width = m_graphW->value();
        go.height = m_graphH->value();
        go.showAxes = m_showAxes->isChecked();
        go.showGrid = m_showGrid->isChecked();
        go.showTicks = m_showTicks->isChecked();
        go.showLabels = m_showLabels->isChecked();
        go.showTitle = m_showTitle->isChecked();
        go.curveColor = m_curveColor;
        go.curveWidth = static_cast<float>(m_curveWidth->value());

        const auto gr = MathGraphRenderer::render(go);
        m_previewGraph = gr.image;
        if (!gr.error.isEmpty()) {
            m_graphError->setText(gr.error);
            m_graphPreview->setPixmap(QPixmap());
            m_graphPreview->setText(gr.error);
        } else {
            m_graphError->clear();
            const QPixmap pm = QPixmap::fromImage(m_previewGraph.scaled(
                m_graphPreview->width() > 40 ? m_graphPreview->width() - 8 : 640,
                220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_graphPreview->setPixmap(pm);
            m_graphPreview->setText(QString());
        }
    }
}

void MathInsertDialog::acceptInsert()
{
    // Tutorial is read-only help — Close dismisses without inserting
    if (m_tabs && m_tabs->currentIndex() == static_cast<int>(StartTab::Tutorial)) {
        reject();
        return;
    }

    refreshPreview();
    const bool formulaTab = m_tabs->currentIndex() == 0;
    if (formulaTab) {
        if (m_previewFormula.isNull()) {
            m_formulaError->setText(QStringLiteral("Nothing to insert — fix the formula."));
            return;
        }
        m_result.mode = Mode::Formula;
        m_result.image = m_previewFormula;
        m_result.expression = m_formulaEdit->toPlainText().trimmed();
        m_result.canvasWidth = 360.0f;
    } else {
        if (m_previewGraph.isNull()) {
            m_graphError->setText(
                m_graphError->text().isEmpty()
                    ? QStringLiteral("Nothing to insert — fix the expression.")
                    : m_graphError->text());
            return;
        }
        m_result.mode = Mode::Graph;
        m_result.image = m_previewGraph;
        m_result.expression = m_graphExpr->text().trimmed();
        m_result.canvasWidth = 480.0f;
    }
    accept();
}
