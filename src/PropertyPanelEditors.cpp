#include "PropertyPanel.h"
#include "DrawingPrimitive.h"
#include "DrawingCanvas.h"
#include "ImagePrimitive.h"
#include "SimpleTextPanel.h"
#include "ClassicTextTool.h"
#include "BrushStrokePrimitive.h"
#include "DrawingTool.h"

#include <QColorDialog>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QCheckBox>
#include <QGroupBox>
#include <QDebug>
#include <QFontDatabase>
#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QMap>
#include <functional>
#include <cmath>
#include <algorithm>
#include <vector>

// UI chrome + property editors (refactor E17).

void PropertyPanel::setupUI()
{
    // Ensure a comfortable default width; still resizable by the user
    setMinimumWidth(280);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    
    // macOS-style Light Theme for PropertyPanel
    setStyleSheet(R"(
        PropertyPanel {
            background: #17191c;
            border-left: 1px solid rgba(255,255,255,0.08);
            padding: 0px;
        }

        QScrollArea {
            border: none;
            background: transparent;
            margin: 0px;
        }

        QScrollBar:vertical {
            background: rgba(255,255,255,0.06);
            width: 12px;
            border-radius: 6px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: rgba(138, 180, 255, 0.55);
            border-radius: 6px;
            min-height: 24px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(138, 180, 255, 0.75);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            border: none;
            background: none;
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }

        QLabel {
            color: #e7eaf0;
            font-size: 10px;
        }

        QToolTip {
            background-color: rgba(20, 22, 26, 0.92);
            color: #e7eaf0;
            border: 1px solid rgba(138, 180, 255, 0.25);
            border-radius: 8px;
            padding: 8px 10px;
            font-size: 10px;
        }

        QLineEdit, QComboBox {
            background-color: #22252b;
            color: #e7eaf0;
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 6px;
            padding: 5px 7px;
            font-size: 10px;
        }

        QSpinBox, QDoubleSpinBox {
            background-color: #22252b;
            color: #e7eaf0;
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 6px;
            padding: 5px 8px;
            font-size: 10px;
        }
        QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover {
            border-color: rgba(138, 180, 255, 0.45);
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: rgba(138, 180, 255, 0.85);
            background-color: #252a31;
        }

        QComboBox::drop-down {
            border: none;
            width: 18px;
        }
        QComboBox QAbstractItemView {
            background: #1e2126;
            color: #e7eaf0;
            border: 1px solid rgba(255,255,255,0.12);
            selection-background-color: rgba(138, 180, 255, 0.22);
        }

        QCheckBox {
            color: #e7eaf0;
            spacing: 8px;
        }

        QSlider::groove:horizontal {
            height: 6px;
            background: rgba(255,255,255,0.10);
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            width: 14px;
            margin: -5px 0;
            border-radius: 7px;
            background: rgba(138, 180, 255, 0.95);
        }

        QPushButton {
            background: #22252b;
            color: #e7eaf0;
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 6px;
            padding: 5px 9px;
            font-size: 10px;
        }
        QPushButton:hover {
            border-color: rgba(138, 180, 255, 0.55);
            background: #262b33;
        }
        QPushButton:pressed {
            background: #1d2026;
        }


        /* Spinbox buttons are removed programmatically via setButtonSymbols(NoButtons). */
    )");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    // Dock widget already provides a title bar — don't duplicate it here.
    
    // Scroll area for properties
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn); // Always show scrollbar
    
    m_contentWidget = new QWidget();
    // Let content expand with the dock width (avoid right-side "dead space")
    m_contentWidget->setMaximumWidth(QWIDGETSIZE_MAX);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(10, 10, 10, 10);
    m_contentLayout->setSpacing(6);
    
    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea, 1);
    
    // Initial message: subtle, no placeholder-style card
    QLabel* emptyLabel = new QLabel("No selection");
    emptyLabel->setStyleSheet(R"(
        QLabel {
            color: rgba(231,234,240,0.35);
            font-size: 11px;
            padding: 32px 8px;
            background: transparent;
        }
    )");
    emptyLabel->setAlignment(Qt::AlignCenter);
    m_contentLayout->addWidget(emptyLabel);
    m_contentLayout->addStretch();
}

