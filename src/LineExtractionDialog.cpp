#include "LineExtractionDialog.h"
#include "DXFExporter.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "Layer.h"
#include "LayerManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QDebug>

LineExtractionDialog::LineExtractionDialog(const QImage &sourceImage,
                                           DrawingCanvas *canvas,
                                           LayerManager *layerManager,
                                           QWidget *parent)
    : QDialog(parent)
    , m_sourceImage(sourceImage)
    , m_canvas(canvas)
    , m_layerManager(layerManager)
{
    setWindowTitle("Extract Lines from Building Plan");
    setMinimumSize(900, 600);
    setupUI();

    connect(&m_extractor, &LineExtractor::progressChanged,
            this, &LineExtractionDialog::onProgressChanged);

    // Show initial edge preview
    updatePreview();
}

void LineExtractionDialog::setupUI()
{
    auto *mainLayout = new QHBoxLayout(this);

    // Left side: controls
    auto *controlsWidget = new QWidget;
    controlsWidget->setFixedWidth(280);
    auto *controlsLayout = new QVBoxLayout(controlsWidget);

    // Edge detection group
    auto *edgeGroup = new QGroupBox("Edge Detection");
    auto *edgeLayout = new QFormLayout(edgeGroup);

    m_edgeThresholdSlider = new QSlider(Qt::Horizontal);
    m_edgeThresholdSlider->setRange(10, 255);
    m_edgeThresholdSlider->setValue(50);
    m_edgeThresholdValue = new QLabel("50");
    auto *edgeRow = new QHBoxLayout;
    edgeRow->addWidget(m_edgeThresholdSlider);
    edgeRow->addWidget(m_edgeThresholdValue);
    edgeLayout->addRow("Edge Threshold:", edgeRow);

    connect(m_edgeThresholdSlider, &QSlider::valueChanged,
            this, &LineExtractionDialog::onEdgeThresholdChanged);

    controlsLayout->addWidget(edgeGroup);

    // Line detection group
    auto *lineGroup = new QGroupBox("Line Detection");
    auto *lineLayout = new QFormLayout(lineGroup);

    m_minLineLengthSlider = new QSlider(Qt::Horizontal);
    m_minLineLengthSlider->setRange(10, 500);
    m_minLineLengthSlider->setValue(30);
    m_minLineLengthValue = new QLabel("30 px");
    auto *minLenRow = new QHBoxLayout;
    minLenRow->addWidget(m_minLineLengthSlider);
    minLenRow->addWidget(m_minLineLengthValue);
    lineLayout->addRow("Min Line Length:", minLenRow);

    connect(m_minLineLengthSlider, &QSlider::valueChanged, [this](int v) {
        m_minLineLengthValue->setText(QString("%1 px").arg(v));
    });

    m_houghThresholdSlider = new QSlider(Qt::Horizontal);
    m_houghThresholdSlider->setRange(20, 200);
    m_houghThresholdSlider->setValue(80);
    m_houghThresholdValue = new QLabel("80");
    auto *houghRow = new QHBoxLayout;
    houghRow->addWidget(m_houghThresholdSlider);
    houghRow->addWidget(m_houghThresholdValue);
    lineLayout->addRow("Hough Threshold:", houghRow);

    connect(m_houghThresholdSlider, &QSlider::valueChanged, [this](int v) {
        m_houghThresholdValue->setText(QString::number(v));
    });

    m_maxLineGapSlider = new QSlider(Qt::Horizontal);
    m_maxLineGapSlider->setRange(1, 50);
    m_maxLineGapSlider->setValue(10);
    m_maxLineGapValue = new QLabel("10 px");
    auto *gapRow = new QHBoxLayout;
    gapRow->addWidget(m_maxLineGapSlider);
    gapRow->addWidget(m_maxLineGapValue);
    lineLayout->addRow("Max Line Gap:", gapRow);

    connect(m_maxLineGapSlider, &QSlider::valueChanged, [this](int v) {
        m_maxLineGapValue->setText(QString("%1 px").arg(v));
    });

    controlsLayout->addWidget(lineGroup);

    // Post-processing group
    auto *postGroup = new QGroupBox("Post-processing");
    auto *postLayout = new QVBoxLayout(postGroup);

    m_angleSnapCheck = new QCheckBox("Snap to H/V");
    m_angleSnapCheck->setChecked(true);
    m_angleSnapSpin = new QDoubleSpinBox;
    m_angleSnapSpin->setRange(0.5, 10.0);
    m_angleSnapSpin->setValue(2.0);
    m_angleSnapSpin->setSuffix(" deg");
    auto *snapRow = new QHBoxLayout;
    snapRow->addWidget(m_angleSnapCheck);
    snapRow->addWidget(m_angleSnapSpin);
    postLayout->addLayout(snapRow);

    m_mergeCheck = new QCheckBox("Merge collinear segments");
    m_mergeCheck->setChecked(true);
    postLayout->addWidget(m_mergeCheck);

    controlsLayout->addWidget(postGroup);

    // Detect button
    m_detectButton = new QPushButton("Detect Lines");
    m_detectButton->setStyleSheet("QPushButton { font-weight: bold; padding: 8px; }");
    controlsLayout->addWidget(m_detectButton);
    connect(m_detectButton, &QPushButton::clicked, this, &LineExtractionDialog::onDetectLines);

    // Results
    m_lineCountLabel = new QLabel("Lines detected: 0");
    m_lineCountLabel->setStyleSheet("font-weight: bold; color: #333;");
    controlsLayout->addWidget(m_lineCountLabel);

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    controlsLayout->addWidget(m_progressBar);

    controlsLayout->addStretch();

    // Scale group
    auto *scaleGroup = new QGroupBox("Scale / Units");
    auto *scaleLayout = new QFormLayout(scaleGroup);

    m_scaleSpin = new QDoubleSpinBox;
    m_scaleSpin->setRange(0.01, 1000.0);
    m_scaleSpin->setValue(1.0);
    m_scaleSpin->setDecimals(4);
    scaleLayout->addRow("Pixels per unit:", m_scaleSpin);

    m_unitsCombo = new QComboBox;
    m_unitsCombo->addItem("Millimeters", static_cast<int>(DXFExporter::Units::Millimeters));
    m_unitsCombo->addItem("Centimeters", static_cast<int>(DXFExporter::Units::Centimeters));
    m_unitsCombo->addItem("Inches", static_cast<int>(DXFExporter::Units::Inches));
    scaleLayout->addRow("Units:", m_unitsCombo);

    controlsLayout->addWidget(scaleGroup);

    // Action buttons
    auto *buttonLayout = new QHBoxLayout;

    m_createButton = new QPushButton("Create Lines on New Layer");
    m_createButton->setEnabled(false);
    connect(m_createButton, &QPushButton::clicked, this, &LineExtractionDialog::onCreateLines);
    buttonLayout->addWidget(m_createButton);

    m_exportButton = new QPushButton("Export to DXF");
    m_exportButton->setEnabled(false);
    connect(m_exportButton, &QPushButton::clicked, this, &LineExtractionDialog::onExportDXF);
    buttonLayout->addWidget(m_exportButton);

    auto *cancelButton = new QPushButton("Cancel");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    controlsLayout->addLayout(buttonLayout);

    mainLayout->addWidget(controlsWidget);

    // Right side: preview
    m_previewTabs = new QTabWidget;

    auto createScrollPreview = [](QLabel *&label) -> QScrollArea * {
        label = new QLabel;
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumSize(400, 400);
        label->setStyleSheet("background: #2a2a2a;");
        auto *scroll = new QScrollArea;
        scroll->setWidget(label);
        scroll->setWidgetResizable(true);
        return scroll;
    };

    m_previewTabs->addTab(createScrollPreview(m_originalPreview), "Original");
    m_previewTabs->addTab(createScrollPreview(m_edgePreview), "Edge Detection");
    m_previewTabs->addTab(createScrollPreview(m_linesPreview), "Detected Lines");

    mainLayout->addWidget(m_previewTabs, 1);

    // Show original image
    QImage scaled = m_sourceImage.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_originalPreview->setPixmap(QPixmap::fromImage(scaled));
}

