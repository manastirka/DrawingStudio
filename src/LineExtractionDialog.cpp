#include "LineExtractionDialog.h"
#include "DXFExporter.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "Layer.h"
#include "LayerManager.h"
#include "CommandManager.h"
#include "Commands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QDebug>
#include <algorithm>

LineExtractionDialog::LineExtractionDialog(const QImage &sourceImage,
                                           DrawingCanvas *canvas,
                                           LayerManager *layerManager,
                                           CommandManager *commandManager,
                                           QWidget *parent)
    : QDialog(parent)
    , m_sourceImage(sourceImage)
    , m_canvas(canvas)
    , m_layerManager(layerManager)
    , m_commandManager(commandManager)
{
    setWindowTitle(QStringLiteral("Extract Lines from Floor Plan"));
    setMinimumSize(960, 640);
    setupUI();

    connect(&m_extractor, &LineExtractor::progressChanged,
            this, &LineExtractionDialog::onProgressChanged);

    applyScaleAwareSliderDefaults();
    updatePreview();
}

void LineExtractionDialog::applyScaleAwareSliderDefaults()
{
    double minLen = 30, gap = 10;
    LineExtractor::suggestScaleDefaults(m_sourceImage.width(), m_sourceImage.height(),
                                        &minLen, &gap);
    m_minLineLengthSlider->setValue(static_cast<int>(std::lround(minLen)));
    m_maxLineGapSlider->setValue(static_cast<int>(std::lround(std::max(gap, 12.0))));
    m_edgeThresholdSlider->setValue(0); // auto ink
    m_houghThresholdSlider->setValue(50);
}

LineExtractor::Parameters LineExtractionDialog::collectParams() const
{
    LineExtractor::Parameters params;
    params.floorPlanMode = m_floorPlanCheck->isChecked();
    const int thr = m_edgeThresholdSlider->value();
    params.edgeThreshold = thr <= 0 ? 50 : thr;
    params.inkThreshold = thr <= 0 ? -1 : thr;
    params.houghThreshold = m_houghThresholdSlider->value();
    params.minLineLength = m_minLineLengthSlider->value();
    params.maxLineGap = m_maxLineGapSlider->value();
    params.angleSnapDegrees =
        m_angleSnapCheck->isChecked() ? m_angleSnapSpin->value() : 0.0;
    params.mergeCollinear = m_mergeCheck->isChecked();
    params.invert = m_invertCheck->isChecked();
    params.deskew = m_deskewCheck->isChecked();
    params.morphCleanup = m_morphCheck->isChecked();
    params.scaleAwareDefaults = false;
    params.duplicateDist = 5.0;
    params.junctionSnap = 6.0;
    params.wallKernelThickness = 1;
    return params;
}

void LineExtractionDialog::onEdgeThresholdChanged(int value)
{
    m_edgeThresholdValue->setText(value <= 0 ? QStringLiteral("auto")
                                             : QString::number(value));
    updatePreview();
}

void LineExtractionDialog::onAutoThreshold()
{
    const int t = LineExtractor::suggestInkThreshold(
        m_sourceImage, m_invertCheck->isChecked());
    m_edgeThresholdSlider->setValue(t);
}

QImage LineExtractionDialog::renderEdgePreview()
{
    const int thr = m_edgeThresholdSlider->value();
    return m_extractor.getEdgePreview(
        m_sourceImage, thr <= 0 ? -1 : thr, m_invertCheck->isChecked(),
        m_morphCheck->isChecked(), m_floorPlanCheck->isChecked());
}

