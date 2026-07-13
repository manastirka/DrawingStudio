#include "PhysicsSolverDialog.h"
#include "MathExpression.h"
#include "MathFormulaRenderer.h"
#include "MathGraphRenderer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QPixmap>
#include <QSignalBlocker>
#include <QFrame>
#include <QTabWidget>
#include <QApplication>
#include <QClipboard>
#include <QSet>
#include <QRegularExpression>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <algorithm>
#include <cmath>

namespace {

bool shouldPrefillConstant(const PhysicsVariable &v)
{
    const QString id = v.id.toLower();
    const QString n = v.name.toLower();
    static const QSet<QString> always = {
        QStringLiteral("gn"), QStringLiteral("ke"), QStringLiteral("mu0"),
        QStringLiteral("eps0"), QStringLiteral("hbar"), QStringLiteral("na"),
        QStringLiteral("e_charge"), QStringLiteral("sigma"), QStringLiteral("me"),
        QStringLiteral("mp"), QStringLiteral("mn"), QStringLiteral("msun"),
        QStringLiteral("mearth"), QStringLiteral("rearth"), QStringLiteral("au")
    };
    if (always.contains(id))
        return true;
    if (id == QStringLiteral("g") &&
        (n.contains(QStringLiteral("grav")) || n == QStringLiteral("gravity")))
        return true;
    if (id == QStringLiteral("c") && n.contains(QStringLiteral("light")))
        return true;
    if (id == QStringLiteral("h") && n.contains(QStringLiteral("planck")))
        return true;
    if (id == QStringLiteral("k") && n.contains(QStringLiteral("boltzmann")))
        return true;
    if (id == QStringLiteral("r") && n.contains(QStringLiteral("gas")))
        return true;
    return false;
}

} // namespace

