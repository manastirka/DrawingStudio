#include "GuitarSpecsBrowserDialog.h"
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>

GuitarSpecsBrowserDialog::GuitarSpecsBrowserDialog(QWidget *parent)
    : QDialog(parent)
    , m_database(GuitarSpecsDatabase::instance())
{
    setWindowTitle("Guitar Specifications Database");
    setMinimumSize(1000, 700);
    resize(1200, 800);
    
    setupUI();
    connectSignals();
    populateManufacturers();
    
    // Load all specs initially
    populateSpecs(m_database.getAllSpecs());
}

void GuitarSpecsBrowserDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Top controls
    m_topLayout = new QHBoxLayout();
    
    m_topLayout->addWidget(new QLabel("Manufacturer:"));
    m_manufacturerCombo = new QComboBox();
    m_manufacturerCombo->setMinimumWidth(150);
    m_topLayout->addWidget(m_manufacturerCombo);
    
    m_topLayout->addSpacing(20);
    
    m_topLayout->addWidget(new QLabel("Search:"));
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Search models, woods, features...");
    m_searchEdit->setMinimumWidth(200);
    m_topLayout->addWidget(m_searchEdit);
    
    m_clearButton = new QPushButton("Clear");
    m_clearButton->setMaximumWidth(60);
    m_topLayout->addWidget(m_clearButton);
    
    m_topLayout->addStretch();
    
    m_exportButton = new QPushButton("Export to CAD");
    m_exportButton->setEnabled(false);
    m_topLayout->addWidget(m_exportButton);
    
    m_mainLayout->addLayout(m_topLayout);
    
    // Main content splitter
    m_splitter = new QSplitter(Qt::Horizontal);
    
    // Setup specs table
    setupSpecsTable();
    m_splitter->addWidget(m_specsTable);
    
    // Setup details panel
    setupDetailsPanel();
    m_splitter->addWidget(m_detailsGroup);
    
    // Set splitter proportions (60% table, 40% details)
    m_splitter->setSizes({600, 400});
    
    m_mainLayout->addWidget(m_splitter);
    
    // Status bar with count
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(new QLabel("Double-click a guitar to see detailed specifications"));
    statusLayout->addStretch();
    m_mainLayout->addLayout(statusLayout);
}

void GuitarSpecsBrowserDialog::setupSpecsTable()
{
    m_specsTable = new QTableWidget();
    m_specsTable->setColumnCount(8);
    
    QStringList headers = {
        "Manufacturer", "Model", "Series", "Year", "Body Style", 
        "Scale Length", "Pickups", "Price Range"
    };
    m_specsTable->setHorizontalHeaderLabels(headers);
    
    // Configure table appearance
    m_specsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_specsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_specsTable->setAlternatingRowColors(true);
    m_specsTable->setSortingEnabled(true);
    
    // Adjust column widths
    QHeaderView *header = m_specsTable->horizontalHeader();
    header->setStretchLastSection(true);
    header->resizeSection(0, 100); // Manufacturer
    header->resizeSection(1, 150); // Model
    header->resizeSection(2, 120); // Series
    header->resizeSection(3, 60);  // Year
    header->resizeSection(4, 100); // Body Style
    header->resizeSection(5, 80);  // Scale Length
    header->resizeSection(6, 80);  // Pickups
    // Price Range stretches
}

