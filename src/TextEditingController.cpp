#include "TextEditingController.h"

#include "AdvancedTextEditor.h"
#include "ClassicTextTool.h"
#include "CommandManager.h"
#include "Commands.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "MathInsertDialog.h"
#include "PhysicsSolverDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFontDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QUuid>
#include <QVector2D>
#include <memory>
#include <vector>

TextEditingController::TextEditingController(QObject *parent)
    : QObject(parent)
{
}

void TextEditingController::setHost(Host host)
{
    m_host = std::move(host);
}

void TextEditingController::setStatus(const QString &msg)
{
    if (m_host.setStatusText)
        m_host.setStatusText(msg);
}

QWidget *TextEditingController::dialogParent() const
{
    return m_host.dialogParent;
}

void TextEditingController::commitTextPropertyEdits(
    const QString &description,
    const std::function<void(TextPrimitive *)> &mutate)
{
    if (!m_host.canvas || !mutate)
        return;

    std::vector<TextPrimitive *> texts;
    for (auto *obj : m_host.canvas->selectedObjects()) {
        if (auto *tp = dynamic_cast<TextPrimitive *>(obj))
            texts.push_back(tp);
    }
    if (texts.empty()) {
        setStatus(QStringLiteral("Select a text object first."));
        return;
    }

    auto compound = std::make_unique<CompoundCommand>(description);
    for (TextPrimitive *tp : texts) {
        QJsonObject oldState = tp->toJson();
        mutate(tp);
        QJsonObject newState = tp->toJson();
        if (oldState == newState)
            continue;
        auto cmd = std::make_unique<EditTextCommand>(tp, description);
        cmd->storeOldState(oldState);
        cmd->storeNewState(newState);
        cmd->markAlreadyApplied();
        compound->addCommand(std::move(cmd));
    }

    if (!compound->isEmpty() && m_host.commandManager) {
        compound->markAlreadyApplied();
        m_host.commandManager->addCommandWithoutExecuting(std::move(compound));
        if (m_host.syncModifiedFlag) m_host.syncModifiedFlag();
    }
    m_host.canvas->update();
}

void TextEditingController::setTextAlignLeft() {
    commitTextPropertyEdits(QStringLiteral("Align Text Left"), [](TextPrimitive *tp) {
        tp->setAlignment(TextPrimitive::TextAlignment::Left);
    });
    if (m_host.canvas && !m_host.canvas->selectedObjects().empty())
        setStatus(QStringLiteral("Text alignment: Left"));
}

void TextEditingController::setTextAlignCenter() {
    commitTextPropertyEdits(QStringLiteral("Align Text Center"), [](TextPrimitive *tp) {
        tp->setAlignment(TextPrimitive::TextAlignment::Center);
    });
    if (m_host.canvas && !m_host.canvas->selectedObjects().empty())
        setStatus(QStringLiteral("Text alignment: Center"));
}

void TextEditingController::setTextAlignRight() {
    commitTextPropertyEdits(QStringLiteral("Align Text Right"), [](TextPrimitive *tp) {
        tp->setAlignment(TextPrimitive::TextAlignment::Right);
    });
    if (m_host.canvas && !m_host.canvas->selectedObjects().empty())
        setStatus(QStringLiteral("Text alignment: Right"));
}

void TextEditingController::setTextAlignJustify() {
    commitTextPropertyEdits(QStringLiteral("Align Text Justify"), [](TextPrimitive *tp) {
        tp->setAlignment(TextPrimitive::TextAlignment::Justify);
    });
    if (m_host.canvas && !m_host.canvas->selectedObjects().empty())
        setStatus(QStringLiteral("Text alignment: Justify"));
}

