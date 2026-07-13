#pragma once

#include "DrawingCanvas.h"

#include <QColor>
#include <QWidget>
#include <functional>

class ClassicTextTool;
class CommandManager;
class ImagePrimitive;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QSlider;
class QToolButton;

/**
 * Photoshop-style tool options bar (sliders, mask, text, presets).
 * Extracted from MainWindow (refactor A10).
 */
class ToolOptionsBar : public QWidget {
    Q_OBJECT
public:
    struct Host {
        DrawingCanvas *canvas = nullptr;
        ClassicTextTool *classicTextTool = nullptr;
        CommandManager *commandManager = nullptr;
        QWidget *dialogParent = nullptr;
        std::function<void()> extractSelectedSubjects;
        std::function<void(const QString &)> setStatusText;
        std::function<void()> refreshSmartSuggestions;
        std::function<QString(DrawingTool)> toolKey;
        std::function<ImagePrimitive *()> imageForDetection;
        std::function<void(ImagePrimitive *)> selectImageForMaskUI;
        std::function<void()> updatePropertyPanel;
    };

    explicit ToolOptionsBar(QWidget *parent = nullptr);

    void setHost(Host host);
    void updateForTool(DrawingTool tool);
    void showMaskSettingsPopup();

    void updatePresetList(DrawingTool tool);
    void applyPreset(const QString &presetName);
    void saveCurrentPreset();

    /** Smart-suggestion chip strip (owned). */
    QWidget *suggestionStrip() const { return m_suggestionStrip; }
    QHBoxLayout *suggestionStripLayout() const { return m_suggestionStripLayout; }

    QPushButton *extractButton() const { return m_extractButton; }

signals:
    void extractSubjectClicked();

private:
    void buildUi();
    void setStatus(const QString &msg);
    QString toolKey(DrawingTool tool) const;

    Host m_host;
    QHBoxLayout *m_layout = nullptr;
    QColor m_selectByColor = Qt::black;

    QLabel *m_setting1Label = nullptr;
    QSlider *m_setting1Slider = nullptr;
    QLabel *m_setting1ValueLabel = nullptr;
    QLabel *m_setting2Label = nullptr;
    QSlider *m_setting2Slider = nullptr;
    QLabel *m_setting2ValueLabel = nullptr;
    QLabel *m_setting3Label = nullptr;
    QSlider *m_setting3Slider = nullptr;
    QLabel *m_setting3ValueLabel = nullptr;
    QCheckBox *m_boolSetting1 = nullptr;
    QCheckBox *m_boolSetting2 = nullptr;
    QFrame *m_optSep1 = nullptr;
    QFrame *m_optSep2 = nullptr;
    QFontComboBox *m_textFontCombo = nullptr;
    QToolButton *m_textUnderlineBtn = nullptr;
    QToolButton *m_textColorBtn = nullptr;
    QLabel *m_selectionModeLabel = nullptr;
    QComboBox *m_selectionModeCombo = nullptr;
    QPushButton *m_selectSimilarButton = nullptr;
    QLabel *m_presetLabel = nullptr;
    QComboBox *m_presetCombo = nullptr;
    QToolButton *m_savePresetButton = nullptr;
    QPushButton *m_maskSettingsButton = nullptr;
    QPushButton *m_extractButton = nullptr;
    QCheckBox *m_maskInvertCheck = nullptr;
    QCheckBox *m_maskOverlayCheck = nullptr;
    QLabel *m_maskFeatherLabel = nullptr;
    QSlider *m_maskFeatherSlider = nullptr;
    QLabel *m_maskFeatherValue = nullptr;
    QLabel *m_maskBlurLabel = nullptr;
    QSlider *m_maskBlurSlider = nullptr;
    QLabel *m_maskBlurValue = nullptr;
    QLabel *m_maskExpandLabel = nullptr;
    QSlider *m_maskExpandSlider = nullptr;
    QLabel *m_maskExpandValue = nullptr;
    QLabel *m_lineStyleLabel = nullptr;
    QComboBox *m_lineStyleCombo = nullptr;
    QLabel *m_selectColorLabel = nullptr;
    QPushButton *m_selectColorButton = nullptr;
    QPushButton *m_pipetteButton = nullptr;
    QWidget *m_suggestionStrip = nullptr;
    QHBoxLayout *m_suggestionStripLayout = nullptr;
};