void GuitarSpecsBrowserDialog::setupDetailsPanel()
{
    m_detailsGroup = new QGroupBox("Guitar Specifications");
    m_detailsLayout = new QVBoxLayout(m_detailsGroup);
    
    // Basic info grid
    QGroupBox *basicGroup = new QGroupBox("Basic Information");
    m_detailsGridLayout = new QGridLayout(basicGroup);
    
    int row = 0;
    m_detailsGridLayout->addWidget(new QLabel("Model:"), row, 0);
    m_modelLabel = new QLabel("-");
    m_modelLabel->setStyleSheet("font-weight: bold;");
    m_detailsGridLayout->addWidget(m_modelLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Series:"), row, 0);
    m_seriesLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_seriesLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Year Introduced:"), row, 0);
    m_yearLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_yearLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Body Style:"), row, 0);
    m_bodyStyleLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_bodyStyleLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Body Wood:"), row, 0);
    m_bodyWoodLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_bodyWoodLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Neck Wood:"), row, 0);
    m_neckWoodLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_neckWoodLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Fretboard:"), row, 0);
    m_fretboardWoodLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_fretboardWoodLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Scale Length:"), row, 0);
    m_scaleLengthLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_scaleLengthLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Nut Width:"), row, 0);
    m_nutWidthLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_nutWidthLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Number of Frets:"), row, 0);
    m_numberOfFretsLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_numberOfFretsLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Pickup Config:"), row, 0);
    m_pickupConfigLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_pickupConfigLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Bridge:"), row, 0);
    m_bridgeLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_bridgeLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Tuners:"), row, 0);
    m_tunersLabel = new QLabel("-");
    m_detailsGridLayout->addWidget(m_tunersLabel, row++, 1);
    
    m_detailsGridLayout->addWidget(new QLabel("Price Range:"), row, 0);
    m_priceRangeLabel = new QLabel("-");
    m_priceRangeLabel->setStyleSheet("color: #0066cc; font-weight: bold;");
    m_detailsGridLayout->addWidget(m_priceRangeLabel, row++, 1);
    
    m_detailsLayout->addWidget(basicGroup);
    
    // Dimensions group
    m_dimensionsGroup = new QGroupBox("Dimensions (mm)");
    m_dimensionsGridLayout = new QGridLayout(m_dimensionsGroup);
    
    row = 0;
    m_dimensionsGridLayout->addWidget(new QLabel("Body Length:"), row, 0);
    m_bodyLengthLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_bodyLengthLabel, row, 1);
    m_dimensionsGridLayout->addWidget(new QLabel("Body Width:"), row, 2);
    m_bodyWidthLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_bodyWidthLabel, row++, 3);
    
    m_dimensionsGridLayout->addWidget(new QLabel("Body Thickness:"), row, 0);
    m_bodyThicknessLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_bodyThicknessLabel, row, 1);
    m_dimensionsGridLayout->addWidget(new QLabel("Upper Bout:"), row, 2);
    m_upperBoutLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_upperBoutLabel, row++, 3);
    
    m_dimensionsGridLayout->addWidget(new QLabel("Lower Bout:"), row, 0);
    m_lowerBoutLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_lowerBoutLabel, row, 1);
    m_dimensionsGridLayout->addWidget(new QLabel("Waist Width:"), row, 2);
    m_waistWidthLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_waistWidthLabel, row++, 3);
    
    m_dimensionsGridLayout->addWidget(new QLabel("Headstock Length:"), row, 0);
    m_headstockLengthLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_headstockLengthLabel, row, 1);
    m_dimensionsGridLayout->addWidget(new QLabel("Headstock Width:"), row, 2);
    m_headstockWidthLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_headstockWidthLabel, row++, 3);
    
    m_dimensionsGridLayout->addWidget(new QLabel("Neck @ 1st Fret:"), row, 0);
    m_neckThickness1stLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_neckThickness1stLabel, row, 1);
    m_dimensionsGridLayout->addWidget(new QLabel("Neck @ 12th Fret:"), row, 2);
    m_neckThickness12thLabel = new QLabel("-");
    m_dimensionsGridLayout->addWidget(m_neckThickness12thLabel, row++, 3);
    
    m_detailsLayout->addWidget(m_dimensionsGroup);
    
    // Description
    QGroupBox *descGroup = new QGroupBox("Description");
    QVBoxLayout *descLayout = new QVBoxLayout(descGroup);
    m_descriptionText = new QTextEdit();
    m_descriptionText->setMaximumHeight(80);
    m_descriptionText->setReadOnly(true);
    descLayout->addWidget(m_descriptionText);
    m_detailsLayout->addWidget(descGroup);
    
    // Special features
    QGroupBox *featuresGroup = new QGroupBox("Special Features");
    QVBoxLayout *featuresLayout = new QVBoxLayout(featuresGroup);
    m_featuresText = new QTextEdit();
    m_featuresText->setMaximumHeight(60);
    m_featuresText->setReadOnly(true);
    featuresLayout->addWidget(m_featuresText);
    m_detailsLayout->addWidget(featuresGroup);
    
    m_detailsLayout->addStretch();
}

