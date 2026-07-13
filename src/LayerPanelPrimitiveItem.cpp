#include "LayerPanel.h"
#include "Commands.h"
#include "Layer.h"
#include "LayerManager.h"
#include "DrawingPrimitive.h"

#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QColorDialog>
#include <QDebug>
#include <QSignalBlocker>
#include <QStyle>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidgetItem>
#include <QUuid>
#include <QIcon>
#include <QSize>

// Primitive row widget in layer list (refactor E18).

PrimitiveItemWidget::PrimitiveItemWidget(DrawingPrimitive* primitive, QWidget* parent)
    : QWidget(parent)
    , m_primitive(primitive)
    , m_isSelected(false)
{
    setupUI();
    updateFromPrimitive();
    setFixedHeight(25); // Smaller than layer items
}

PrimitiveItemWidget::~PrimitiveItemWidget()
{
}

void PrimitiveItemWidget::setupUI()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(20, 2, 5, 2); // Indent to show hierarchy
    m_layout->setSpacing(5);
    
    // Visibility button
    m_visibilityButton = new QToolButton();
    m_visibilityButton->setFixedSize(16, 16);
    m_visibilityButton->setText("👁");
    m_visibilityButton->setStyleSheet("font-size: 10px;");
    m_visibilityButton->setToolTip("Toggle primitive visibility");
    connect(m_visibilityButton, &QToolButton::clicked, this, &PrimitiveItemWidget::onVisibilityClicked);
    
    // Type label
    m_typeLabel = new QLabel();
    m_typeLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: #444;");
    m_typeLabel->setMinimumWidth(60);
    
    // Description label
    m_descriptionLabel = new QLabel();
    m_descriptionLabel->setStyleSheet("font-size: 10px; color: #666;");
    
    m_layout->addWidget(m_visibilityButton);
    m_layout->addWidget(m_typeLabel);
    m_layout->addWidget(m_descriptionLabel, 1); // Take remaining space
}

void PrimitiveItemWidget::updateFromPrimitive()
{
    if (!m_primitive) return;
    
    m_typeLabel->setText(getPrimitiveTypeName());
    m_descriptionLabel->setText(getPrimitiveDescription());
    
    // Update visibility icon
    if (m_primitive->isVisible()) {
        m_visibilityButton->setStyleSheet("font-size: 10px; color: black;");
    } else {
        m_visibilityButton->setStyleSheet("font-size: 10px; color: #ccc;");
    }
    
    update();
}

QString PrimitiveItemWidget::getPrimitiveTypeName() const
{
    if (!m_primitive) return "Unknown";
    
    switch (m_primitive->type()) {
        case PrimitiveType::Line: return "Line";
        case PrimitiveType::Curve: return "Curve";
        case PrimitiveType::BezierCurve: return "Bézier";
        case PrimitiveType::Spline: return "Spline";
        case PrimitiveType::Arc: return "Arc";
        case PrimitiveType::Circle: return "Circle";
        case PrimitiveType::Rectangle: return "Rectangle";
        case PrimitiveType::Ellipse: return "Ellipse";
        case PrimitiveType::Polygon: return "Polygon";
        case PrimitiveType::Text: return "Text";
        case PrimitiveType::Dimension: return "Dimension";
        default: return "Unknown";
    }
}

QString PrimitiveItemWidget::getPrimitiveDescription() const
{
    if (!m_primitive) return "";
    
    // Return a brief description based on primitive type
    switch (m_primitive->type()) {
        case PrimitiveType::Rectangle: {
            auto rect = dynamic_cast<RectanglePrimitive*>(m_primitive);
            if (rect) {
                QVector2D tl = rect->topLeft();
                QVector2D br = rect->bottomRight();
                float width = abs(br.x() - tl.x());
                float height = abs(br.y() - tl.y());
                return QString("%1×%2").arg(QString::number(width, 'f', 1))
                                       .arg(QString::number(height, 'f', 1));
            }
            break;
        }
        case PrimitiveType::Line: {
            auto line = dynamic_cast<LinePrimitive*>(m_primitive);
            if (line) {
                float length = (line->endPoint() - line->startPoint()).length();
                return QString("Length: %1").arg(QString::number(length, 'f', 1));
            }
            break;
        }
        default:
            return QString("Object #%1").arg(reinterpret_cast<quintptr>(m_primitive) & 0xFFFF);
    }
    return "";
}

void PrimitiveItemWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    
    if (m_isSelected) {
        painter.fillRect(rect(), QColor(100, 150, 255, 30)); // Light blue selection
    } else {
        painter.fillRect(rect(), QColor(250, 250, 250)); // Very light gray background
    }
    
    QWidget::paintEvent(event);
}

void PrimitiveItemWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit primitiveSelected(m_primitive);
    }
    QWidget::mousePressEvent(event);
}

void PrimitiveItemWidget::onVisibilityClicked()
{
    if (m_primitive) {
        bool newVisibility = !m_primitive->isVisible();
        emit primitiveVisibilityToggled(m_primitive, newVisibility);
    }
}

// LayerItemWidget Implementation