PhysicsSolverDialog::PhysicsSolverDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Physics Problem Solver"));
    setMinimumSize(980, 700);
    resize(1080, 760);

    auto *root = new QVBoxLayout(this);
    auto *split = new QSplitter(Qt::Horizontal, this);

    // Left: field + equation / constants
    auto *left = new QWidget(this);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->addWidget(new QLabel(QStringLiteral("Field"), left));
    m_field = new QComboBox(left);
    m_field->addItem(QStringLiteral("All fields"));
    for (const QString &f : PhysicsCatalog::fields())
        m_field->addItem(f);
    leftLay->addWidget(m_field);

    auto *leftTabs = new QTabWidget(left);
    auto *eqPage = new QWidget(leftTabs);
    auto *eqLay = new QVBoxLayout(eqPage);
    eqLay->setContentsMargins(0, 6, 0, 0);
    eqLay->addWidget(new QLabel(QStringLiteral("Equation"), eqPage));
    m_eqList = new QListWidget(eqPage);
    eqLay->addWidget(m_eqList, 1);
    leftTabs->addTab(eqPage, QStringLiteral("Equations"));

    auto *constPage = new QWidget(leftTabs);
    auto *constLay = new QVBoxLayout(constPage);
    constLay->setContentsMargins(0, 6, 0, 0);
    auto *constHint = new QLabel(
        QStringLiteral("Fundamental constants (SI). Click to copy id."),
        constPage);
    constHint->setWordWrap(true);
    constHint->setStyleSheet(QStringLiteral("color:#9ca3af; font-size:11px;"));
    constLay->addWidget(constHint);
    auto *constList = new QListWidget(constPage);
    for (const auto &c : PhysicsCatalog::constants()) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1 = %2 %3")
                .arg(c.symbolLatex, QString::number(c.value, 'g', 10), c.unit),
            constList);
        item->setToolTip(
            QStringLiteral("%1\n%2\nid: %3")
                .arg(c.name, c.category, c.id));
        item->setData(Qt::UserRole, c.id);
    }
    connect(constList, &QListWidget::itemClicked, this, [](QListWidgetItem *item) {
        if (item)
            QApplication::clipboard()->setText(item->data(Qt::UserRole).toString());
    });
    constLay->addWidget(constList, 1);
    leftTabs->addTab(constPage, QStringLiteral("Constants"));
    leftLay->addWidget(leftTabs, 1);
    split->addWidget(left);

    // Right
    auto *right = new QWidget(this);
    auto *rightLay = new QVBoxLayout(right);

    m_eqTitle = new QLabel(right);
    m_eqTitle->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 600;"));
    m_eqTitle->setWordWrap(true);
    rightLay->addWidget(m_eqTitle);

    m_eqLatex = new QLabel(right);
    m_eqLatex->setAlignment(Qt::AlignCenter);
    m_eqLatex->setMinimumHeight(64);
    m_eqLatex->setStyleSheet(
        QStringLiteral("background:#1a1d23; border-radius:6px; padding:8px;"));
    rightLay->addWidget(m_eqLatex);

    m_eqDesc = new QLabel(right);
    m_eqDesc->setWordWrap(true);
    m_eqDesc->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    rightLay->addWidget(m_eqDesc);

    // Shared knowns
    auto *varsBox =
        new QGroupBox(QStringLiteral("Fixed / known values"), right);
    auto *scroll = new QScrollArea(varsBox);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMaximumHeight(180);
    m_varsHost = new QWidget(scroll);
    m_varsForm = new QFormLayout(m_varsHost);
    scroll->setWidget(m_varsHost);
    auto *varsLay = new QVBoxLayout(varsBox);
    varsLay->addWidget(scroll);
    rightLay->addWidget(varsBox);

    m_modeTabs = new QTabWidget(right);

    // ---- Point solve tab ----
    auto *solvePage = new QWidget(m_modeTabs);
    auto *solveLay = new QVBoxLayout(solvePage);
    auto *unkRow = new QHBoxLayout();
    unkRow->addWidget(new QLabel(QStringLiteral("Solve for:"), solvePage));
    m_unknown = new QComboBox(solvePage);
    unkRow->addWidget(m_unknown, 1);
    m_solveBtn = new QPushButton(QStringLiteral("Solve"), solvePage);
    unkRow->addWidget(m_solveBtn);
    solveLay->addLayout(unkRow);

    m_resultText = new QLabel(solvePage);
    m_resultText->setWordWrap(true);
    m_resultText->setStyleSheet(QStringLiteral("color:#a7f3d0; font-size:13px;"));
    solveLay->addWidget(m_resultText);

    auto *resBox = new QGroupBox(QStringLiteral("Result formula"), solvePage);
    auto *resLay = new QVBoxLayout(resBox);
    m_resultPreview = new QLabel(resBox);
    m_resultPreview->setAlignment(Qt::AlignCenter);
    m_resultPreview->setMinimumHeight(140);
    m_resultPreview->setStyleSheet(
        QStringLiteral(
            "background:#f7f7f5; border:1px solid #d4d4d8; border-radius:4px;"));
    resLay->addWidget(m_resultPreview);
    solveLay->addWidget(resBox, 1);
    m_modeTabs->addTab(solvePage, QStringLiteral("Point solve"));

    // ---- Dependency graph tab ----
    auto *depPage = new QWidget(m_modeTabs);
    auto *depLay = new QVBoxLayout(depPage);

    auto *depHint = new QLabel(
        QStringLiteral(
            "Sweep one variable over a range and plot how another depends on it. "
            "Other quantities stay fixed."),
        depPage);
    depHint->setWordWrap(true);
    depHint->setStyleSheet(QStringLiteral("color:#9ca3af; font-size:12px;"));
    depLay->addWidget(depHint);

    auto *depForm = new QFormLayout();
    m_depY = new QComboBox(depPage);
    m_depX = new QComboBox(depPage);
    depForm->addRow(QStringLiteral("Dependent  Y (vertical)"), m_depY);
    depForm->addRow(QStringLiteral("Independent X (horizontal)"), m_depX);
    depLay->addLayout(depForm);

    auto *rangeRow = new QHBoxLayout();
    m_xMin = new QDoubleSpinBox(depPage);
    m_xMax = new QDoubleSpinBox(depPage);
    m_samples = new QSpinBox(depPage);
    for (auto *sp : {m_xMin, m_xMax}) {
        sp->setDecimals(6);
        sp->setRange(-1e12, 1e12);
        sp->setMinimumWidth(110);
    }
    m_xMin->setValue(0.0);
    m_xMax->setValue(10.0);
    m_samples->setRange(20, 2000);
    m_samples->setValue(400);
    rangeRow->addWidget(new QLabel(QStringLiteral("X min"), depPage));
    rangeRow->addWidget(m_xMin);
    rangeRow->addWidget(new QLabel(QStringLiteral("X max"), depPage));
    rangeRow->addWidget(m_xMax);
    rangeRow->addWidget(new QLabel(QStringLiteral("Samples"), depPage));
    rangeRow->addWidget(m_samples);
    m_plotBtn = new QPushButton(QStringLiteral("Plot dependency"), depPage);
    rangeRow->addWidget(m_plotBtn);
    depLay->addLayout(rangeRow);

    m_graphInfo = new QLabel(depPage);
    m_graphInfo->setWordWrap(true);
    m_graphInfo->setStyleSheet(QStringLiteral("color:#a7f3d0; font-size:12px;"));
    depLay->addWidget(m_graphInfo);

    auto *gBox = new QGroupBox(QStringLiteral("Dependency graph"), depPage);
    auto *gLay = new QVBoxLayout(gBox);
    m_graphPreview = new QLabel(gBox);
    m_graphPreview->setAlignment(Qt::AlignCenter);
    m_graphPreview->setMinimumHeight(220);
    m_graphPreview->setStyleSheet(
        QStringLiteral(
            "background:#fafafa; border:1px solid #d4d4d8; border-radius:4px;"));
    gLay->addWidget(m_graphPreview);
    depLay->addWidget(gBox, 1);
    m_modeTabs->addTab(depPage, QStringLiteral("Dependency graph"));

    rightLay->addWidget(m_modeTabs, 1);

    m_error = new QLabel(right);
    m_error->setStyleSheet(QStringLiteral("color:#f87171;"));
    m_error->setWordWrap(true);
    rightLay->addWidget(m_error);

    auto *insertRow = new QHBoxLayout();
    insertRow->addWidget(new QLabel(QStringLiteral("Insert on canvas:"), right));
    m_insertWhat = new QComboBox(right);
    m_insertWhat->addItem(QStringLiteral("Formula only"),
                          static_cast<int>(InsertMode::Formula));
    m_insertWhat->addItem(QStringLiteral("Graph only"),
                          static_cast<int>(InsertMode::Graph));
    m_insertWhat->addItem(QStringLiteral("Formula + Graph"),
                          static_cast<int>(InsertMode::Both));
    insertRow->addWidget(m_insertWhat, 1);
    rightLay->addLayout(insertRow);

    split->addWidget(right);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    root->addWidget(split, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);
    m_okBtn->setText(QStringLiteral("Insert on Canvas"));
    connect(buttons, &QDialogButtonBox::accepted, this,
            &PhysicsSolverDialog::acceptInsert);
    connect(buttons, &QDialogButtonBox::rejected, this, &PhysicsSolverDialog::reject);
    root->addWidget(buttons);

    connect(m_field, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PhysicsSolverDialog::onFieldChanged);
    connect(m_eqList, &QListWidget::currentRowChanged, this,
            &PhysicsSolverDialog::onEquationSelected);
    connect(m_unknown, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PhysicsSolverDialog::onUnknownChanged);
    connect(m_depX, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PhysicsSolverDialog::onIndepChanged);
    connect(m_modeTabs, &QTabWidget::currentChanged, this,
            &PhysicsSolverDialog::onModeTabChanged);
    connect(m_solveBtn, &QPushButton::clicked, this, &PhysicsSolverDialog::solveNow);
    connect(m_plotBtn, &QPushButton::clicked, this,
            &PhysicsSolverDialog::plotDependency);
    connect(m_insertWhat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateInsertButton(); });

    rebuildEquationList();
    if (m_eqList->count() > 0)
        m_eqList->setCurrentRow(0);
    updateInsertButton();
}