void TextEditingController::showFontFamilyDialog() {
    if (!m_host.canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_host.canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        setStatus("Select a text object first.");
        return;
    }

    QFont initial(textPrim->fontFamily(), qMax(1, static_cast<int>(textPrim->fontSize())));
    initial.setBold(textPrim->isBold());
    initial.setItalic(textPrim->isItalic());
    initial.setUnderline(textPrim->isUnderline());

    bool ok = false;
    QFont font = QFontDialog::getFont(&ok, initial, dialogParent());
    if (!ok)
        return;

    const QString family = font.family();
    const qreal size = font.pointSizeF() > 0 ? font.pointSizeF() : textPrim->fontSize();
    const bool bold = font.bold();
    const bool italic = font.italic();
    const bool underline = font.underline();
    commitTextPropertyEdits(QStringLiteral("Change Font"), [=](TextPrimitive *tp) {
        tp->setFontFamily(family);
        tp->setFontSize(size);
        tp->setBold(bold);
        tp->setItalic(italic);
        tp->setUnderline(underline);
    });
    setStatus("Font applied: " + family);
}

void TextEditingController::showLetterSpacingDialog() {
    if (!m_host.canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_host.canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        setStatus("Select a text object first.");
        return;
    }

    bool ok = false;
    double spacing = QInputDialog::getDouble(dialogParent(), "Letter Spacing",
        "Extra spacing between letters (px):", textPrim->letterSpacing(), -10.0, 50.0, 1, &ok);
    if (!ok)
        return;
    commitTextPropertyEdits(QStringLiteral("Letter Spacing"), [spacing](TextPrimitive *tp) {
        tp->setLetterSpacing(spacing);
    });
    setStatus(QString("Letter spacing: %1px").arg(spacing));
}

void TextEditingController::showLineSpacingDialog() {
    if (!m_host.canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_host.canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        setStatus("Select a text object first.");
        return;
    }

    bool ok = false;
    double spacing = QInputDialog::getDouble(dialogParent(), "Line Spacing",
        "Line spacing multiplier (1.0 = normal):", textPrim->lineSpacing(), 0.5, 5.0, 2, &ok);
    if (!ok)
        return;
    commitTextPropertyEdits(QStringLiteral("Line Spacing"), [spacing](TextPrimitive *tp) {
        tp->setLineSpacing(spacing);
    });
    setStatus(QString("Line spacing: %1").arg(spacing));
}

void TextEditingController::showTrackingDialog() {
    if (!m_host.canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_host.canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        setStatus("Select a text object first.");
        return;
    }

    bool ok = false;
    double tracking = QInputDialog::getDouble(dialogParent(), "Tracking",
        "Tracking (extra letter spacing, px):", textPrim->letterSpacing(), -10.0, 50.0, 1, &ok);
    if (!ok)
        return;
    commitTextPropertyEdits(QStringLiteral("Tracking"), [tracking](TextPrimitive *tp) {
        tp->setLetterSpacing(tracking);
    });
    setStatus(QString("Tracking: %1px").arg(tracking));
}

void TextEditingController::showShadowDialog() {
    if (!m_host.canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_host.canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        setStatus("Select a text object first.");
        return;
    }

    QDialog dialog(dialogParent());
    dialog.setWindowTitle("Text Shadow");
    QFormLayout *form = new QFormLayout(&dialog);

    QCheckBox *enabled = new QCheckBox(); enabled->setChecked(textPrim->shadowEnabled());
    QDoubleSpinBox *offsetX = new QDoubleSpinBox(); offsetX->setRange(-50, 50); offsetX->setValue(textPrim->shadowOffsetX());
    QDoubleSpinBox *offsetY = new QDoubleSpinBox(); offsetY->setRange(-50, 50); offsetY->setValue(textPrim->shadowOffsetY());
    QDoubleSpinBox *blur = new QDoubleSpinBox(); blur->setRange(0, 50); blur->setValue(textPrim->shadowBlur());
    QPushButton *colorBtn = new QPushButton();
    QColor shadowColor = textPrim->shadowColor();
    colorBtn->setFixedSize(48, 24);
    colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(shadowColor.name()));
    connect(colorBtn, &QPushButton::clicked, &dialog, [colorBtn, &shadowColor, &dialog]() {
        QColor c = QColorDialog::getColor(shadowColor, &dialog, "Shadow Color");
        if (c.isValid()) {
            shadowColor = c;
            colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(c.name()));
        }
    });

    form->addRow("Enabled:", enabled);
    form->addRow("Color:", colorBtn);
    form->addRow("Offset X:", offsetX);
    form->addRow("Offset Y:", offsetY);
    form->addRow("Blur:", blur);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        const bool on = enabled->isChecked();
        const double ox = offsetX->value();
        const double oy = offsetY->value();
        const double bl = blur->value();
        const QColor col = shadowColor;
        commitTextPropertyEdits(QStringLiteral("Text Shadow"), [=](TextPrimitive *tp) {
            tp->setShadowEnabled(on);
            tp->setShadowColor(col);
            tp->setShadowOffsetX(ox);
            tp->setShadowOffsetY(oy);
            tp->setShadowBlur(bl);
        });
    }
}

