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

// ToolOptionsBar::updateForTool (refactor E23).

// --- updateForTool ---
void ToolOptionsBar::updateForTool(DrawingTool tool)
{
    // Leaving Select tool: turn off sticky multi-select so normal clicks replace
    if (tool != DrawingTool::Select && m_host.canvas)
        m_host.canvas->setAdditiveSelection(false);

    // Disconnect all previous connections to avoid conflicts
    disconnect(m_setting1Slider, nullptr, this, nullptr);
    disconnect(m_setting2Slider, nullptr, this, nullptr);
    disconnect(m_setting3Slider, nullptr, this, nullptr);
    disconnect(m_boolSetting1, nullptr, this, nullptr);
    disconnect(m_boolSetting2, nullptr, this, nullptr);
    disconnect(m_lineStyleCombo, nullptr, this, nullptr);
    disconnect(m_selectColorButton, nullptr, this, nullptr);
    disconnect(m_pipetteButton, nullptr, this, nullptr);
    disconnect(m_selectionModeCombo, nullptr, this, nullptr);
    disconnect(m_selectSimilarButton, nullptr, this, nullptr);
    disconnect(m_presetCombo, nullptr, this, nullptr);
    disconnect(m_savePresetButton, nullptr, this, nullptr);
    disconnect(m_maskSettingsButton, nullptr, this, nullptr);
    disconnect(m_textFontCombo, nullptr, this, nullptr);
    disconnect(m_textUnderlineBtn, nullptr, this, nullptr);
    disconnect(m_textColorBtn, nullptr, this, nullptr);

    // Block signals while resetting widget state to prevent stale handlers
    m_setting1Slider->blockSignals(true);
    m_setting2Slider->blockSignals(true);
    m_setting3Slider->blockSignals(true);
    m_boolSetting1->blockSignals(true);
    m_boolSetting2->blockSignals(true);
    m_lineStyleCombo->blockSignals(true);
    m_selectionModeCombo->blockSignals(true);
    m_presetCombo->blockSignals(true);

    // Hide all settings initially
    m_setting1Label->hide();
    m_setting1Slider->hide();
    m_setting1ValueLabel->hide();
    m_setting2Label->hide();
    m_setting2Slider->hide();
    m_setting2ValueLabel->hide();
    m_setting3Label->hide();
    m_setting3Slider->hide();
    m_setting3ValueLabel->hide();
    m_boolSetting1->hide();
    m_boolSetting2->hide();
    m_textFontCombo->hide();
    m_textUnderlineBtn->hide();
    m_textColorBtn->hide();
    m_extractButton->hide();
    m_lineStyleLabel->hide();
    m_lineStyleCombo->hide();
    m_selectColorLabel->hide();
    m_selectColorButton->hide();
    m_pipetteButton->hide();
    m_selectionModeLabel->hide();
    m_selectionModeCombo->hide();
    m_selectSimilarButton->hide();
    m_presetLabel->hide();
    m_presetCombo->hide();
    m_savePresetButton->hide();
    m_maskSettingsButton->hide();
    m_optSep1->hide();
    m_optSep2->hide();

    // NOTE: signals stay blocked through the switch statement and preset setup
    // to prevent setValue/setChecked from triggering handlers during reconfiguration.
    // They are unblocked at the very end of this function.

    // Configure settings based on active tool
    switch (tool) {
        case DrawingTool::Select:
            // Select by color widgets
            m_selectColorLabel->show();
            m_selectColorButton->show();
            m_pipetteButton->show();
            m_selectionModeLabel->show();
            m_selectionModeCombo->show();
            m_selectSimilarButton->show();

            // Multi-select: Shift/⌘ still work; checkbox enables sticky add-mode.
            // Default OFF so the AI composite isn't glued to the original photo.
            m_boolSetting1->setText(QStringLiteral("Multi-select mode"));
            m_boolSetting1->setToolTip(
                QStringLiteral(
                    "When on, clicks add images to the selection (no Shift needed). "
                    "Green mask clicks always add subjects on the same photo."));
            m_boolSetting1->setChecked(false);
            if (m_host.canvas)
                m_host.canvas->setAdditiveSelection(false);
            m_boolSetting1->show();
            connect(m_boolSetting1, &QCheckBox::toggled, this, [this](bool on) {
                if (m_host.canvas)
                    m_host.canvas->setAdditiveSelection(on);
            });

            // Color range tolerance slider — value feeds selectByColor / pipette.
            m_setting1Label->setText("Tolerance:");
            m_setting1Label->show();
            m_setting1Slider->setRange(0, 100);
            m_setting1Slider->setValue(10); // Default tolerance
            m_setting1Slider->setToolTip("Color match tolerance for Select-by-Color and pipette");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText("10");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value));
                // Tolerance is stored and used when selecting by color
            });

            // Update color button swatch appearance
            m_selectColorButton->setStyleSheet(QString(
                "QPushButton { background: %1; border: 1px solid #3a3f47; border-radius: 4px; }"
                "QPushButton:hover { border: 1px solid #2a82da; }").arg(m_selectByColor.name()));

            // Color picker button - use Qt::QueuedConnection to avoid double-trigger
            connect(m_selectColorButton, &QPushButton::clicked, this, [this]() {
                m_selectColorButton->setEnabled(false);
                QColor color = QColorDialog::getColor(m_selectByColor, m_host.dialogParent, "Select Color to Find");
                m_selectColorButton->setEnabled(true);
                if (color.isValid()) {
                    m_selectByColor = color;
                    m_selectColorButton->setStyleSheet(QString(
                        "QPushButton { background: %1; border: 1px solid #3a3f47; border-radius: 4px; }"
                        "QPushButton:hover { border: 1px solid #2a82da; }").arg(color.name()));
                    if (m_host.canvas) {
                        m_host.canvas->selectByColor(color, m_setting1Slider->value());
                    }
                }
            }, Qt::QueuedConnection);

            // Pipette button
            connect(m_pipetteButton, &QPushButton::clicked, this, [this]() {
                if (m_host.canvas) {
                    m_host.canvas->enablePipetteMode(m_setting1Slider->value());
                }
            });

            if (m_host.canvas) {
                m_selectionModeCombo->setCurrentIndex(
                    m_host.canvas->selectionMode() == DrawingCanvas::SelectionMode::Lasso ? 1 : 0);
            }
            connect(m_selectionModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this](int index) {
                        if (m_host.canvas) {
                            auto mode = index == 1 ? DrawingCanvas::SelectionMode::Lasso
                                                   : DrawingCanvas::SelectionMode::Rectangle;
                            m_host.canvas->setSelectionMode(mode);
                        }
                    });

            connect(m_selectSimilarButton, &QPushButton::clicked, this, [this]() {
                if (!m_host.canvas) {
                    return;
                }
                auto selected = m_host.canvas->selectedObjects();
                if (selected.empty()) {
                    return;
                }
                QColor color = selected.front()->color();
                m_host.canvas->selectByColor(color, m_setting1Slider->value());
            });
            break;
            
        case DrawingTool::Move:
            // No specific settings for move tool
            break;
            
        case DrawingTool::Line:
        case DrawingTool::AngleLine:
        case DrawingTool::Curve:
        case DrawingTool::BezierCurve:
        case DrawingTool::Spline:
        case DrawingTool::Polygon:
            // Line Width
            m_setting1Label->setText("Width:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 50);
            m_setting1Slider->setValue(m_host.canvas ? static_cast<int>(m_host.canvas->defaultLineWidth()) : 2);
            m_setting1Slider->setToolTip("Stroke width in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_host.canvas) {
                    m_host.canvas->setDefaultLineWidth(static_cast<float>(value));
                }
            });

            // Line Style selector — preserve current selection, re-apply to canvas.
            m_lineStyleLabel->show();
            m_lineStyleCombo->show();
            if (m_host.canvas) {
                int currentIndex = m_lineStyleCombo->currentIndex();
                Qt::PenStyle style = static_cast<Qt::PenStyle>(m_lineStyleCombo->itemData(currentIndex).toInt());
                m_host.canvas->setDefaultLineStyle(style);
            }
            connect(m_lineStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
                if (m_host.canvas) {
                    Qt::PenStyle style = static_cast<Qt::PenStyle>(m_lineStyleCombo->itemData(index).toInt());
                    m_host.canvas->setDefaultLineStyle(style);
                }
            });

            // Snap to Grid checkbox
            m_boolSetting1->setText("Snap to Grid");
            m_boolSetting1->setChecked(m_host.canvas ? m_host.canvas->isSnapEnabled() : true);
            m_boolSetting1->setToolTip("Snap new points to the grid");
            m_boolSetting1->show();
            connect(m_boolSetting1, &QCheckBox::toggled, this, [this](bool checked) {
                if (m_host.canvas) {
                    m_host.canvas->setSnapEnabled(checked);
                }
            });
            break;
            
        case DrawingTool::Rectangle:
        case DrawingTool::Ellipse:
        case DrawingTool::Circle:
        case DrawingTool::Arc:
            // Line Width
            m_setting1Label->setText("Width:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 50);
            m_setting1Slider->setValue(m_host.canvas ? static_cast<int>(m_host.canvas->defaultLineWidth()) : 2);
            m_setting1Slider->setToolTip("Stroke width in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_host.canvas) {
                    m_host.canvas->setDefaultLineWidth(static_cast<float>(value));
                }
            });

            // Line Style selector — preserve current selection, re-apply to canvas.
            m_lineStyleLabel->show();
            m_lineStyleCombo->show();
            if (m_host.canvas) {
                int currentIndex = m_lineStyleCombo->currentIndex();
                Qt::PenStyle style = static_cast<Qt::PenStyle>(m_lineStyleCombo->itemData(currentIndex).toInt());
                m_host.canvas->setDefaultLineStyle(style);
            }
            connect(m_lineStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
                if (m_host.canvas) {
                    Qt::PenStyle style = static_cast<Qt::PenStyle>(m_lineStyleCombo->itemData(index).toInt());
                    m_host.canvas->setDefaultLineStyle(style);
                }
            });
            break;
            
        case DrawingTool::Brush:
            // Brush Size
            m_setting1Label->setText("Size:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 100);
            m_setting1Slider->setValue(m_host.canvas ? static_cast<int>(m_host.canvas->brushSize()) : 10);
            m_setting1Slider->setToolTip("Brush diameter in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_host.canvas) {
                    m_host.canvas->setBrushSize(value);
                }
            });

            // Hardness
            m_setting2Label->setText("Hardness:");
            m_setting2Label->show();
            m_setting2Slider->setRange(0, 100);
            m_setting2Slider->setValue(m_host.canvas ? static_cast<int>(m_host.canvas->brushHardness() * 100.0f) : 50);
            m_setting2Slider->setToolTip("Edge softness of the brush");
            m_setting2Slider->show();
            m_setting2ValueLabel->setText(QString::number(m_setting2Slider->value()) + "%");
            m_setting2ValueLabel->show();
            connect(m_setting2Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting2ValueLabel->setText(QString::number(value) + "%");
                if (m_host.canvas) {
                    m_host.canvas->setBrushHardness(value / 100.0f);
                }
            });
            break;
            
        case DrawingTool::Blur:
            // Blur Size — reuses the canvas brush size for the blur stroke radius.
            m_setting1Label->setText("Size:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 100);
            m_setting1Slider->setValue(m_host.canvas ? static_cast<int>(m_host.canvas->brushSize()) : 20);
            m_setting1Slider->setToolTip("Blur brush diameter in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_host.canvas) {
                    m_host.canvas->setBrushSize(value);
                }
            });

            // Blur Strength — reuses the canvas brush hardness for blur intensity.
            m_setting2Label->setText("Strength:");
            m_setting2Label->show();
            m_setting2Slider->setRange(0, 100);
            m_setting2Slider->setValue(m_host.canvas ? static_cast<int>(m_host.canvas->brushHardness() * 100.0f) : 50);
            m_setting2Slider->setToolTip("Blur intensity");
            m_setting2Slider->show();
            m_setting2ValueLabel->setText(QString::number(m_setting2Slider->value()) + "%");
            m_setting2ValueLabel->show();
            connect(m_setting2Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting2ValueLabel->setText(QString::number(value) + "%");
                if (m_host.canvas) {
                    m_host.canvas->setBrushHardness(value / 100.0f);
                }
            });
            break;

        case DrawingTool::Eraser:
            // Eraser Size — wired to the canvas eraser radius.
            m_setting1Label->setText("Size:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 100);
            m_setting1Slider->setValue(m_host.canvas ? static_cast<int>(m_host.canvas->eraserSize()) : 20);
            m_setting1Slider->setToolTip("Eraser diameter in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_host.canvas) {
                    m_host.canvas->setEraserSize(static_cast<float>(value));
                }
            });
            break;

        case DrawingTool::Fill:
            // Fill has no numeric options that reach the flood-fill implementation;
            // only the preset picker is shown for this tool.
            break;

        case DrawingTool::Measure:
            // Measure has no wired options; measurements render directly on canvas.
            break;

        case DrawingTool::Text:
        {
            // --- Font Family ---
            m_textFontCombo->show();
            if (m_host.classicTextTool) {
                m_textFontCombo->setCurrentFont(QFont(m_host.classicTextTool->fontFamily()));
            }
            connect(m_textFontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &f) {
                if (m_host.classicTextTool) {
                    m_host.classicTextTool->setFontFamily(f.family());
                }
            });

            // --- Font Size ---
            m_setting1Label->setText("Size:");
            m_setting1Label->show();
            m_setting1Slider->setRange(8, 144);
            m_setting1Slider->setToolTip("Font size in points");
            {
                int initSize = m_host.classicTextTool ? m_host.classicTextTool->fontSize() : 24;
                m_setting1Slider->setValue(initSize);
                m_setting1ValueLabel->setText(QString::number(initSize) + "pt");
            }
            m_setting1Slider->show();
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "pt");
                if (m_host.classicTextTool) {
                    m_host.classicTextTool->setFontSize(value);
                }
            });

            // --- Bold ---
            m_boolSetting1->setText("Bold");
            m_boolSetting1->setChecked(m_host.classicTextTool ? m_host.classicTextTool->isBold() : false);
            m_boolSetting1->setToolTip("Bold");
            m_boolSetting1->show();
            connect(m_boolSetting1, &QCheckBox::toggled, this, [this](bool checked) {
                if (m_host.classicTextTool) {
                    m_host.classicTextTool->setBold(checked);
                }
            });

            // --- Italic ---
            m_boolSetting2->setText("Italic");
            m_boolSetting2->setChecked(m_host.classicTextTool ? m_host.classicTextTool->isItalic() : false);
            m_boolSetting2->setToolTip("Italic");
            m_boolSetting2->show();
            connect(m_boolSetting2, &QCheckBox::toggled, this, [this](bool checked) {
                if (m_host.classicTextTool) {
                    m_host.classicTextTool->setItalic(checked);
                }
            });

            // --- Underline (IconFactory icon button) ---
            m_textUnderlineBtn->setChecked(m_host.classicTextTool ? m_host.classicTextTool->isUnderline() : false);
            m_textUnderlineBtn->show();
            connect(m_textUnderlineBtn, &QToolButton::toggled, this, [this](bool checked) {
                if (m_host.classicTextTool) {
                    m_host.classicTextTool->setUnderline(checked);
                }
            });

            // --- Text Color (IconFactory swatch icon) ---
            {
                QColor initColor = m_host.classicTextTool ? m_host.classicTextTool->textColor() : Qt::white;
                m_textColorBtn->setIcon(IconFactory::textColor(initColor));
            }
            m_textColorBtn->show();
            connect(m_textColorBtn, &QToolButton::clicked, this, [this]() {
                QColor current = m_host.classicTextTool ? m_host.classicTextTool->textColor() : Qt::white;
                QColor color = QColorDialog::getColor(current, m_host.dialogParent, "Text Color");
                if (color.isValid()) {
                    if (m_host.classicTextTool) {
                        m_host.classicTextTool->setTextColor(color);
                    }
                    m_textColorBtn->setIcon(IconFactory::textColor(color));
                }
            });

            // --- Sync toolbar when ClassicTextTool properties change (e.g. after selecting existing text) ---
            if (m_host.classicTextTool) {
                connect(m_host.classicTextTool, &ClassicTextTool::propertyChanged, this, [this]() {
                    if (!m_host.classicTextTool) return;
                    m_textFontCombo->blockSignals(true);
                    m_textFontCombo->setCurrentFont(QFont(m_host.classicTextTool->fontFamily()));
                    m_textFontCombo->blockSignals(false);

                    m_setting1Slider->blockSignals(true);
                    m_setting1Slider->setValue(m_host.classicTextTool->fontSize());
                    m_setting1ValueLabel->setText(QString::number(m_host.classicTextTool->fontSize()) + "pt");
                    m_setting1Slider->blockSignals(false);

                    m_boolSetting1->blockSignals(true);
                    m_boolSetting1->setChecked(m_host.classicTextTool->isBold());
                    m_boolSetting1->blockSignals(false);

                    m_boolSetting2->blockSignals(true);
                    m_boolSetting2->setChecked(m_host.classicTextTool->isItalic());
                    m_boolSetting2->blockSignals(false);

                    m_textUnderlineBtn->blockSignals(true);
                    m_textUnderlineBtn->setChecked(m_host.classicTextTool->isUnderline());
                    m_textUnderlineBtn->blockSignals(false);

                    m_textColorBtn->setIcon(IconFactory::textColor(m_host.classicTextTool->textColor()));
                });
            }
            break;
        }
            
        case DrawingTool::Image:
        {
            // AI subject detection workflow (all controls reach ImagePrimitive).
            m_extractButton->show();

            // Enable Object Detection
            m_boolSetting2->setText("Detect Subjects");
            m_boolSetting2->setChecked(false);
            m_boolSetting2->setToolTip("Automatically detect the main subject in the selected image");
            m_boolSetting2->show();
            connect(m_boolSetting2, &QCheckBox::toggled, this, [this](bool checked) {
                if (!m_host.canvas) return;

                ImagePrimitive *imgPrim =
                    m_host.imageForDetection ? m_host.imageForDetection() : nullptr;
                if (!imgPrim) {
                    if (checked) {
                        m_boolSetting2->setChecked(false);
                        QMessageBox::information(m_host.dialogParent ? m_host.dialogParent : this,
                                                 "No Image Selected",
                            "Please select an image first to detect objects.");
                    }
                    return;
                }

                if (checked) {
                    if (m_host.selectImageForMaskUI)
                        m_host.selectImageForMaskUI(imgPrim);
                    // Detection is asynchronous; success/failure
                    // status arrives via maskDetectionComplete /
                    // maskDetectionFailed. Don't read the candidate
                    // count here (always 0 right after starting).
                    imgPrim->startSubjectDetection();
                    setStatus("Detecting subjects…");
                    m_host.canvas->update();
                } else {
                    m_host.canvas->update();
                    setStatus("Object detection disabled");
                }
            });

            // Mask refinement controls — moved to popup dialog
            m_maskSettingsButton->show();
            connect(m_maskSettingsButton, &QPushButton::clicked, this, &ToolOptionsBar::showMaskSettingsPopup);
            break;
        }
            
        default:
            break;
    }

    // Preset controls — only for drawing tools that benefit from presets.
    // (Pin-to-favorites and recent-size/color widgets were removed.)
    bool showPresets = (tool == DrawingTool::Brush || tool == DrawingTool::Eraser ||
                        tool == DrawingTool::Blur || tool == DrawingTool::Line ||
                        tool == DrawingTool::Curve || tool == DrawingTool::BezierCurve ||
                        tool == DrawingTool::Spline || tool == DrawingTool::Polygon ||
                        tool == DrawingTool::Rectangle || tool == DrawingTool::Ellipse ||
                        tool == DrawingTool::Circle || tool == DrawingTool::Arc);
    if (showPresets) {
        m_presetLabel->show();
        m_presetCombo->show();
        m_savePresetButton->show();

        updatePresetList(tool);
        connect(m_presetCombo, &QComboBox::currentTextChanged, this, &ToolOptionsBar::applyPreset);
        connect(m_savePresetButton, &QToolButton::clicked, this, &ToolOptionsBar::saveCurrentPreset);
    }

    // Position hairline group separators: only shown when they actually divide
    // two visible groups (avoids orphan/leading separators when space collapses).
    bool leftGroup = m_setting1Slider->isVisible() || m_setting2Slider->isVisible() ||
                     m_lineStyleCombo->isVisible() || m_selectColorButton->isVisible() ||
                     m_selectionModeCombo->isVisible() || m_selectSimilarButton->isVisible() ||
                     m_textFontCombo->isVisible();
    bool rightGroup = m_boolSetting1->isVisible() || m_boolSetting2->isVisible() ||
                      m_textUnderlineBtn->isVisible() || m_textColorBtn->isVisible() ||
                      m_extractButton->isVisible() ||
                      m_maskSettingsButton->isVisible();
    m_optSep1->setVisible(leftGroup && rightGroup);
    m_optSep2->setVisible(showPresets && (leftGroup || rightGroup));

    if (m_host.refreshSmartSuggestions) m_host.refreshSmartSuggestions();

    // Unblock signals now that all widgets are configured and connections are set up
    m_setting1Slider->blockSignals(false);
    m_setting2Slider->blockSignals(false);
    m_setting3Slider->blockSignals(false);
    m_boolSetting1->blockSignals(false);
    m_boolSetting2->blockSignals(false);
    m_lineStyleCombo->blockSignals(false);
    m_selectionModeCombo->blockSignals(false);
    m_presetCombo->blockSignals(false);

    if (m_host.updatePropertyPanel)
        m_host.updatePropertyPanel();
}


