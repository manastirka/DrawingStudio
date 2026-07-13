#include "ToolOptionsBar.h"

#include "ClassicTextTool.h"
#include "CommandManager.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "IconFactory.h"
#include "ImagePrimitive.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QToolButton>
#include <QVariantMap>

// ToolOptionsBar mask popup + presets (refactor E23).

// --- showMaskSettingsPopup ---
void ToolOptionsBar::showMaskSettingsPopup()
{
    // Sync slider values from the currently selected image (if any)
    ImagePrimitive *firstImage = nullptr;
    if (m_host.canvas) {
        for (auto *obj : m_host.canvas->selectedObjects()) {
            if (auto *imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
                firstImage = imgPrim;
                break;
            }
        }
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Mask Settings");
    dlg.setMinimumWidth(320);

    QFormLayout *form = new QFormLayout(&dlg);
    form->setContentsMargins(12, 12, 12, 12);
    form->setSpacing(8);

    QString sliderStyle = R"(
        QSlider::groove:horizontal { background: #34495e; height: 6px; border-radius: 3px; }
        QSlider::handle:horizontal { background: #4a90e2; width: 16px; margin: -5px 0; border-radius: 8px; }
        QSlider::handle:horizontal:hover { background: #5ba0f2; }
    )";

    // Invert Mask
    QCheckBox *invertCheck = new QCheckBox("Invert Mask");
    invertCheck->setChecked(firstImage ? firstImage->isMaskInverted() : m_maskInvertCheck->isChecked());
    form->addRow(invertCheck);

    // Preview Mask
    QCheckBox *overlayCheck = new QCheckBox("Preview Mask");
    overlayCheck->setChecked(firstImage ? firstImage->isMaskOverlayVisible() : m_maskOverlayCheck->isChecked());
    form->addRow(overlayCheck);

    // Feather
    QSlider *featherSlider = new QSlider(Qt::Horizontal);
    featherSlider->setRange(0, 30);
    featherSlider->setValue(firstImage ? firstImage->getMaskFeather() : m_maskFeatherSlider->value());
    featherSlider->setStyleSheet(sliderStyle);
    QLabel *featherVal = new QLabel(QString::number(featherSlider->value()));
    QHBoxLayout *featherRow = new QHBoxLayout();
    featherRow->addWidget(featherSlider);
    featherRow->addWidget(featherVal);
    connect(featherSlider, &QSlider::valueChanged, [featherVal](int v) { featherVal->setText(QString::number(v)); });
    form->addRow("Feather:", featherRow);

    // Blur
    QSlider *blurSlider = new QSlider(Qt::Horizontal);
    blurSlider->setRange(0, 20);
    blurSlider->setValue(firstImage ? firstImage->getMaskBlur() : m_maskBlurSlider->value());
    blurSlider->setStyleSheet(sliderStyle);
    QLabel *blurVal = new QLabel(QString::number(blurSlider->value()));
    QHBoxLayout *blurRow = new QHBoxLayout();
    blurRow->addWidget(blurSlider);
    blurRow->addWidget(blurVal);
    connect(blurSlider, &QSlider::valueChanged, [blurVal](int v) { blurVal->setText(QString::number(v)); });
    form->addRow("Blur:", blurRow);

    // Expand
    QSlider *expandSlider = new QSlider(Qt::Horizontal);
    expandSlider->setRange(-20, 20);
    expandSlider->setValue(firstImage ? firstImage->getMaskExpand() : m_maskExpandSlider->value());
    expandSlider->setStyleSheet(sliderStyle);
    QLabel *expandVal = new QLabel(QString::number(expandSlider->value()));
    QHBoxLayout *expandRow = new QHBoxLayout();
    expandRow->addWidget(expandSlider);
    expandRow->addWidget(expandVal);
    connect(expandSlider, &QSlider::valueChanged, [expandVal](int v) { expandVal->setText(QString::number(v)); });
    form->addRow("Expand:", expandRow);

    // Apply live while dialog is open
    auto applyToSelected = [&]() {
        if (!m_host.canvas) return;
        for (auto *obj : m_host.canvas->selectedObjects()) {
            if (auto *imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
                imgPrim->setMaskFeather(featherSlider->value());
                imgPrim->setMaskBlur(blurSlider->value());
                imgPrim->setMaskExpand(expandSlider->value());
                if (imgPrim->isMaskInverted() != invertCheck->isChecked()) {
                    imgPrim->invertMask();
                }
                imgPrim->setMaskOverlayVisible(overlayCheck->isChecked());
            }
        }
        m_host.canvas->update();
    };

    connect(featherSlider, &QSlider::valueChanged, [&]() { applyToSelected(); });
    connect(blurSlider, &QSlider::valueChanged, [&]() { applyToSelected(); });
    connect(expandSlider, &QSlider::valueChanged, [&]() { applyToSelected(); });
    connect(invertCheck, &QCheckBox::toggled, [&]() { applyToSelected(); });
    connect(overlayCheck, &QCheckBox::toggled, [&]() { applyToSelected(); });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    form->addRow(buttons);

    // Keep internal slider state in sync
    dlg.exec();
    m_maskFeatherSlider->setValue(featherSlider->value());
    m_maskBlurSlider->setValue(blurSlider->value());
    m_maskExpandSlider->setValue(expandSlider->value());
    m_maskInvertCheck->setChecked(invertCheck->isChecked());
    m_maskOverlayCheck->setChecked(overlayCheck->isChecked());
}


// --- updatePresetList ---
void ToolOptionsBar::updatePresetList(DrawingTool tool)
{
    if (!m_presetCombo) {
        return;
    }

    m_presetCombo->blockSignals(true);
    m_presetCombo->clear();
    m_presetCombo->addItem("Default");

    QSettings settings;
    settings.beginGroup("ToolPresets");
    settings.beginGroup(toolKey(tool));
    const QStringList names = settings.childKeys();
    for (const auto &name : names) {
        m_presetCombo->addItem(name);
    }
    settings.endGroup();
    settings.endGroup();
    m_presetCombo->blockSignals(false);
}


// --- applyPreset ---
void ToolOptionsBar::applyPreset(const QString& presetName)
{
    if (!m_presetCombo || presetName == "Default") {
        return;
    }

    QSettings settings;
    settings.beginGroup("ToolPresets");
    settings.beginGroup(toolKey(m_host.canvas ? m_host.canvas->currentTool() : DrawingTool::Select));
    QVariantMap map = settings.value(presetName).toMap();
    settings.endGroup();
    settings.endGroup();

    if (map.contains("setting1") && m_setting1Slider) {
        m_setting1Slider->setValue(map.value("setting1").toInt());
    }
    if (map.contains("setting2") && m_setting2Slider) {
        m_setting2Slider->setValue(map.value("setting2").toInt());
    }
    if (map.contains("setting3") && m_setting3Slider) {
        m_setting3Slider->setValue(map.value("setting3").toInt());
    }
    if (map.contains("bool1") && m_boolSetting1) {
        m_boolSetting1->setChecked(map.value("bool1").toBool());
    }
    if (map.contains("bool2") && m_boolSetting2) {
        m_boolSetting2->setChecked(map.value("bool2").toBool());
    }
    if (map.contains("lineStyle") && m_lineStyleCombo) {
        int style = map.value("lineStyle").toInt();
        for (int i = 0; i < m_lineStyleCombo->count(); ++i) {
            if (m_lineStyleCombo->itemData(i).toInt() == style) {
                m_lineStyleCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    if (map.contains("maskFeather") && m_maskFeatherSlider) {
        m_maskFeatherSlider->setValue(map.value("maskFeather").toInt());
    }
    if (map.contains("maskBlur") && m_maskBlurSlider) {
        m_maskBlurSlider->setValue(map.value("maskBlur").toInt());
    }
    if (map.contains("maskExpand") && m_maskExpandSlider) {
        m_maskExpandSlider->setValue(map.value("maskExpand").toInt());
    }
    if (map.contains("maskInvert") && m_maskInvertCheck) {
        m_maskInvertCheck->setChecked(map.value("maskInvert").toBool());
    }
    if (map.contains("maskOverlay") && m_maskOverlayCheck) {
        m_maskOverlayCheck->setChecked(map.value("maskOverlay").toBool());
    }
}


// --- saveCurrentPreset ---
void ToolOptionsBar::saveCurrentPreset()
{
    if (!m_host.canvas) {
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(m_host.dialogParent, "Save Preset",
                                         "Preset name:", QLineEdit::Normal,
                                         "", &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    QVariantMap map;
    if (m_setting1Slider && m_setting1Slider->isVisible()) {
        map["setting1"] = m_setting1Slider->value();
    }
    if (m_setting2Slider && m_setting2Slider->isVisible()) {
        map["setting2"] = m_setting2Slider->value();
    }
    if (m_setting3Slider && m_setting3Slider->isVisible()) {
        map["setting3"] = m_setting3Slider->value();
    }
    if (m_boolSetting1 && m_boolSetting1->isVisible()) {
        map["bool1"] = m_boolSetting1->isChecked();
    }
    if (m_boolSetting2 && m_boolSetting2->isVisible()) {
        map["bool2"] = m_boolSetting2->isChecked();
    }
    if (m_lineStyleCombo && m_lineStyleCombo->isVisible()) {
        map["lineStyle"] = m_lineStyleCombo->currentData().toInt();
    }
    if (m_maskFeatherSlider && m_maskFeatherSlider->isVisible()) {
        map["maskFeather"] = m_maskFeatherSlider->value();
    }
    if (m_maskBlurSlider && m_maskBlurSlider->isVisible()) {
        map["maskBlur"] = m_maskBlurSlider->value();
    }
    if (m_maskExpandSlider && m_maskExpandSlider->isVisible()) {
        map["maskExpand"] = m_maskExpandSlider->value();
    }
    if (m_maskInvertCheck && m_maskInvertCheck->isVisible()) {
        map["maskInvert"] = m_maskInvertCheck->isChecked();
    }
    if (m_maskOverlayCheck && m_maskOverlayCheck->isVisible()) {
        map["maskOverlay"] = m_maskOverlayCheck->isChecked();
    }

    QSettings settings;
    settings.beginGroup("ToolPresets");
    settings.beginGroup(toolKey(m_host.canvas->currentTool()));
    settings.setValue(name, map);
    settings.endGroup();
    settings.endGroup();

    updatePresetList(m_host.canvas->currentTool());
    m_presetCombo->setCurrentText(name);
}

