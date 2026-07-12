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

class LineExtractionDialog : public QDialog {
    Q_OBJECT

public:
    explicit LineExtractionDialog(const QImage &sourceImage,
                                  DrawingCanvas *canvas,
                                  LayerManager *layerManager,
                                  QWidget *parent = nullptr);

    const std::vector<LineExtractor::ExtractedLine> &extractedLines() const { return m_lines; }

private slots:
    void onDetectLines();
    void onCreateLines();
    void onExportDXF();
    void onEdgeThresholdChanged(int value);
    void onProgressChanged(int percent);
    void updatePreview();

private:
    void setupUI();
    QImage renderEdgePreview();
    QImage renderLinesPreview();

    QImage m_sourceImage;
    DrawingCanvas *m_canvas;
    LayerManager *m_layerManager;
    LineExtractor m_extractor;
    std::vector<LineExtractor::ExtractedLine> m_lines;

    // Parameter controls
    QSlider *m_edgeThresholdSlider;
    QLabel *m_edgeThresholdValue;
    QSlider *m_minLineLengthSlider;
    QLabel *m_minLineLengthValue;
    QSlider *m_houghThresholdSlider;
    QLabel *m_houghThresholdValue;
    QSlider *m_maxLineGapSlider;
    QLabel *m_maxLineGapValue;
    QCheckBox *m_angleSnapCheck;
    QDoubleSpinBox *m_angleSnapSpin;
    QCheckBox *m_mergeCheck;

    // Preview
    QTabWidget *m_previewTabs;
    QLabel *m_originalPreview;
    QLabel *m_edgePreview;
    QLabel *m_linesPreview;
    QLabel *m_lineCountLabel;
    QProgressBar *m_progressBar;

    // Bottom controls
    QDoubleSpinBox *m_scaleSpin;
    QComboBox *m_unitsCombo;
    QPushButton *m_detectButton;
    QPushButton *m_createButton;
    QPushButton *m_exportButton;
};
