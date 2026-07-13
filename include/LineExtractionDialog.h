#pragma once

#include <QDialog>
#include <QImage>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTabWidget>
#include <QProgressBar>
#include <QComboBox>
#include "LineExtractor.h"

class DrawingCanvas;
class LayerManager;
class CommandManager;

class LineExtractionDialog : public QDialog {
    Q_OBJECT

public:
    explicit LineExtractionDialog(const QImage &sourceImage,
                                  DrawingCanvas *canvas,
                                  LayerManager *layerManager,
                                  CommandManager *commandManager = nullptr,
                                  QWidget *parent = nullptr);

    const std::vector<LineExtractor::ExtractedLine> &extractedLines() const { return m_lines; }

private slots:
    void onDetectLines();
    void onCreateLines();
    void onExportDXF();
    void onEdgeThresholdChanged(int value);
    void onAutoThreshold();
    void onProgressChanged(int percent);
    void updatePreview();

private:
    void setupUI();
    void applyScaleAwareSliderDefaults();
    LineExtractor::Parameters collectParams() const;
    QImage renderEdgePreview();
    QImage renderLinesPreview();

    QImage m_sourceImage;
    DrawingCanvas *m_canvas;
    LayerManager *m_layerManager;
    CommandManager *m_commandManager = nullptr;
    LineExtractor m_extractor;
    std::vector<LineExtractor::ExtractedLine> m_lines;

    QSlider *m_edgeThresholdSlider = nullptr;
    QLabel *m_edgeThresholdValue = nullptr;
    QSlider *m_minLineLengthSlider = nullptr;
    QLabel *m_minLineLengthValue = nullptr;
    QSlider *m_houghThresholdSlider = nullptr;
    QLabel *m_houghThresholdValue = nullptr;
    QSlider *m_maxLineGapSlider = nullptr;
    QLabel *m_maxLineGapValue = nullptr;
    QCheckBox *m_angleSnapCheck = nullptr;
    QDoubleSpinBox *m_angleSnapSpin = nullptr;
    QCheckBox *m_mergeCheck = nullptr;
    QCheckBox *m_invertCheck = nullptr;
    QCheckBox *m_deskewCheck = nullptr;
    QCheckBox *m_morphCheck = nullptr;
    QCheckBox *m_floorPlanCheck = nullptr;
    QPushButton *m_autoThresholdBtn = nullptr;

    QTabWidget *m_previewTabs = nullptr;
    QLabel *m_originalPreview = nullptr;
    QLabel *m_edgePreview = nullptr;
    QLabel *m_linesPreview = nullptr;
    QLabel *m_lineCountLabel = nullptr;
    QLabel *m_qualityTip = nullptr;
    QProgressBar *m_progressBar = nullptr;

    QDoubleSpinBox *m_scaleSpin = nullptr;
    QComboBox *m_unitsCombo = nullptr;
    QPushButton *m_detectButton = nullptr;
    QPushButton *m_createButton = nullptr;
    QPushButton *m_exportButton = nullptr;
};
