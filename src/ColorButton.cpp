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

// Color swatch button (refactor E17).

ColorButton::ColorButton(const QColor& color, QWidget* parent)
    : QPushButton(parent), m_color(color)
{
    setFixedSize(28, 18);
    setCursor(Qt::PointingHandCursor);
    updateStyle();
}

void ColorButton::setColor(const QColor& color)
{
    if (m_color != color) {
        m_color = color;
        updateStyle();
        emit colorChanged(color);
    }
}

void ColorButton::updateStyle()
{
    setStyleSheet(QString(R"(
        ColorButton {
            background-color: %1;
            border: 1px solid rgba(255,255,255,0.14);
            border-radius: 5px;
            padding: 0;
        }
        ColorButton:hover {
            border: 1px solid rgba(138, 180, 255, 0.85);
        }
    )").arg(m_color.name()));
}

void ColorButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF colorRect = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);

    // Transparency checker
    if (m_color.alpha() < 255) {
        const int s = 3;
        QColor c1(255, 255, 255, 28);
        QColor c2(0, 0, 0, 28);
        for (int y = (int)colorRect.top(); y < (int)colorRect.bottom(); y += s) {
            for (int x = (int)colorRect.left(); x < (int)colorRect.right(); x += s) {
                const bool odd = ((x / s) + (y / s)) % 2;
                painter.fillRect(QRect(x, y, s, s), odd ? c1 : c2);
            }
        }
    }

    QPainterPath path;
    path.addRoundedRect(colorRect, 3.5, 3.5);
    painter.fillPath(path, m_color);
}

void ColorButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QColor newColor = QColorDialog::getColor(m_color, this, "Select Color");
        if (newColor.isValid()) {
            setColor(newColor);
        }
    }
    QPushButton::mousePressEvent(event);
}

// PropertyPanel Implementation