void TextEditingController::showStrokeDialog() {
    if (!m_host.canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_host.canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        setStatus("Select a text object first.");
        return;
    }

    QDialog dialog(dialogParent());
    dialog.setWindowTitle("Text Stroke");
    QFormLayout *form = new QFormLayout(&dialog);

    QCheckBox *enabled = new QCheckBox(); enabled->setChecked(textPrim->strokeEnabled());
    QDoubleSpinBox *width = new QDoubleSpinBox(); width->setRange(0.1, 20); width->setValue(textPrim->strokeWidth());
    QPushButton *colorBtn = new QPushButton();
    QColor strokeColor = textPrim->strokeColor();
    colorBtn->setFixedSize(48, 24);
    colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(strokeColor.name()));
    connect(colorBtn, &QPushButton::clicked, &dialog, [colorBtn, &strokeColor, &dialog]() {
        QColor c = QColorDialog::getColor(strokeColor, &dialog, "Stroke Color");
        if (c.isValid()) {
            strokeColor = c;
            colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(c.name()));
        }
    });

    form->addRow("Enabled:", enabled);
    form->addRow("Color:", colorBtn);
    form->addRow("Width:", width);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        const bool on = enabled->isChecked();
        const double w = width->value();
        const QColor col = strokeColor;
        commitTextPropertyEdits(QStringLiteral("Text Stroke"), [=](TextPrimitive *tp) {
            tp->setStrokeEnabled(on);
            tp->setStrokeColor(col);
            tp->setStrokeWidth(w);
        });
    }
}

void TextEditingController::showGradientDialog() {
    if (!m_host.canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_host.canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        setStatus("Select a text object first.");
        return;
    }

    QDialog dialog(dialogParent());
    dialog.setWindowTitle("Text Gradient");
    QFormLayout *form = new QFormLayout(&dialog);

    QCheckBox *enabled = new QCheckBox(); enabled->setChecked(textPrim->gradientEnabled());
    QDoubleSpinBox *angle = new QDoubleSpinBox(); angle->setRange(0, 360); angle->setValue(textPrim->gradientAngle());
    QPushButton *startBtn = new QPushButton();
    QPushButton *endBtn = new QPushButton();
    QColor startColor = textPrim->gradientStartColor();
    QColor endColor = textPrim->gradientEndColor();
    startBtn->setFixedSize(48, 24);
    endBtn->setFixedSize(48, 24);
    startBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(startColor.name()));
    endBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(endColor.name()));
    connect(startBtn, &QPushButton::clicked, &dialog, [startBtn, &startColor, &dialog]() {
        QColor c = QColorDialog::getColor(startColor, &dialog, "Gradient Start");
        if (c.isValid()) {
            startColor = c;
            startBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(c.name()));
        }
    });
    connect(endBtn, &QPushButton::clicked, &dialog, [endBtn, &endColor, &dialog]() {
        QColor c = QColorDialog::getColor(endColor, &dialog, "Gradient End");
        if (c.isValid()) {
            endColor = c;
            endBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(c.name()));
        }
    });

    form->addRow("Enabled:", enabled);
    form->addRow("Start color:", startBtn);
    form->addRow("End color:", endBtn);
    form->addRow("Angle:", angle);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        const bool on = enabled->isChecked();
        const double ang = angle->value();
        const QColor sc = startColor;
        const QColor ec = endColor;
        commitTextPropertyEdits(QStringLiteral("Text Gradient"), [=](TextPrimitive *tp) {
            tp->setGradientEnabled(on);
            tp->setGradientStartColor(sc);
            tp->setGradientEndColor(ec);
            tp->setGradientAngle(ang);
        });
    }
}