void PropertyPanel::clearProperties()
{
    qDebug() << "PropertyPanel::clearProperties called";
    qDebug() << "  m_contentLayout:" << (void*)m_contentLayout;
    qDebug() << "  m_simpleTextPanel:" << (void*)m_simpleTextPanel;
    
    clearLayout();
    m_currentPrimitive = nullptr;
    
    // Hide text tool panel
    if (m_simpleTextPanel) {
        m_simpleTextPanel->hide();
    }
    
    // Safety check before adding widgets
    if (!m_contentLayout) {
        qDebug() << "PropertyPanel::clearProperties - m_contentLayout is null, cannot add empty label!";
        return;
    }
    
    QLabel* emptyLabel = new QLabel("No selection");
    emptyLabel->setStyleSheet(R"(
        QLabel {
            color: rgba(231,234,240,0.35);
            font-size: 11px;
            padding: 32px 8px;
            background: transparent;
        }
    )");
    emptyLabel->setAlignment(Qt::AlignCenter);
    m_contentLayout->addWidget(emptyLabel);
    m_contentLayout->addStretch();

    qDebug() << "PropertyPanel::clearProperties completed";
}

void PropertyPanel::clearLayout()
{
    // Safety check - ensure layout exists
    if (!m_contentLayout) {
        qDebug() << "PropertyPanel::clearLayout - m_contentLayout is null!";
        return;
    }
    
    // Clear existing layout
    QLayoutItem* child;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        QWidget* widget = child->widget();
        
        // Don't delete the text tool panel directly - just remove it from layout
        if (widget && widget != m_simpleTextPanel) {
            delete widget;
        } else if (widget == m_simpleTextPanel && m_simpleTextPanel) {
            m_simpleTextPanel->hide();
        }
        delete child;
    }
    m_propertyEditors.clear();
}

void PropertyPanel::addGroup(const QString& title)
{
    // Subtle top separator so consecutive groups read as distinct sections.
    QFrame* topSep = new QFrame();
    topSep->setFrameShape(QFrame::HLine);
    topSep->setFrameShadow(QFrame::Plain);
    topSep->setStyleSheet(R"(
        QFrame {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 transparent, stop:0.5 rgba(255,255,255,0.09), stop:1 transparent);
            max-height: 1px;
            margin: 8px 0 0 0;
        }
    )");
    m_contentLayout->addWidget(topSep);

    // Wrapper with a subtle accent strip on the left and spacing above
    QWidget* header = new QWidget();
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setStyleSheet(R"(
        QWidget {
            background: transparent;
            margin-top: 10px;
        }
    )");

    QHBoxLayout* lay = new QHBoxLayout(header);
    lay->setContentsMargins(0, 6, 0, 4);
    lay->setSpacing(8);

    QWidget* accent = new QWidget();
    accent->setFixedSize(3, 11);
    accent->setStyleSheet(R"(
        QWidget {
            background: rgba(138, 180, 255, 0.8);
            border-radius: 1.5px;
        }
    )");

    QLabel* groupLabel = new QLabel(title.toUpper());
    groupLabel->setStyleSheet(R"(
        QLabel {
            font-weight: 700;
            font-size: 10px;
            color: rgba(231, 234, 240, 0.72);
            letter-spacing: 0.9px;
            background: transparent;
        }
    )");

    lay->addWidget(accent);
    lay->addWidget(groupLabel);
    lay->addStretch();

    m_contentLayout->addWidget(header);
}

