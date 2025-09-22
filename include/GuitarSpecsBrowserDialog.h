#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTableWidgetItem>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include "GuitarSpecsDatabase.h"

class GuitarSpecsBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GuitarSpecsBrowserDialog(QWidget *parent = nullptr);
    ~GuitarSpecsBrowserDialog() = default;

private slots:
    void onManufacturerChanged(const QString &manufacturer);
    void onSearchTextChanged(const QString &text);
    void onSpecSelectionChanged();
    void onClearSearch();
    void onExportToCAD();

private:
    void setupUI();
    void setupSpecsTable();
    void setupDetailsPanel();
    void connectSignals();
    void populateManufacturers();
    void populateSpecs(const QVector<GuitarSpecs> &specs);
    void updateDetailsPanel(const GuitarSpecs &spec);
    void clearDetailsPanel();
    GuitarSpecs getSelectedSpec() const;
    
    // UI Components
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_topLayout;
    QSplitter *m_splitter;
    
    // Search and filter controls
    QComboBox *m_manufacturerCombo;
    QLineEdit *m_searchEdit;
    QPushButton *m_clearButton;
    QPushButton *m_exportButton;
    
    // Specs table
    QTableWidget *m_specsTable;
    
    // Details panel
    QGroupBox *m_detailsGroup;
    QVBoxLayout *m_detailsLayout;
    QGridLayout *m_detailsGridLayout;
    
    // Detail labels
    QLabel *m_modelLabel;
    QLabel *m_seriesLabel;
    QLabel *m_yearLabel;
    QLabel *m_bodyStyleLabel;
    QLabel *m_bodyWoodLabel;
    QLabel *m_neckWoodLabel;
    QLabel *m_fretboardWoodLabel;
    QLabel *m_scaleLengthLabel;
    QLabel *m_nutWidthLabel;
    QLabel *m_numberOfFretsLabel;
    QLabel *m_pickupConfigLabel;
    QLabel *m_bridgeLabel;
    QLabel *m_tunersLabel;
    QLabel *m_priceRangeLabel;
    
    // Dimensions display
    QGroupBox *m_dimensionsGroup;
    QGridLayout *m_dimensionsGridLayout;
    QLabel *m_bodyLengthLabel;
    QLabel *m_bodyWidthLabel;
    QLabel *m_bodyThicknessLabel;
    QLabel *m_upperBoutLabel;
    QLabel *m_lowerBoutLabel;
    QLabel *m_waistWidthLabel;
    QLabel *m_headstockLengthLabel;
    QLabel *m_headstockWidthLabel;
    QLabel *m_neckThickness1stLabel;
    QLabel *m_neckThickness12thLabel;
    
    // Description
    QTextEdit *m_descriptionText;
    
    // Special features
    QTextEdit *m_featuresText;
    
    // Database reference
    GuitarSpecsDatabase &m_database;
};