void GuitarSpecsBrowserDialog::connectSignals()
{
    connect(m_manufacturerCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &GuitarSpecsBrowserDialog::onManufacturerChanged);
    
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &GuitarSpecsBrowserDialog::onSearchTextChanged);
    
    connect(m_clearButton, &QPushButton::clicked,
            this, &GuitarSpecsBrowserDialog::onClearSearch);
    
    connect(m_exportButton, &QPushButton::clicked,
            this, &GuitarSpecsBrowserDialog::onExportToCAD);
    
    connect(m_specsTable, &QTableWidget::itemSelectionChanged,
            this, &GuitarSpecsBrowserDialog::onSpecSelectionChanged);
}

void GuitarSpecsBrowserDialog::populateManufacturers()
{
    m_manufacturerCombo->addItem("All Manufacturers");
    
    QStringList manufacturers = m_database.getAllManufacturers();
    manufacturers.sort();
    m_manufacturerCombo->addItems(manufacturers);
}

void GuitarSpecsBrowserDialog::populateSpecs(const QVector<GuitarSpecs> &specs)
{
    m_specsTable->setRowCount(specs.size());
    
    for (int i = 0; i < specs.size(); ++i) {
        const GuitarSpecs &spec = specs[i];
        
        m_specsTable->setItem(i, 0, new QTableWidgetItem(spec.manufacturer));
        m_specsTable->setItem(i, 1, new QTableWidgetItem(spec.model));
        m_specsTable->setItem(i, 2, new QTableWidgetItem(spec.series));
        m_specsTable->setItem(i, 3, new QTableWidgetItem(QString::number(spec.yearIntroduced)));
        m_specsTable->setItem(i, 4, new QTableWidgetItem(m_database.bodyStyleToString(spec.bodyStyle)));
        m_specsTable->setItem(i, 5, new QTableWidgetItem(QString("%1mm").arg(spec.scaleLength)));
        m_specsTable->setItem(i, 6, new QTableWidgetItem(m_database.pickupConfigToString(spec.pickupConfig)));
        m_specsTable->setItem(i, 7, new QTableWidgetItem(spec.priceRange));
        
        // Store the spec index in the first item for easy retrieval
        m_specsTable->item(i, 0)->setData(Qt::UserRole, i);
    }
    
    m_specsTable->resizeRowsToContents();
}

void GuitarSpecsBrowserDialog::onManufacturerChanged(const QString &manufacturer)
{
    if (manufacturer == "All Manufacturers") {
        populateSpecs(m_database.getAllSpecs());
    } else {
        // Find manufacturer enum
        for (int i = 0; i < 14; ++i) { // We have 14 manufacturers
            GuitarManufacturer mfg = static_cast<GuitarManufacturer>(i);
            if (m_database.manufacturerToString(mfg) == manufacturer) {
                populateSpecs(m_database.getSpecsByManufacturer(mfg));
                break;
            }
        }
    }
    
    clearDetailsPanel();
}

void GuitarSpecsBrowserDialog::onSearchTextChanged(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        onManufacturerChanged(m_manufacturerCombo->currentText());
    } else {
        populateSpecs(m_database.searchSpecs(text));
    }
    
    clearDetailsPanel();
}