void TextEditingController::showTextBoxDialog() {
    if (!m_host.canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_host.canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        setStatus("Select a text object first.");
        return;
    }

    QDialog dialog(dialogParent());
    dialog.setWindowTitle("Text Box Settings");
    QFormLayout *form = new QFormLayout(&dialog);

    QDoubleSpinBox *boxWidth = new QDoubleSpinBox(); boxWidth->setRange(0, 2000); boxWidth->setValue(textPrim->textBoxWidth());
    QDoubleSpinBox *boxHeight = new QDoubleSpinBox(); boxHeight->setRange(0, 2000); boxHeight->setValue(textPrim->textBoxHeight());

    form->addRow("Width (0=auto):", boxWidth);
    form->addRow("Height (0=auto):", boxHeight);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        const double w = boxWidth->value();
        const double h = boxHeight->value();
        commitTextPropertyEdits(QStringLiteral("Text Box"), [=](TextPrimitive *tp) {
            tp->setTextBoxWidth(w);
            tp->setTextBoxHeight(h);
        });
    }
}

void TextEditingController::showAdvancedTextEditor() {
    if (!m_host.canvas)
        return;

    // Commit any in-place ClassicTextTool edit first — it hides the
    // canvas primitive and shows an overlay that would mask ATE updates.
    if (m_host.classicTextTool && m_host.classicTextTool->isEditing())
        m_host.classicTextTool->finishEditing();

    TextPrimitive *textPrim = nullptr;
    for (auto *obj : m_host.canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive *>(obj)))
            break;
    }
    if (!textPrim) {
        setStatus(QStringLiteral("Select a text object first."));
        return;
    }

    if (!m_advancedTextEditor) {
        m_advancedTextEditor = new AdvancedTextEditor(dialogParent());
        connect(m_advancedTextEditor, &AdvancedTextEditor::canvasNeedsUpdate,
                this, [this]() {
                    if (m_host.canvas) {
                        m_host.canvas->update();
                        m_host.canvas->repaint();
                    }
                });
        connect(m_advancedTextEditor, &QDialog::accepted, this, [this]() {
            TextPrimitive *prim = m_advancedTextEditor
                                      ? m_advancedTextEditor->currentPrimitive()
                                      : nullptr;
            if (!prim || !m_host.commandManager || !m_advancedTextEditor)
                return;
            auto cmd = std::make_unique<EditTextCommand>(
                prim, QStringLiteral("Advanced Text Edit"));
            cmd->storeOldState(m_advancedTextEditor->editSnapshot());
            cmd->storeNewState(prim->toJson());
            cmd->markAlreadyApplied();
            m_host.commandManager->addCommandWithoutExecuting(std::move(cmd));
            if (m_host.syncModifiedFlag) m_host.syncModifiedFlag();
            if (m_host.canvas) {
                m_host.canvas->update();
                m_host.canvas->repaint();
            }
            setStatus(QStringLiteral("Text updated"));
        });
    }

    m_advancedTextEditor->setCanvas(m_host.canvas);
    textPrim->setVisible(true);
    m_advancedTextEditor->setTextPrimitive(textPrim);
    m_advancedTextEditor->show();
    m_advancedTextEditor->raise();
    m_advancedTextEditor->activateWindow();
}