void LineExtractionDialog::onEdgeThresholdChanged(int value)
{
    m_edgeThresholdValue->setText(QString::number(value));
    updatePreview();
}

void LineExtractionDialog::updatePreview()
{
    // Update edge preview
    QImage edgeImg = renderEdgePreview();
    QImage scaledEdge = edgeImg.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_edgePreview->setPixmap(QPixmap::fromImage(scaledEdge));
}

QImage LineExtractionDialog::renderEdgePreview()
{
    return m_extractor.getEdgePreview(m_sourceImage, m_edgeThresholdSlider->value());
}

QImage LineExtractionDialog::renderLinesPreview()
{
    QImage preview = m_sourceImage.convertToFormat(QImage::Format_ARGB32);
    QPainter painter(&preview);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(Qt::red, 2);
    painter.setPen(pen);

    for (const auto &line : m_lines) {
        painter.drawLine(line.start, line.end);
    }

    // Draw endpoints
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 255, 0, 200));
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

    LineExtractor::Parameters params;
    params.edgeThreshold = m_edgeThresholdSlider->value();
    params.houghThreshold = m_houghThresholdSlider->value();
    params.minLineLength = m_minLineLengthSlider->value();
    params.maxLineGap = m_maxLineGapSlider->value();
    params.angleSnapDegrees = m_angleSnapCheck->isChecked() ? m_angleSnapSpin->value() : 0.0;
    params.mergeCollinear = m_mergeCheck->isChecked();

    m_lines = m_extractor.extractLines(m_sourceImage, params);

    m_lineCountLabel->setText(QString("Lines detected: %1").arg(m_lines.size()));
    m_detectButton->setEnabled(true);
    m_progressBar->setVisible(false);

    bool hasLines = !m_lines.empty();
    m_createButton->setEnabled(hasLines);
    m_exportButton->setEnabled(hasLines);

    // Update lines preview
    QImage linesImg = renderLinesPreview();
    QImage scaledLines = linesImg.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_linesPreview->setPixmap(QPixmap::fromImage(scaledLines));

    // Switch to lines tab
    m_previewTabs->setCurrentIndex(2);
}