void GuitarSpecsBrowserDialog::onClearSearch()
{
    m_searchEdit->clear();
    m_manufacturerCombo->setCurrentIndex(0);
    populateSpecs(m_database.getAllSpecs());
    clearDetailsPanel();
}

void GuitarSpecsBrowserDialog::onSpecSelectionChanged()
{
    GuitarSpecs spec = getSelectedSpec();
    if (!spec.manufacturer.isEmpty()) {
        updateDetailsPanel(spec);
        m_exportButton->setEnabled(true);
    } else {
        clearDetailsPanel();
        m_exportButton->setEnabled(false);
    }
}

void GuitarSpecsBrowserDialog::onExportToCAD()
{
    GuitarSpecs spec = getSelectedSpec();
    if (spec.manufacturer.isEmpty()) {
        return;
    }
    
    // Copy specifications to clipboard in a formatted way
    QString export_text = QString(
        "Guitar Specifications Export\n"
        "============================\n\n"
        "Basic Information:\n"
        "Manufacturer: %1\n"
        "Model: %2\n"
        "Series: %3\n"
        "Year Introduced: %4\n"
        "Description: %5\n\n"
        "Body Specifications:\n"
        "Body Style: %6\n"
        "Body Length: %7 mm\n"
        "Body Width: %8 mm\n"
        "Body Thickness: %9 mm\n"
        "Upper Bout Width: %10 mm\n"
        "Lower Bout Width: %11 mm\n"
        "Waist Width: %12 mm\n"
        "Body Wood: %13\n\n"
        "Neck Specifications:\n"
        "Scale Length: %14 mm\n"
        "Neck Length: %15 mm\n"
        "Number of Frets: %16\n"
        "Nut Width: %17 mm\n"
        "Neck Thickness (1st Fret): %18 mm\n"
        "Neck Thickness (12th Fret): %19 mm\n"
        "Neck Wood: %20\n"
        "Fretboard Wood: %21\n"
        "Neck Profile: %22\n"
        "Neck Joint: %23\n\n"
        "Hardware:\n"
        "Pickup Configuration: %24\n"
        "Bridge: %25\n"
        "Tailpiece: %26\n"
        "Tuners: %27\n\n"
        "Price Range: %28\n"
    ).arg(spec.manufacturer, spec.model, spec.series, QString::number(spec.yearIntroduced), spec.description)
     .arg(m_database.bodyStyleToString(spec.bodyStyle))
     .arg(spec.bodyLength).arg(spec.bodyWidth).arg(spec.bodyThickness)
     .arg(spec.upperBoutWidth).arg(spec.lowerBoutWidth).arg(spec.waistWidth)
     .arg(spec.bodyWood)
     .arg(spec.scaleLength).arg(spec.neckLength).arg(spec.numberOfFrets)
     .arg(spec.nutWidth).arg(spec.neckThickness1stFret).arg(spec.neckThickness12thFret)
     .arg(spec.neckWood, spec.fretboardWood, spec.neckProfile, spec.neckJoint)
     .arg(m_database.pickupConfigToString(spec.pickupConfig))
     .arg(spec.bridge, spec.tailpiece, spec.tuners, spec.priceRange);
    
    QApplication::clipboard()->setText(export_text);
    
    QMessageBox::information(this, "Export Complete", 
        QString("Specifications for %1 %2 have been copied to clipboard.\n\n"
                "You can now paste these measurements into your CAD application.")
        .arg(spec.manufacturer, spec.model));
}

GuitarSpecs GuitarSpecsBrowserDialog::getSelectedSpec() const
{
    int currentRow = m_specsTable->currentRow();
    if (currentRow < 0) {
        return GuitarSpecs();
    }
    
    QTableWidgetItem *item = m_specsTable->item(currentRow, 0);
    if (!item) {
        return GuitarSpecs();
    }
    
    int specIndex = item->data(Qt::UserRole).toInt();
    QVector<GuitarSpecs> allSpecs = m_database.getAllSpecs();
    
    if (specIndex < 0 || specIndex >= allSpecs.size()) {
        return GuitarSpecs();
    }
    
    return allSpecs[specIndex];
}