void TextEditingController::addAutoShadow() {
    auto selected = m_host.canvas->selectedObjects();
    for (auto* obj : selected) {
        obj->setShadowEnabled(true);
        obj->setShadowBlur(10);
        obj->setShadowOffset(5, 5);
    }
    m_host.canvas->update();
}

void TextEditingController::insertMathFormula()
{
    if (!m_host.canvas)
        return;

    MathInsertDialog dlg(MathInsertDialog::StartTab::Formula, dialogParent());
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto result = dlg.result();
    if (result.image.isNull()) {
        QMessageBox::warning(dialogParent(), QStringLiteral("Insert Math"),
                             QStringLiteral("Nothing to insert."));
        return;
    }

    const float targetW = result.canvasWidth > 0 ? result.canvasWidth : 400.0f;
    const float aspect =
        result.image.height() > 0
            ? static_cast<float>(result.image.width()) /
                  static_cast<float>(result.image.height())
            : 1.0f;
    const float targetH = targetW / std::max(0.01f, aspect);
    const QVector2D center = m_host.canvas->viewCenter();
    const QVector2D pos(center.x() - targetW * 0.5f, center.y() - targetH * 0.5f);
    const QVector2D size(targetW, targetH);

    auto imagePrimitive =
        std::make_unique<ImagePrimitive>(result.image, pos, size);
    imagePrimitive->setVisible(true);
    imagePrimitive->setSelected(true);
    const QUuid id = imagePrimitive->id();

    if (m_host.commandManager) {
        auto cmd =
            std::make_unique<ImportImageCommand>(m_host.canvas, std::move(imagePrimitive));
        m_host.commandManager->executeCommand(std::move(cmd));
    } else {
        m_host.canvas->addPrimitive(std::move(imagePrimitive));
    }

    m_host.canvas->clearSelection();
    m_host.canvas->selectPrimitiveById(id);
    m_host.canvas->setCurrentTool(DrawingTool::Select);
    m_host.canvas->update();

    const QString kind =
        result.mode == MathInsertDialog::Mode::Graph
            ? QStringLiteral("Graph")
            : QStringLiteral("Formula");
    setStatus(
            QStringLiteral("%1 inserted: %2")
                .arg(kind, result.expression.left(80)));
    if (m_host.showStatusMessage) m_host.showStatusMessage(
        QStringLiteral("%1 placed on canvas — move/resize with Select tool")
            .arg(kind),
        4000);
}

void TextEditingController::insertMathGraph()
{
    if (!m_host.canvas)
        return;

    MathInsertDialog dlg(MathInsertDialog::StartTab::Graph, dialogParent());
    if (dlg.exec() != QDialog::Accepted)
        return;
    // Reuse placement path
    const auto result = dlg.result();
    if (result.image.isNull()) {
        QMessageBox::warning(dialogParent(), QStringLiteral("Insert Graph"),
                             QStringLiteral("Nothing to insert."));
        return;
    }

    const float targetW = result.canvasWidth > 0 ? result.canvasWidth : 480.0f;
    const float aspect =
        result.image.height() > 0
            ? static_cast<float>(result.image.width()) /
                  static_cast<float>(result.image.height())
            : 1.0f;
    const float targetH = targetW / std::max(0.01f, aspect);
    const QVector2D center = m_host.canvas->viewCenter();
    const QVector2D pos(center.x() - targetW * 0.5f, center.y() - targetH * 0.5f);

    auto imagePrimitive =
        std::make_unique<ImagePrimitive>(result.image, pos, QVector2D(targetW, targetH));
    imagePrimitive->setVisible(true);
    const QUuid id = imagePrimitive->id();
    if (m_host.commandManager) {
        m_host.commandManager->executeCommand(
            std::make_unique<ImportImageCommand>(m_host.canvas, std::move(imagePrimitive)));
    } else {
        m_host.canvas->addPrimitive(std::move(imagePrimitive));
    }
    m_host.canvas->clearSelection();
    m_host.canvas->selectPrimitiveById(id);
    m_host.canvas->setCurrentTool(DrawingTool::Select);
    m_host.canvas->update();
    setStatus(
            QStringLiteral("Graph inserted: y = %1").arg(result.expression.left(80)));
}