void PropertyPanel::addSeparator()
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet(R"(
        QFrame {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 transparent, stop:0.5 rgba(255,255,255,0.10), stop:1 transparent);
            height: 1px;
            margin: 10px 0;
        }
    )");
    m_contentLayout->addWidget(line);
}

void PropertyPanel::addPropertyEditor(const QString& propertyName,
                                     const QString& displayName,
                                     QMetaType::Type type, const QVariant& value)
{
    QWidget* editor = createEditor(propertyName, type, value);
    if (!editor) return;

    QWidget* row = new QWidget();
    row->setObjectName("propRow");
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setStyleSheet(R"(
        QWidget#propRow {
            background: transparent;
            border: none;
            border-radius: 4px;
        }
        QWidget#propRow:hover {
            background: rgba(255,255,255,0.035);
        }
    )");

    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->setSpacing(10);

    QLabel* label = new QLabel(displayName);
    label->setFixedWidth(86);
    label->setStyleSheet(R"(
        QLabel {
            color: rgba(231, 234, 240, 0.62);
            font-weight: 500;
            font-size: 11px;
            background: transparent;
        }
    )");
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    layout->addWidget(label);
    layout->addWidget(editor, 1);

    m_contentLayout->addWidget(row);

    m_propertyEditors[propertyName] = editor;
}