void PhysicsSolverDialog::onFieldChanged(int) { rebuildEquationList(); }

void PhysicsSolverDialog::onModeTabChanged(int index)
{
    if (index == 1)
        m_insertWhat->setCurrentIndex(1); // prefer graph
    else
        m_insertWhat->setCurrentIndex(0);
    updateInsertButton();
}

void PhysicsSolverDialog::updateInsertButton()
{
    const auto mode = static_cast<InsertMode>(m_insertWhat->currentData().toInt());
    switch (mode) {
    case InsertMode::Graph:
        m_okBtn->setText(QStringLiteral("Insert Graph on Canvas"));
        break;
    case InsertMode::Both:
        m_okBtn->setText(QStringLiteral("Insert Formula + Graph"));
        break;
    default:
        m_okBtn->setText(QStringLiteral("Insert Formula on Canvas"));
        break;
    }
}

void PhysicsSolverDialog::rebuildEquationList()
{
    const QSignalBlocker b(m_eqList);
    m_eqList->clear();
    const QString field =
        m_field->currentIndex() <= 0 ? QString() : m_field->currentText();
    const auto eqs = field.isEmpty() ? PhysicsCatalog::equations()
                                     : PhysicsCatalog::equationsInField(field);
    for (const auto &e : eqs) {
        auto *item = new QListWidgetItem(e.name, m_eqList);
        item->setData(Qt::UserRole, e.id);
        item->setToolTip(e.field + QStringLiteral("\n") + e.description);
    }
    if (m_eqList->count() > 0)
        m_eqList->setCurrentRow(0);
}