void TextEditingController::showMathTutorial()
{
    MathInsertDialog dlg(MathInsertDialog::StartTab::Tutorial, dialogParent());
    dlg.exec(); // Close only — no canvas insert
}

void TextEditingController::insertPhysicsSolver()
{
    if (!m_host.canvas)
        return;

    PhysicsSolverDialog dlg(dialogParent());
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto result = dlg.result();

    const auto placeImage = [&](const QImage &img, float targetW,
                                const QVector2D &offset) -> QUuid {
        if (img.isNull() || targetW <= 0)
            return {};
        const float aspect =
            img.height() > 0
                ? static_cast<float>(img.width()) / static_cast<float>(img.height())
                : 1.0f;
        const float targetH = targetW / std::max(0.01f, aspect);
        const QVector2D center = m_host.canvas->viewCenter() + offset;
        const QVector2D pos(center.x() - targetW * 0.5f, center.y() - targetH * 0.5f);

        auto imagePrimitive =
            std::make_unique<ImagePrimitive>(img, pos, QVector2D(targetW, targetH));
        imagePrimitive->setVisible(true);
        imagePrimitive->setSelected(true);
        const QUuid id = imagePrimitive->id();
        if (m_host.commandManager) {
            m_host.commandManager->executeCommand(
                std::make_unique<ImportImageCommand>(m_host.canvas, std::move(imagePrimitive)));
        } else {
            m_host.canvas->addPrimitive(std::move(imagePrimitive));
        }
        return id;
    };

    using Mode = PhysicsSolverDialog::InsertMode;
    QUuid lastId;
    const bool wantFormula =
        result.insertMode == Mode::Formula || result.insertMode == Mode::Both;
    const bool wantGraph =
        result.insertMode == Mode::Graph || result.insertMode == Mode::Both;

    if (wantFormula) {
        if (result.formulaImage.isNull()) {
            QMessageBox::warning(dialogParent(), QStringLiteral("Physics Solver"),
                                 QStringLiteral("No formula to insert."));
            return;
        }
        lastId = placeImage(result.formulaImage,
                            result.canvasWidth > 0 ? result.canvasWidth : 440.0f,
                            wantGraph ? QVector2D(0, -180) : QVector2D(0, 0));
    }
    if (wantGraph) {
        if (result.graphImage.isNull()) {
            QMessageBox::warning(dialogParent(), QStringLiteral("Physics Solver"),
                                 QStringLiteral("No graph to insert."));
            return;
        }
        lastId = placeImage(result.graphImage,
                            result.graphCanvasWidth > 0 ? result.graphCanvasWidth
                                                       : 520.0f,
                            wantFormula ? QVector2D(0, 200) : QVector2D(0, 0));
    }

    if (lastId.isNull()) {
        QMessageBox::warning(dialogParent(), QStringLiteral("Physics Solver"),
                             QStringLiteral("Nothing to insert."));
        return;
    }

    m_host.canvas->clearSelection();
    m_host.canvas->selectPrimitiveById(lastId);
    m_host.canvas->setCurrentTool(DrawingTool::Select);
    m_host.canvas->update();

    setStatus(
            QStringLiteral("Physics result inserted: %1").arg(result.summary));
    if (m_host.showStatusMessage) m_host.showStatusMessage(
        wantGraph && wantFormula
            ? QStringLiteral("Formula + dependency graph placed on canvas")
            : (wantGraph ? QStringLiteral("Dependency graph placed on canvas")
                         : QStringLiteral("Physics formula placed on canvas")),
        4000);
}