QWidget* PropertyPanel::createEditor(const QString& propertyName, QMetaType::Type type, const QVariant& value)
{
    // Check if this is a readonly/calculated property
    bool isReadonly = (propertyName == "type");

    // Special case: line style editor
    if (propertyName == "lineStyle") {
        QComboBox* combo = new QComboBox();
        struct Item { const char* name; Qt::PenStyle style; };
        const Item items[] = {
            {"Solid", Qt::SolidLine},
            {"Dash", Qt::DashLine},
            {"Dot", Qt::DotLine},
            {"Dash Dot", Qt::DashDotLine},
            {"Dash Dot Dot", Qt::DashDotDotLine}
        };
        int currentIdx = 0;
        for (int i = 0; i < (int)(sizeof(items)/sizeof(items[0])); ++i) {
            combo->addItem(items[i].name, static_cast<int>(items[i].style));
        }
        Qt::PenStyle current = static_cast<Qt::PenStyle>(value.toInt());
        for (int i = 0; i < combo->count(); ++i) {
            if (static_cast<Qt::PenStyle>(combo->itemData(i).toInt()) == current) {
                currentIdx = i;
                break;
            }
        }
        combo->setCurrentIndex(currentIdx);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &PropertyPanel::onPropertyValueChanged);
        return combo;
    }

    // Special case for paper format dropdown
    if (propertyName == "paperFormat") {
            QComboBox* combo = new QComboBox();
            combo->addItem("A4 (210×297mm)", static_cast<int>(DrawingCanvas::PaperFormat::A4));
            combo->addItem("A3 (297×420mm)", static_cast<int>(DrawingCanvas::PaperFormat::A3));
            combo->addItem("A2 (420×594mm)", static_cast<int>(DrawingCanvas::PaperFormat::A2));
            combo->addItem("A1 (594×841mm)", static_cast<int>(DrawingCanvas::PaperFormat::A1));
            combo->addItem("A0 (841×1189mm)", static_cast<int>(DrawingCanvas::PaperFormat::A0));
            combo->addItem("Letter (8.5×11in)", static_cast<int>(DrawingCanvas::PaperFormat::Letter));
            combo->addItem("Legal (8.5×14in)", static_cast<int>(DrawingCanvas::PaperFormat::Legal));
            combo->addItem("Tabloid (11×17in)", static_cast<int>(DrawingCanvas::PaperFormat::Tabloid));
            combo->addItem("Custom", static_cast<int>(DrawingCanvas::PaperFormat::Custom));
            
            // Set current value based on display name
            QString currentName = value.toString();
            for (int i = 0; i < combo->count(); ++i) {
                if (combo->itemText(i) == currentName) {
                    combo->setCurrentIndex(i);
                    break;
                }
            }
            
            // Use panel-global dark styling
            combo->setStyleSheet(QString());
            
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
                    this, &PropertyPanel::onPropertyValueChanged);
            return combo;
        }
    
    switch (type) {
        case QMetaType::QString: {
            // Special case: Use QTextEdit for "text" property (multiline text input)
            if (propertyName == "text") {
                QTextEdit* textEdit = new QTextEdit();
                textEdit->setPlainText(value.toString());
                textEdit->setMinimumHeight(100); // Larger text area
                textEdit->setMaximumHeight(200);
                textEdit->setStyleSheet(R"(
                    QTextEdit {
                        background-color: #2a2a2a;
                        color: #e8e8e8;
                        border: 1px solid #3d3d3d;
                        border-radius: 6px;
                        padding: 8px 12px;
                        font-size: 11px;
                        font-family: 'Segoe UI', Arial, sans-serif;
                    }
                    QTextEdit:focus {
                        border: 2px solid #6495ed;
                        background-color: #1e1e1e;
                    }
                    QTextEdit:hover {
                        border: 1px solid #6495ed;
                    }
                )");
                connect(textEdit, &QTextEdit::textChanged, this, &PropertyPanel::onPropertyValueChanged);
                return textEdit;
            }
            
            // Special case: Use QComboBox for "fontFamily" property
            if (propertyName == "fontFamily") {
                QComboBox* fontCombo = new QComboBox();
                fontCombo->setEditable(false);
                fontCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                fontCombo->setMaximumWidth(250); // Prevent stretching beyond panel
                fontCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
                
                // Add common fonts
                QStringList commonFonts = {
                    "Arial", "Helvetica", "Times New Roman", "Georgia", "Courier New",
                    "Verdana", "Trebuchet MS", "Comic Sans MS", "Impact", "Palatino",
                    "Garamond", "Bookman", "Avant Garde", "Futura", "Optima"
                };
                
                // Add all system fonts
                QStringList allFonts = QFontDatabase::families();
                
                // Combine: common fonts first, then separator, then all fonts
                fontCombo->addItems(commonFonts);
                fontCombo->insertSeparator(commonFonts.size());
                
                // Add remaining fonts (excluding duplicates)
                for (const QString& font : allFonts) {
                    if (!commonFonts.contains(font)) {
                        fontCombo->addItem(font);
                    }
                }
                
                // Set current font
                QString currentFont = value.toString();
                int index = fontCombo->findText(currentFont);
                if (index >= 0) {
                    fontCombo->setCurrentIndex(index);
                }
                
                fontCombo->setStyleSheet(R"(
                    QComboBox {
                        background-color: #2a2a2a;
                        color: #e8e8e8;
                        border: 1px solid #3d3d3d;
                        border-radius: 6px;
                        padding: 8px 12px;
                        font-size: 11px;
                    }
                    QComboBox:hover {
                        border: 1px solid #6495ed;
                    }
                    QComboBox:focus {
                        border: 2px solid #6495ed;
                        background-color: #1e1e1e;
                    }
                    QComboBox::drop-down {
                        border: none;
                        width: 20px;
                    }
                    QComboBox::down-arrow {
                        image: none;
                        border-left: 5px solid transparent;
                        border-right: 5px solid transparent;
                        border-top: 5px solid #e8e8e8;
                        margin-right: 5px;
                    }
                    QComboBox QAbstractItemView {
                        background-color: #2a2a2a;
                        color: #e8e8e8;
                        border: 1px solid #3d3d3d;
                        selection-background-color: #6495ed;
                        selection-color: white;
                    }
                )");
                
                connect(fontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &PropertyPanel::onPropertyValueChanged);
                return fontCombo;
            }
            
            // Default: Use QLineEdit for other string properties
            QLineEdit* edit = new QLineEdit();
            edit->setText(value.toString());
            if (isReadonly) {
                edit->setReadOnly(true);
                edit->setStyleSheet(R"(
                    QLineEdit {
                        background-color: #2a2a2a;
                        color: #a0a0a0;
                        border: 1px solid #3d3d3d;
                        border-radius: 6px;
                        padding: 8px 12px;
                        font-style: italic;
                    }
                )");
            } else {
                edit->setStyleSheet(R"(
                    QLineEdit {
                        background-color: #2a2a2a;
                        color: #e8e8e8;
                        border: 1px solid #3d3d3d;
                        border-radius: 6px;
                        padding: 8px 12px;
                        font-size: 11px;
                    }
                    QLineEdit:focus {
                        border: 2px solid #6495ed;
                        background-color: #1e1e1e;
                    }
                    QLineEdit:hover {
                        border: 1px solid #6495ed;
                    }
                )");
                connect(edit, &QLineEdit::textChanged, this, &PropertyPanel::onPropertyValueChanged);
            }
            return edit;
        }
        
        case QMetaType::Int: {
            QSpinBox* spin = new QSpinBox();
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);

            // Set appropriate ranges based on property type
            if (propertyName.contains("Count", Qt::CaseInsensitive) || 
                propertyName.contains("vertices", Qt::CaseInsensitive) ||
                propertyName.contains("subdivisions", Qt::CaseInsensitive) ||
                propertyName.contains("quality", Qt::CaseInsensitive)) {
                // Count properties - must be positive
                spin->setRange(1, 1000);
            } else if (propertyName.contains("precision", Qt::CaseInsensitive)) {
                // Precision for measurements
                spin->setRange(0, 10);
            } else {
                // Default range
                spin->setRange(-10000, 10000);
            }
            
            spin->setValue(value.toInt());
            
            if (isReadonly) {
                spin->setReadOnly(true);
                // Keep readonly fields visually distinct but still readable on dark UI
                spin->setStyleSheet(R"(
                    QSpinBox {
                        background-color: rgba(255,255,255,0.04);
                        color: rgba(231,234,240,0.60);
                        border: 1px solid rgba(255,255,255,0.10);
                        border-radius: 6px;
                        padding: 6px 26px 6px 8px;
                        font-style: italic;
                    }
                )");
            } else {
                // Use panel-global dark stylesheet
                connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                        this, &PropertyPanel::onPropertyValueChanged);
            }
            return spin;
        }
        
        case QMetaType::Double: {
            QDoubleSpinBox* spin = new QDoubleSpinBox();
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);

            // Set appropriate ranges based on property type
            if (propertyName.contains("radius") || propertyName.contains("Width") || 
                propertyName.contains("Height") || propertyName.contains("length", Qt::CaseInsensitive)) {
                // Geometric properties - must be positive
                spin->setRange(0.1, 10000.0);
            } else if (propertyName.contains("angle", Qt::CaseInsensitive)) {
                // Angle properties
                spin->setRange(-360.0, 360.0);
            } else if (propertyName.contains("opacity", Qt::CaseInsensitive) || 
                      propertyName.contains("smoothness", Qt::CaseInsensitive) ||
                      propertyName.contains("tension", Qt::CaseInsensitive)) {
                // Normalized properties (0-1 or 0-2)
                spin->setRange(0.0, 2.0);
            } else {
                // Default range
                spin->setRange(-10000.0, 10000.0);
            }
            
            spin->setDecimals(2);
            spin->setValue(value.toDouble());
            
            if (isReadonly) {
                spin->setReadOnly(true);
                // Keep readonly fields visually distinct but still readable on dark UI
                spin->setStyleSheet(R"(
                    QDoubleSpinBox {
                        background-color: rgba(255,255,255,0.04);
                        color: rgba(231,234,240,0.60);
                        border: 1px solid rgba(255,255,255,0.10);
                        border-radius: 6px;
                        padding: 6px 26px 6px 8px;
                        font-style: italic;
                    }
                )");
            } else {
                // Use panel-global dark stylesheet
                connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                        this, &PropertyPanel::onPropertyValueChanged);
            }
            return spin;
        }
        
        case QMetaType::Bool: {
            QCheckBox* check = new QCheckBox();
            check->setChecked(value.toBool());
            
            // Use panel-global dark stylesheet
            
            if (!isReadonly) {
                connect(check, &QCheckBox::toggled, this, &PropertyPanel::onPropertyValueChanged);
            } else {
                check->setEnabled(false);
            }
            return check;
        }
        
        case QMetaType::QColor: {
            ColorButton* colorBtn = new ColorButton(value.value<QColor>());
            connect(colorBtn, &ColorButton::colorChanged, this, &PropertyPanel::onPropertyValueChanged);
            return colorBtn;
        }
        
        default:
            return nullptr;
    }
}