void LineExtractionDialog::onProgressChanged(int percent)
{
    m_progressBar->setValue(percent);
}

void LineExtractionDialog::onCreateLines()
{
    if (m_lines.empty() || !m_layerManager) return;

    double scale = m_scaleSpin->value();

    // Create a new layer for extracted lines
    Layer *layer = m_layerManager->createLayer("Extracted Lines");
    if (!layer) return;

    m_layerManager->setActiveLayer(layer);

    for (const auto &line : m_lines) {
        auto linePrim = std::make_unique<LinePrimitive>(
            QVector2D(line.start.x() / scale, line.start.y() / scale),
            QVector2D(line.end.x() / scale, line.end.y() / scale));
        linePrim->setColor(Qt::black);
        linePrim->setLineWidth(1.0f);
        linePrim->setLayerId(layer->id());
        layer->addPrimitive(std::move(linePrim));
    }

    if (m_canvas) {
        m_canvas->update();
    }

    QMessageBox::information(this, "Lines Created",
                             QString("Created %1 line primitives on layer \"%2\".")
                                 .arg(m_lines.size())
                                 .arg(layer->name()));
    accept();
}

void LineExtractionDialog::onExportDXF()
{
    if (m_lines.empty()) return;

    QString fileName = QFileDialog::getSaveFileName(
        this, "Export Lines as DXF", "", "DXF Files (*.dxf)");
    if (fileName.isEmpty()) return;

    double scale = m_scaleSpin->value();
    auto units = static_cast<DXFExporter::Units>(m_unitsCombo->currentData().toInt());

    // Convert to line pairs with scale applied
    std::vector<std::pair<QPointF, QPointF>> linePairs;
    linePairs.reserve(m_lines.size());
    for (const auto &line : m_lines) {
        linePairs.emplace_back(
            QPointF(line.start.x() / scale, line.start.y() / scale),
            QPointF(line.end.x() / scale, line.end.y() / scale));
    }

    DXFExporter exporter;
    if (exporter.exportLines(fileName, linePairs, units)) {
        QMessageBox::information(this, "DXF Export",
                                 QString("Exported %1 lines to:\n%2")
                                     .arg(m_lines.size())
                                     .arg(fileName));
    } else {
        QMessageBox::warning(this, "DXF Export", "Failed to export DXF file.");
    }
}