void LineExtractionDialog::setupUI()
{
    auto *mainLayout = new QHBoxLayout(this);

    auto *controlsWidget = new QWidget;
    controlsWidget->setFixedWidth(300);
    auto *controlsLayout = new QVBoxLayout(controlsWidget);

    auto *edgeGroup = new QGroupBox(QStringLiteral("Ink / Edge Detection"));
    auto *edgeLayout = new QFormLayout(edgeGroup);

    m_floorPlanCheck = new QCheckBox(QStringLiteral("Floor-plan mode (walls)"));
    m_floorPlanCheck->setChecked(true);
    m_floorPlanCheck->setToolTip(
        QStringLiteral("Ink binarization + H/V wall filter — best for architectural plans"));
    edgeLayout->addRow(QString(), m_floorPlanCheck);
    connect(m_floorPlanCheck, &QCheckBox::toggled, this, [this](bool) { updatePreview(); });

    m_edgeThresholdSlider = new QSlider(Qt::Horizontal);
    m_edgeThresholdSlider->setRange(0, 255);
    m_edgeThresholdSlider->setValue(0); // 0 = auto Otsu in floor-plan mode
    m_edgeThresholdValue = new QLabel(QStringLiteral("auto"));
    auto *edgeRow = new QHBoxLayout;
    edgeRow->addWidget(m_edgeThresholdSlider);
    edgeRow->addWidget(m_edgeThresholdValue);
    edgeLayout->addRow(QStringLiteral("Ink / Edge Threshold:"), edgeRow);

    m_autoThresholdBtn = new QPushButton(QStringLiteral("Auto Threshold"));
    edgeLayout->addRow(QString(), m_autoThresholdBtn);
    connect(m_autoThresholdBtn, &QPushButton::clicked, this,
            &LineExtractionDialog::onAutoThreshold);

    connect(m_edgeThresholdSlider, &QSlider::valueChanged,
            this, &LineExtractionDialog::onEdgeThresholdChanged);

    controlsLayout->addWidget(edgeGroup);

    auto *lineGroup = new QGroupBox(QStringLiteral("Line Detection"));
    auto *lineLayout = new QFormLayout(lineGroup);

    m_minLineLengthSlider = new QSlider(Qt::Horizontal);
    m_minLineLengthSlider->setRange(10, 800);
    m_minLineLengthSlider->setValue(30);
    m_minLineLengthValue = new QLabel(QStringLiteral("30 px"));
    auto *minLenRow = new QHBoxLayout;
    minLenRow->addWidget(m_minLineLengthSlider);
    minLenRow->addWidget(m_minLineLengthValue);
    lineLayout->addRow(QStringLiteral("Min Line Length:"), minLenRow);
    connect(m_minLineLengthSlider, &QSlider::valueChanged, [this](int v) {
        m_minLineLengthValue->setText(QStringLiteral("%1 px").arg(v));
    });

    m_houghThresholdSlider = new QSlider(Qt::Horizontal);
    m_houghThresholdSlider->setRange(20, 200);
    m_houghThresholdSlider->setValue(70);
    m_houghThresholdValue = new QLabel(QStringLiteral("70"));
    auto *houghRow = new QHBoxLayout;
    houghRow->addWidget(m_houghThresholdSlider);
    houghRow->addWidget(m_houghThresholdValue);
    lineLayout->addRow(QStringLiteral("Hough Threshold:"), houghRow);
    connect(m_houghThresholdSlider, &QSlider::valueChanged, [this](int v) {
        m_houghThresholdValue->setText(QString::number(v));
    });

    m_maxLineGapSlider = new QSlider(Qt::Horizontal);
    m_maxLineGapSlider->setRange(1, 80);
    m_maxLineGapSlider->setValue(10);
    m_maxLineGapValue = new QLabel(QStringLiteral("10 px"));
    auto *gapRow = new QHBoxLayout;
    gapRow->addWidget(m_maxLineGapSlider);
    gapRow->addWidget(m_maxLineGapValue);
    lineLayout->addRow(QStringLiteral("Max Line Gap:"), gapRow);
    connect(m_maxLineGapSlider, &QSlider::valueChanged, [this](int v) {
        m_maxLineGapValue->setText(QStringLiteral("%1 px").arg(v));
    });

    controlsLayout->addWidget(lineGroup);

    auto *postGroup = new QGroupBox(QStringLiteral("Pre / Post-processing"));
    auto *postLayout = new QVBoxLayout(postGroup);

    m_invertCheck = new QCheckBox(QStringLiteral("Invert (light lines on dark)"));
    postLayout->addWidget(m_invertCheck);
    connect(m_invertCheck, &QCheckBox::toggled, this, [this](bool) { updatePreview(); });

    m_deskewCheck = new QCheckBox(QStringLiteral("Auto deskew"));
    m_deskewCheck->setChecked(false); // off by default — false positives break H/V plans
    postLayout->addWidget(m_deskewCheck);

    m_morphCheck = new QCheckBox(QStringLiteral("Clean speckles (morph open)"));
    m_morphCheck->setChecked(true);
    postLayout->addWidget(m_morphCheck);
    connect(m_morphCheck, &QCheckBox::toggled, this, [this](bool) { updatePreview(); });

    m_angleSnapCheck = new QCheckBox(QStringLiteral("Snap to H/V"));
    m_angleSnapCheck->setChecked(true);
    m_angleSnapSpin = new QDoubleSpinBox;
    m_angleSnapSpin->setRange(0.5, 10.0);
    m_angleSnapSpin->setValue(2.0);
    m_angleSnapSpin->setSuffix(QStringLiteral(" deg"));
    auto *snapRow = new QHBoxLayout;
    snapRow->addWidget(m_angleSnapCheck);
    snapRow->addWidget(m_angleSnapSpin);
    postLayout->addLayout(snapRow);

    m_mergeCheck = new QCheckBox(QStringLiteral("Merge collinear segments"));
    m_mergeCheck->setChecked(true);
    postLayout->addWidget(m_mergeCheck);

    controlsLayout->addWidget(postGroup);

    m_detectButton = new QPushButton(QStringLiteral("Detect Lines"));
    m_detectButton->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; padding: 8px; }"));
    controlsLayout->addWidget(m_detectButton);
    connect(m_detectButton, &QPushButton::clicked, this, &LineExtractionDialog::onDetectLines);

    m_lineCountLabel = new QLabel(QStringLiteral("Lines detected: 0"));
    m_lineCountLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #333;"));
    controlsLayout->addWidget(m_lineCountLabel);

    m_qualityTip = new QLabel;
    m_qualityTip->setWordWrap(true);
    m_qualityTip->setStyleSheet(QStringLiteral("color:#b45309; font-size:11px;"));
    controlsLayout->addWidget(m_qualityTip);

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    controlsLayout->addWidget(m_progressBar);

    controlsLayout->addStretch();

    auto *scaleGroup = new QGroupBox(QStringLiteral("Scale / Units"));
    auto *scaleLayout = new QFormLayout(scaleGroup);

    m_scaleSpin = new QDoubleSpinBox;
    m_scaleSpin->setRange(0.01, 1000.0);
    m_scaleSpin->setValue(1.0);
    m_scaleSpin->setDecimals(4);
    scaleLayout->addRow(QStringLiteral("Pixels per unit:"), m_scaleSpin);

    m_unitsCombo = new QComboBox;
    m_unitsCombo->addItem(QStringLiteral("Millimeters"),
                          static_cast<int>(DXFExporter::Units::Millimeters));
    m_unitsCombo->addItem(QStringLiteral("Centimeters"),
                          static_cast<int>(DXFExporter::Units::Centimeters));
    m_unitsCombo->addItem(QStringLiteral("Inches"),
                          static_cast<int>(DXFExporter::Units::Inches));
    scaleLayout->addRow(QStringLiteral("Units:"), m_unitsCombo);

    controlsLayout->addWidget(scaleGroup);

    auto *buttonLayout = new QHBoxLayout;

    m_createButton = new QPushButton(QStringLiteral("Create Lines on New Layer"));
    m_createButton->setEnabled(false);
    connect(m_createButton, &QPushButton::clicked, this, &LineExtractionDialog::onCreateLines);
    buttonLayout->addWidget(m_createButton);

    m_exportButton = new QPushButton(QStringLiteral("Export to DXF"));
    m_exportButton->setEnabled(false);
    connect(m_exportButton, &QPushButton::clicked, this, &LineExtractionDialog::onExportDXF);
    buttonLayout->addWidget(m_exportButton);

    auto *cancelButton = new QPushButton(QStringLiteral("Cancel"));
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    controlsLayout->addLayout(buttonLayout);

    mainLayout->addWidget(controlsWidget);

    m_previewTabs = new QTabWidget;

    auto createScrollPreview = [](QLabel *&label) -> QScrollArea * {
        label = new QLabel;
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumSize(400, 400);
        label->setStyleSheet(QStringLiteral("background: #2a2a2a;"));
        auto *scroll = new QScrollArea;
        scroll->setWidget(label);
        scroll->setWidgetResizable(true);
        return scroll;
    };

    m_previewTabs->addTab(createScrollPreview(m_originalPreview), QStringLiteral("Original"));
    m_previewTabs->addTab(createScrollPreview(m_edgePreview), QStringLiteral("Wall Mask"));
    m_previewTabs->addTab(createScrollPreview(m_linesPreview), QStringLiteral("Detected Lines"));

    mainLayout->addWidget(m_previewTabs, 1);

    QImage scaled =
        m_sourceImage.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_originalPreview->setPixmap(QPixmap::fromImage(scaled));
}