void PropertyPanel::refreshCurrentProperties()
{
    // Refresh the current object's properties without changing selection
    if (m_currentPrimitive) {
        // Re-show primitive properties to refresh values
        DrawingPrimitive* primitive = m_currentPrimitive;
        m_currentPrimitive = nullptr; // Reset to force refresh
        showPrimitiveProperties(primitive);
    }
    else if (m_currentPrimitive) {
        // Re-show primitive properties to refresh values
        DrawingPrimitive* primitive = m_currentPrimitive;
        m_currentPrimitive = nullptr; // Reset to force refresh
        showPrimitiveProperties(primitive);
    }
    else if (!m_currentPrimitives.empty()) {
        // Re-show multiple primitive properties
        auto primitives = m_currentPrimitives;
        m_currentPrimitives.clear();
        showMultiplePrimitiveProperties(primitives);
    }
}

void PropertyPanel::onPropertyValueChanged()
{
    QObject* sender = this->sender();
    QString propertyName;
    QVariant value;
    
    qDebug() << "PropertyPanel::onPropertyValueChanged() called - sender:" << sender;
    qDebug() << "Current object name:" << m_currentObjectName;
    
    // Find the property name
    for (auto it = m_propertyEditors.begin(); it != m_propertyEditors.end(); ++it) {
        if (it.value() == sender) {
            propertyName = it.key();
            break;
        }
    }
    
    if (propertyName.isEmpty()) {
        qDebug() << "WARNING: PropertyPanel - property name not found for sender" << sender;
        return;
    }
    
    // Get the value based on widget type
    if (auto textEdit = qobject_cast<QTextEdit*>(sender)) {
        value = textEdit->toPlainText();
    }
    else if (auto edit = qobject_cast<QLineEdit*>(sender)) {
        value = edit->text();
    }
    else if (auto spin = qobject_cast<QSpinBox*>(sender)) {
        value = spin->value();
    }
    else if (auto spin = qobject_cast<QDoubleSpinBox*>(sender)) {
        value = spin->value();
    }
    else if (auto check = qobject_cast<QCheckBox*>(sender)) {
        value = check->isChecked();
    }
    else if (auto colorBtn = qobject_cast<ColorButton*>(sender)) {
        value = colorBtn->color();
    }
    else if (auto combo = qobject_cast<QComboBox*>(sender)) {
        if (propertyName == "paperFormat") {
            // For paper format, return the enum value
            value = combo->currentData().toInt();
        } else if (propertyName == "lineStyle") {
            // For line style, return enum value
            value = combo->currentData().toInt();
        } else {
            value = combo->currentText();
        }
    }
    
    qDebug() << "About to emit propertyChanged signal - Object:" << m_currentObjectName << ", Property:" << propertyName << ", Value:" << value;
    emit propertyChanged(m_currentObjectName, propertyName, value);
    
    qDebug() << "Property changed signal emitted successfully";
}