void GuitarSpecsBrowserDialog::updateDetailsPanel(const GuitarSpecs &spec)
{
    m_modelLabel->setText(QString("%1 %2").arg(spec.manufacturer, spec.model));
    m_seriesLabel->setText(spec.series);
    m_yearLabel->setText(QString::number(spec.yearIntroduced));
    m_bodyStyleLabel->setText(m_database.bodyStyleToString(spec.bodyStyle));
    m_bodyWoodLabel->setText(spec.bodyWood);
    m_neckWoodLabel->setText(spec.neckWood);
    m_fretboardWoodLabel->setText(spec.fretboardWood);
    m_scaleLengthLabel->setText(QString("%1 mm").arg(spec.scaleLength));
    m_nutWidthLabel->setText(QString("%1 mm").arg(spec.nutWidth));
    m_numberOfFretsLabel->setText(QString::number(spec.numberOfFrets));
    m_pickupConfigLabel->setText(m_database.pickupConfigToString(spec.pickupConfig));
    m_bridgeLabel->setText(spec.bridge);
    m_tunersLabel->setText(spec.tuners);
    m_priceRangeLabel->setText(spec.priceRange);
    
    // Dimensions
    m_bodyLengthLabel->setText(QString("%1").arg(spec.bodyLength));
    m_bodyWidthLabel->setText(QString("%1").arg(spec.bodyWidth));
    m_bodyThicknessLabel->setText(QString("%1").arg(spec.bodyThickness));
    m_upperBoutLabel->setText(QString("%1").arg(spec.upperBoutWidth));
    m_lowerBoutLabel->setText(QString("%1").arg(spec.lowerBoutWidth));
    m_waistWidthLabel->setText(QString("%1").arg(spec.waistWidth));
    m_headstockLengthLabel->setText(QString("%1").arg(spec.headstockLength));
    m_headstockWidthLabel->setText(QString("%1").arg(spec.headstockWidth));
    m_neckThickness1stLabel->setText(QString("%1").arg(spec.neckThickness1stFret));
    m_neckThickness12thLabel->setText(QString("%1").arg(spec.neckThickness12thFret));
    
    // Description and features
    m_descriptionText->setText(spec.description);
    m_featuresText->setText(spec.specialFeatures.join(", "));
}

void GuitarSpecsBrowserDialog::clearDetailsPanel()
{
    m_modelLabel->setText("-");
    m_seriesLabel->setText("-");
    m_yearLabel->setText("-");
    m_bodyStyleLabel->setText("-");
    m_bodyWoodLabel->setText("-");
    m_neckWoodLabel->setText("-");
    m_fretboardWoodLabel->setText("-");
    m_scaleLengthLabel->setText("-");
    m_nutWidthLabel->setText("-");
    m_numberOfFretsLabel->setText("-");
    m_pickupConfigLabel->setText("-");
    m_bridgeLabel->setText("-");
    m_tunersLabel->setText("-");
    m_priceRangeLabel->setText("-");
    
    m_bodyLengthLabel->setText("-");
    m_bodyWidthLabel->setText("-");
    m_bodyThicknessLabel->setText("-");
    m_upperBoutLabel->setText("-");
    m_lowerBoutLabel->setText("-");
    m_waistWidthLabel->setText("-");
    m_headstockLengthLabel->setText("-");
    m_headstockWidthLabel->setText("-");
    m_neckThickness1stLabel->setText("-");
    m_neckThickness12thLabel->setText("-");
    
    m_descriptionText->clear();
    m_featuresText->clear();
    
    m_exportButton->setEnabled(false);
}