void LineExtractionDialog::updatePreview()
{
    QImage edgeImg = renderEdgePreview();
    QImage scaledEdge =
        edgeImg.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_edgePreview->setPixmap(QPixmap::fromImage(scaledEdge));
}

QImage LineExtractionDialog::renderLinesPreview()
{
    QImage preview = m_sourceImage.convertToFormat(QImage::Format_ARGB32);
    QPainter painter(&preview);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(QColor(220, 40, 40), 2);
    painter.setPen(pen);

    for (const auto &line : m_lines)
        painter.drawLine(line.start, line.end);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 220, 80, 200));
    for (const auto &line : m_lines) {
        painter.drawEllipse(line.start, 3, 3);
        painter.drawEllipse(line.end, 3, 3);
    }

    painter.end();
    return preview;
}

void LineExtractionDialog::onDetectLines()
{
    m_detectButton->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_qualityTip->clear();

    m_lines = m_extractor.extractLines(m_sourceImage, collectParams());

    m_lineCountLabel->setText(
        QStringLiteral("Lines detected: %1").arg(m_lines.size()));
    m_detectButton->setEnabled(true);
    m_progressBar->setVisible(false);

    const bool hasLines = !m_lines.empty();
    m_createButton->setEnabled(hasLines);
    m_exportButton->setEnabled(hasLines);

    if (m_lines.size() < 5) {
        m_qualityTip->setText(
            QStringLiteral("Very few lines — try lower Edge/Hough threshold, "
                           "or toggle Invert."));
    } else if (m_lines.size() > 2000) {
        m_qualityTip->setText(
            QStringLiteral("Very dense result — raise Min Line Length / Hough "
                           "threshold or enable Merge."));
    } else {
        m_qualityTip->clear();
    }

    QImage linesImg = renderLinesPreview();
    QImage scaledLines =
        linesImg.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_linesPreview->setPixmap(QPixmap::fromImage(scaledLines));
    m_previewTabs->setCurrentIndex(2);
}