void PhysicsSolverDialog::onEquationSelected()
{
    auto *item = m_eqList->currentItem();
    if (!item) {
        m_hasCurrent = false;
        return;
    }
    const PhysicsEquation *eq =
        PhysicsCatalog::equationById(item->data(Qt::UserRole).toString());
    if (!eq) {
        m_hasCurrent = false;
        return;
    }
    m_current = *eq;
    m_hasCurrent = true;
    m_eqTitle->setText(m_current.name);
    m_eqDesc->setText(m_current.field + QStringLiteral(" — ") + m_current.description);
    updateFormulaPreview();
    rebuildVariableForm();
    rebuildDependencyCombos();
    m_error->clear();
    m_resultText->clear();
    m_resultPreview->clear();
    m_graphPreview->clear();
    m_graphInfo->clear();
    m_previewImage = QImage();
    m_graphImage = QImage();
}

void PhysicsSolverDialog::updateFormulaPreview()
{
    if (!m_hasCurrent) {
        m_eqLatex->setText(QString());
        return;
    }
    MathFormulaRenderer::Options opt;
    opt.font = QFont(QStringLiteral("Times New Roman"), 24);
    opt.textColor = QColor(230, 234, 240);
    opt.padding = 12;
    const QImage img = MathFormulaRenderer::render(m_current.latex, opt);
    if (img.isNull()) {
        m_eqLatex->setText(m_current.latex);
    } else {
        m_eqLatex->setPixmap(QPixmap::fromImage(
            img.scaled(m_eqLatex->width() > 40 ? m_eqLatex->width() - 12 : 480,
                       70, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
}

void PhysicsSolverDialog::rebuildVariableForm()
{
    while (m_varsForm->rowCount() > 0)
        m_varsForm->removeRow(0);
    m_inputs.clear();
    const QSignalBlocker bu(m_unknown);
    m_unknown->clear();
    if (!m_hasCurrent)
        return;

    const QHash<QString, double> consts = PhysicsCatalog::constantValues();

    for (const auto &v : m_current.variables) {
        m_unknown->addItem(
            QStringLiteral("%1 (%2)").arg(v.symbolLatex, v.name), v.id);
        auto *edit = new QLineEdit(m_varsHost);
        edit->setPlaceholderText(v.unit.isEmpty() ? QStringLiteral("value")
                                                  : v.unit);
        if (shouldPrefillConstant(v) && consts.contains(v.id.toLower())) {
            edit->setText(QString::number(consts.value(v.id.toLower()), 'g', 12));
            edit->setToolTip(QStringLiteral("Prefill from physical constant — editable"));
        }
        m_varsForm->addRow(
            QStringLiteral("%1  [%2]").arg(v.symbolLatex, v.unit), edit);
        m_inputs.insert(v.id, edit);
        connect(edit, &QLineEdit::returnPressed, this, &PhysicsSolverDialog::solveNow);
    }

    int unkIdx = 0;
    for (int i = 0; i < m_unknown->count(); ++i) {
        if (m_unknown->itemData(i).toString() == m_current.defaultUnknown) {
            unkIdx = i;
            break;
        }
    }
    m_unknown->setCurrentIndex(unkIdx);
    onUnknownChanged(unkIdx);
}

void PhysicsSolverDialog::rebuildDependencyCombos()
{
    const QSignalBlocker by(m_depY);
    const QSignalBlocker bx(m_depX);
    m_depY->clear();
    m_depX->clear();
    if (!m_hasCurrent)
        return;

    for (const auto &v : m_current.variables) {
        if (!m_current.solve.contains(v.id))
            continue;
        m_depY->addItem(
            QStringLiteral("%1 (%2)").arg(v.symbolLatex, v.name), v.id);
    }
    for (const auto &v : m_current.variables) {
        m_depX->addItem(
            QStringLiteral("%1 (%2) [%3]")
                .arg(v.symbolLatex, v.name, v.unit),
            v.id);
    }

    // Prefer defaultUnknown as Y
    for (int i = 0; i < m_depY->count(); ++i) {
        if (m_depY->itemData(i).toString() == m_current.defaultUnknown) {
            m_depY->setCurrentIndex(i);
            break;
        }
    }
    // Prefer a different var as X (often time/length)
    QString preferX;
    for (const auto &cand :
         {QStringLiteral("t"), QStringLiteral("x"), QStringLiteral("r"),
          QStringLiteral("s"), QStringLiteral("v"), QStringLiteral("a")}) {
        for (const auto &v : m_current.variables) {
            if (v.id == cand && v.id != m_depY->currentData().toString()) {
                preferX = cand;
                break;
            }
        }
        if (!preferX.isEmpty())
            break;
    }
    if (preferX.isEmpty()) {
        for (const auto &v : m_current.variables) {
            if (v.id != m_depY->currentData().toString()) {
                preferX = v.id;
                break;
            }
        }
    }
    for (int i = 0; i < m_depX->count(); ++i) {
        if (m_depX->itemData(i).toString() == preferX) {
            m_depX->setCurrentIndex(i);
            break;
        }
    }
    suggestIndepRange();
}

void PhysicsSolverDialog::suggestIndepRange()
{
    if (!m_hasCurrent || !m_depX)
        return;
    const PhysicsVariable xv = findVar(m_depX->currentData().toString());
    const QString u = xv.unit.toLower();
    double lo = 0.0, hi = 10.0;
    if (u.contains(QStringLiteral("s")) && !u.contains(QStringLiteral("m/s"))) {
        lo = 0;
        hi = 10; // time
    } else if (u == QStringLiteral("m") || u.contains(QStringLiteral("m^"))) {
        lo = 0;
        hi = 100;
    } else if (u.contains(QStringLiteral("m/s"))) {
        lo = 0;
        hi = 50;
    } else if (u.contains(QStringLiteral("m/s^2")) ||
               u.contains(QStringLiteral("m/s^{2}"))) {
        lo = -10;
        hi = 10;
    } else if (u.contains(QStringLiteral("kg"))) {
        lo = 0.1;
        hi = 20;
    } else if (u.contains(QStringLiteral("n")) || u == QStringLiteral("N")) {
        lo = 0;
        hi = 100;
    } else if (u.contains(QStringLiteral("k")) && u.contains(QStringLiteral("j"))) {
        lo = 200;
        hi = 600; // temperature-ish
    } else if (u.contains(QStringLiteral("hz")) || u.contains(QStringLiteral("1/s"))) {
        lo = 0;
        hi = 100;
    } else if (u.contains(QStringLiteral("rad"))) {
        lo = 0;
        hi = 6.283185307;
    }
    const QSignalBlocker b1(m_xMin);
    const QSignalBlocker b2(m_xMax);
    m_xMin->setValue(lo);
    m_xMax->setValue(hi);
}

void PhysicsSolverDialog::onIndepChanged(int)
{
    suggestIndepRange();
}

void PhysicsSolverDialog::onUnknownChanged(int index)
{
    if (index < 0)
        return;
    const QString unk = m_unknown->itemData(index).toString();
    // Also sync dependency Y when on point-solve mindset
    for (int i = 0; i < m_depY->count(); ++i) {
        if (m_depY->itemData(i).toString() == unk) {
            const QSignalBlocker b(m_depY);
            m_depY->setCurrentIndex(i);
            break;
        }
    }
    for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        const bool isUnk = (it.key() == unk);
        // Keep editable; unknown just cleared for point solve convenience
        if (isUnk && m_modeTabs->currentIndex() == 0) {
            it.value()->setPlaceholderText(QStringLiteral("(unknown — will solve)"));
        }
    }
}

PhysicsVariable PhysicsSolverDialog::findVar(const QString &id) const
{
    for (const auto &v : m_current.variables) {
        if (v.id == id)
            return v;
    }
    return {};
}

QHash<QString, double> PhysicsSolverDialog::collectKnownValues(
    QString *errorOut, const QString &skipA, const QString &skipB) const
{
    QHash<QString, double> vals;
    if (!m_hasCurrent)
        return vals;
    for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        if (it.key() == skipA || it.key() == skipB)
            continue;
        const QString t = it.value()->text().trimmed();
        if (t.isEmpty())
            continue;
        bool ok = false;
        const double v = t.toDouble(&ok);
        if (!ok) {
            if (errorOut)
                *errorOut = QStringLiteral("Invalid number for %1").arg(it.key());
            return {};
        }
        vals.insert(it.key().toLower(), v);
    }
    return vals;
}

QString PhysicsSolverDialog::rewriteIndepAsX(const QString &formula,
                                             const QString &indepId)
{
    if (indepId.compare(QStringLiteral("x"), Qt::CaseInsensitive) == 0)
        return formula;
    // Whole-identifier replace (case-insensitive word boundary)
    QRegularExpression re(
        QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(indepId)),
        QRegularExpression::CaseInsensitiveOption);
    return QString(formula).replace(re, QStringLiteral("x"));
}

QString PhysicsSolverDialog::formatNumberLatex(double x)
{
    if (!std::isfinite(x))
        return QStringLiteral("\\mathrm{NaN}");
    if (x == 0.0)
        return QStringLiteral("0");
    const double ax = std::abs(x);
    if (ax >= 1e-3 && ax < 1e5) {
        QString s = QString::number(x, 'g', 6);
        s.replace(QLatin1Char(','), QLatin1Char('.'));
        return s;
    }
    int exp = static_cast<int>(std::floor(std::log10(ax)));
    double mant = x / std::pow(10.0, exp);
    if (std::abs(mant) >= 10.0) {
        mant /= 10.0;
        ++exp;
    } else if (std::abs(mant) < 1.0) {
        mant *= 10.0;
        --exp;
    }
    QString m = QString::number(mant, 'g', 5);
    m.replace(QLatin1Char(','), QLatin1Char('.'));
    return QStringLiteral("%1 \\times 10^{%2}").arg(m).arg(exp);
}

QString PhysicsSolverDialog::formatUnitLatex(const QString &unit)
{
    if (unit.trimmed().isEmpty())
        return {};
    QString u = unit.trimmed();
    u.replace(QStringLiteral("·"), QStringLiteral("\\cdot"));
    u.replace(QStringLiteral("μ"), QStringLiteral("\\mu"));
    u.replace(QStringLiteral("Ω"), QStringLiteral("\\Omega"));
    static const QRegularExpression powRe(QStringLiteral("\\^(\\d+)"));
    u.replace(powRe, QStringLiteral("^{\\1}"));
    return QStringLiteral("\\mathrm{%1}").arg(u);
}

void PhysicsSolverDialog::solveNow()
{
    m_error->clear();
    m_resultText->clear();
    m_previewImage = QImage();
    m_resultPreview->clear();
    if (!m_hasCurrent) {
        m_error->setText(QStringLiteral("Select an equation."));
        return;
    }
    const QString unk = m_unknown->currentData().toString();
    if (!m_current.solve.contains(unk)) {
        m_error->setText(QStringLiteral("No rearrange stored for that unknown."));
        return;
    }

    QString err;
    QHash<QString, double> vals = collectKnownValues(&err, unk);
    if (!err.isEmpty()) {
        m_error->setText(err);
        return;
    }

    for (const auto &v : m_current.variables) {
        if (v.id == unk)
            continue;
        if (!vals.contains(v.id.toLower())) {
            m_error->setText(
                QStringLiteral("Missing known value for %1 (%2)")
                    .arg(v.symbolLatex, v.name));
            return;
        }
    }

    MathExpression expr(m_current.solve.value(unk));
    QHash<QString, double> named = PhysicsCatalog::constantValues();
    for (const auto &v : m_current.variables)
        named.remove(v.id.toLower());
    expr.setNamedValues(named);
    auto out = expr.evaluate(vals);
    if (!out) {
        m_error->setText(
            QStringLiteral("Could not evaluate: %1")
                .arg(expr.error().isEmpty() ? QStringLiteral("domain/syntax error")
                                            : expr.error()));
        return;
    }
    const double ans = *out;
    if (!std::isfinite(ans)) {
        m_error->setText(QStringLiteral("Result is not finite — check inputs/domain."));
        return;
    }

    const PhysicsVariable unkVar = findVar(unk);
    const QString unit = unkVar.unit;
    const QString numLatex = formatNumberLatex(ans);
    const QString unitLatex = formatUnitLatex(unit);
    QString answerLatex = unkVar.symbolLatex + QStringLiteral(" = ") + numLatex;
    if (!unitLatex.isEmpty())
        answerLatex += QStringLiteral("\\,") + unitLatex;

    const QString summaryPlain =
        QStringLiteral("%1 = %2%3")
            .arg(unkVar.symbolLatex, QString::number(ans, 'g', 8),
                 unit.isEmpty() ? QString() : QStringLiteral(" ") + unit);
    m_resultText->setText(QStringLiteral("Solved: %1").arg(summaryPlain));

    MathFormulaRenderer::Options opt;
    opt.font = QFont(QStringLiteral("Times New Roman"), 28);
    opt.textColor = QColor(18, 18, 22);
    opt.drawBackground = true;
    opt.background = QColor(255, 255, 255);
    opt.padding = 28;
    m_previewImage =
        MathFormulaRenderer::renderPaperSolution({m_current.latex}, answerLatex, opt);
    if (m_previewImage.isNull())
        m_previewImage = MathFormulaRenderer::render(answerLatex, opt);

    if (!m_previewImage.isNull()) {
        const int maxW =
            m_resultPreview->width() > 40 ? m_resultPreview->width() - 12 : 560;
        m_resultPreview->setPixmap(QPixmap::fromImage(m_previewImage.scaled(
            maxW, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }

    m_result.formulaImage = m_previewImage;
    m_result.latex = answerLatex;
    m_result.summary = summaryPlain;
    const float aspect =
        m_previewImage.height() > 0
            ? static_cast<float>(m_previewImage.width()) /
                  static_cast<float>(m_previewImage.height())
            : 2.0f;
    m_result.canvasWidth = std::clamp(280.0f * aspect, 360.0f, 560.0f);
}

void PhysicsSolverDialog::plotDependency()
{
    m_error->clear();
    m_graphInfo->clear();
    m_graphImage = QImage();
    m_graphPreview->clear();

    if (!m_hasCurrent) {
        m_error->setText(QStringLiteral("Select an equation."));
        return;
    }

    const QString yId = m_depY->currentData().toString();
    const QString xId = m_depX->currentData().toString();
    if (yId.isEmpty() || xId.isEmpty()) {
        m_error->setText(QStringLiteral("Pick dependent and independent variables."));
        return;
    }
    if (yId == xId) {
        m_error->setText(QStringLiteral("Y and X must be different variables."));
        return;
    }
    if (!m_current.solve.contains(yId)) {
        m_error->setText(QStringLiteral("No rearrange available for that Y variable."));
        return;
    }

    const double xmin = m_xMin->value();
    const double xmax = m_xMax->value();
    if (!(xmax > xmin)) {
        m_error->setText(QStringLiteral("X max must be greater than X min."));
        return;
    }

    QString err;
    QHash<QString, double> vals = collectKnownValues(&err, yId, xId);
    if (!err.isEmpty()) {
        m_error->setText(err);
        return;
    }

    // All other equation variables (except X,Y) must be known
    for (const auto &v : m_current.variables) {
        if (v.id == yId || v.id == xId)
            continue;
        if (!vals.contains(v.id.toLower())) {
            m_error->setText(
                QStringLiteral("Missing fixed value for %1 (%2)")
                    .arg(v.symbolLatex, v.name));
            return;
        }
    }

    const QString rawFormula = m_current.solve.value(yId);
    // Independent must appear in the formula (after rewrite → x)
    if (!rawFormula.contains(xId, Qt::CaseInsensitive) &&
        xId.compare(QStringLiteral("x"), Qt::CaseInsensitive) != 0) {
        // Still try — some formulas may use aliases; evaluate check below
    }

    const QString plotExpr = rewriteIndepAsX(rawFormula, xId);
    MathExpression expr(plotExpr);
    QHash<QString, double> named = PhysicsCatalog::constantValues();
    for (const auto &v : m_current.variables)
        named.remove(v.id.toLower());
    // Fixed knowns become named constants; x is free
    for (auto it = vals.begin(); it != vals.end(); ++it)
        named.insert(it.key(), it.value());
    expr.setNamedValues(named);

    if (!expr.isValid()) {
        m_error->setText(
            QStringLiteral("Cannot build plot expression: %1").arg(expr.error()));
        return;
    }

    // Dry-run mid-point
    if (!expr.eval(0.5 * (xmin + xmax)) && !expr.eval(xmin) && !expr.eval(xmax)) {
        m_error->setText(
            QStringLiteral(
                "Expression has no real values in this range "
                "(check domain / fixed values).  f(x) = %1")
                .arg(plotExpr));
        return;
    }

    const PhysicsVariable yVar = findVar(yId);
    const PhysicsVariable xVar = findVar(xId);

    MathGraphRenderer::Options gopt;
    gopt.expression = plotExpr;
    gopt.namedValues = named;
    gopt.xMin = xmin;
    gopt.xMax = xmax;
    gopt.autoY = true;
    gopt.samples = m_samples->value();
    gopt.width = 900;
    gopt.height = 560;
    gopt.margin = 56;
    gopt.curveColor = QColor(20, 90, 180);
    gopt.curveWidth = 2.6f;
    gopt.xLabel = xVar.symbolLatex.isEmpty() ? xId : xVar.symbolLatex;
    if (!xVar.unit.isEmpty())
        gopt.xLabel += QStringLiteral(" [") + xVar.unit + QStringLiteral("]");
    gopt.yLabel = yVar.symbolLatex.isEmpty() ? yId : yVar.symbolLatex;
    if (!yVar.unit.isEmpty())
        gopt.yLabel += QStringLiteral(" [") + yVar.unit + QStringLiteral("]");
    gopt.title = QStringLiteral("%1  vs  %2   (%3)")
                     .arg(yVar.symbolLatex, xVar.symbolLatex, m_current.name);

    auto gr = MathGraphRenderer::render(gopt);
    if (!gr.error.isEmpty() || gr.image.isNull()) {
        m_error->setText(gr.error.isEmpty() ? QStringLiteral("Plot failed.")
                                            : gr.error);
        return;
    }

    m_graphImage = gr.image;
    m_result.graphImage = m_graphImage;
    m_result.graphCanvasWidth = 540.0f;

    const int maxW =
        m_graphPreview->width() > 40 ? m_graphPreview->width() - 12 : 640;
    m_graphPreview->setPixmap(QPixmap::fromImage(
        m_graphImage.scaled(maxW, 260, Qt::KeepAspectRatio, Qt::SmoothTransformation)));

    m_graphInfo->setText(
        QStringLiteral("%1(%2) = %3   |  %2 ∈ [%4, %5]  |  %6 points")
            .arg(yVar.symbolLatex, xVar.symbolLatex, plotExpr)
            .arg(xmin, 0, 'g', 6)
            .arg(xmax, 0, 'g', 6)
            .arg(gr.pointCount));

    // Also refresh a compact formula card describing the dependency
    const QString rangeLatex =
        QStringLiteral("%1(%2)\\ \\mathrm{for}\\ %2\\in[%3,%4]")
            .arg(yVar.symbolLatex, xVar.symbolLatex,
                 formatNumberLatex(xmin), formatNumberLatex(xmax));
    MathFormulaRenderer::Options fopt;
    fopt.font = QFont(QStringLiteral("Times New Roman"), 26);
    fopt.textColor = QColor(18, 18, 22);
    fopt.drawBackground = true;
    fopt.background = Qt::white;
    fopt.padding = 24;
    const QImage card = MathFormulaRenderer::renderPaperSolution(
        {m_current.latex}, rangeLatex, fopt);
    if (!card.isNull()) {
        m_previewImage = card;
        m_result.formulaImage = card;
        m_result.latex = rangeLatex;
        m_resultPreview->setPixmap(QPixmap::fromImage(
            card.scaled(m_resultPreview->width() > 40 ? m_resultPreview->width() - 12
                                                      : 480,
                        120, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
    m_result.summary =
        QStringLiteral("%1 vs %2 on [%3, %4]")
            .arg(yVar.symbolLatex, xVar.symbolLatex)
            .arg(xmin, 0, 'g', 4)
            .arg(xmax, 0, 'g', 4);

    // Switch insert preference to graph if still on formula-only
    if (m_insertWhat->currentData().toInt() == static_cast<int>(InsertMode::Formula))
        m_insertWhat->setCurrentIndex(1);
    updateInsertButton();
}

void PhysicsSolverDialog::acceptInsert()
{
    const auto mode =
        static_cast<InsertMode>(m_insertWhat->currentData().toInt());
    m_result.insertMode = mode;

    if (mode == InsertMode::Formula || mode == InsertMode::Both) {
        if (m_result.formulaImage.isNull())
            solveNow();
        if (m_result.formulaImage.isNull()) {
            m_error->setText(QStringLiteral("Solve or plot before inserting a formula."));
            return;
        }
    }
    if (mode == InsertMode::Graph || mode == InsertMode::Both) {
        if (m_result.graphImage.isNull())
            plotDependency();
        if (m_result.graphImage.isNull()) {
            m_error->setText(QStringLiteral("Plot a dependency before inserting a graph."));
            return;
        }
    }
    accept();
}