void LineExtractionDialog::onProgressChanged(int percent)
{
    m_progressBar->setValue(percent);
}

void LineExtractionDialog::onCreateLines()
{
    if (m_lines.empty() || !m_layerManager)
        return;

    const double scale = std::max(0.01, m_scaleSpin->value());

    std::vector<ExtractFloorPlanLinesCommand::Segment> segments;
    segments.reserve(m_lines.size());
    for (const auto &line : m_lines) {
        ExtractFloorPlanLinesCommand::Segment seg;
        seg.start = QVector2D(static_cast<float>(line.start.x() / scale),
                              static_cast<float>(line.start.y() / scale));
        seg.end = QVector2D(static_cast<float>(line.end.x() / scale),
                            static_cast<float>(line.end.y() / scale));
        segments.push_back(seg);
    }

    auto cmd = std::make_unique<ExtractFloorPlanLinesCommand>(
        m_canvas, m_layerManager, QStringLiteral("Floor Plan Lines"),
        std::move(segments), Qt::black, 1.0f);

    if (m_commandManager) {
        m_commandManager->executeCommand(std::move(cmd));
    } else {
        cmd->execute();
    }

    if (m_canvas)
        m_canvas->update();

    QMessageBox::information(
        this, QStringLiteral("Lines Created"),
        QStringLiteral("Created %1 line primitives on layer \"Floor Plan Lines\".\n"
                       "Undo (⌘Z) removes the layer.")
            .arg(m_lines.size()));
    accept();
}

void LineExtractionDialog::onExportDXF()
{
    if (m_lines.empty())
        return;

    QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Lines as DXF"), QString(),
        QStringLiteral("DXF Files (*.dxf)"));
    if (fileName.isEmpty())
        return;

    const double scale = std::max(0.01, m_scaleSpin->value());
    auto units =
        static_cast<DXFExporter::Units>(m_unitsCombo->currentData().toInt());

    std::vector<std::pair<QPointF, QPointF>> linePairs;
    linePairs.reserve(m_lines.size());
    for (const auto &line : m_lines) {
        linePairs.emplace_back(
            QPointF(line.start.x() / scale, line.start.y() / scale),
            QPointF(line.end.x() / scale, line.end.y() / scale));
    }

    DXFExporter exporter;
    if (exporter.exportLines(fileName, linePairs, units)) {
        QMessageBox::information(this, QStringLiteral("DXF Export"),
                                 QStringLiteral("Exported %1 lines to:\n%2")
                                     .arg(m_lines.size())
                                     .arg(fileName));
    } else {
        QMessageBox::warning(this, QStringLiteral("DXF Export"),
                             QStringLiteral("Failed to export DXF file."));
    }
}
