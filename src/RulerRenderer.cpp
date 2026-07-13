#include "RulerRenderer.h"

#include <QFont>
#include <QPainter>
#include <QRect>

#include <cmath>

void RulerRenderer::renderTicks(QPainter &painter, int widgetWidth,
                                int widgetHeight, float zoomLevel,
                                UnitsConverter::Units units)
{
    painter.save();
    painter.resetTransform();

    QColor rulerBg(230, 230, 230, 230);
    painter.fillRect(QRect(WIDTH, 0, widgetWidth - WIDTH, HEIGHT), rulerBg);
    painter.fillRect(QRect(0, HEIGHT, WIDTH, widgetHeight - HEIGHT), rulerBg);

    QColor cornerBg(204, 204, 204, 230);
    painter.fillRect(QRect(0, 0, WIDTH, HEIGHT), cornerBg);

    QPen tickPen(QColor(51, 51, 51), 1.0);
    painter.setPen(tickPen);

    float majorSpacing = 100.0f;
    if (units == UnitsConverter::Units::Centimeters) {
        majorSpacing = 10.0f;
    } else if (units == UnitsConverter::Units::Inches) {
        majorSpacing = 1.0f;
    }

    float majorScreenSpacing = majorSpacing * zoomLevel;
    if (majorScreenSpacing < 30) {
        majorScreenSpacing *= 2;
    }

    const QPoint originScreen(WIDTH, HEIGHT);

    const int leftMarks =
        static_cast<int>((originScreen.x() - WIDTH) / majorScreenSpacing) + 1;
    const int rightMarks =
        static_cast<int>((widgetWidth - originScreen.x()) / majorScreenSpacing) +
        1;

    for (int i = -leftMarks; i <= rightMarks; ++i) {
        int x = originScreen.x() + static_cast<int>(i * majorScreenSpacing);
        if (x < WIDTH || x >= widgetWidth) {
            continue;
        }
        painter.drawLine(x, HEIGHT - 15, x, HEIGHT);
    }

    const int topMarks =
        static_cast<int>((originScreen.y() - HEIGHT) / majorScreenSpacing) + 1;
    const int bottomMarks =
        static_cast<int>((widgetHeight - originScreen.y()) / majorScreenSpacing) +
        1;

    for (int i = -bottomMarks; i <= topMarks; ++i) {
        int y = originScreen.y() - static_cast<int>(i * majorScreenSpacing);
        if (y < HEIGHT || y >= widgetHeight) {
            continue;
        }
        painter.drawLine(WIDTH - 15, y, WIDTH, y);
    }

    painter.restore();
}

void RulerRenderer::renderTexts(
    QPainter &painter, int widgetWidth, int widgetHeight,
    UnitsConverter::Units units,
    const std::function<QPoint(const QVector2D &)> &worldToScreen)
{
    painter.save();
    painter.resetTransform();

    // Simple world bounds used by the previous canvas implementation
    const QRectF viewportWorld(-1000, -1000, 2000, 2000);

    float majorSpacing = 100.0f;
    float unitScale = 1.0f;
    switch (units) {
    case UnitsConverter::Units::Millimeters:
        unitScale = 1.0f;
        majorSpacing = 100.0f;
        break;
    case UnitsConverter::Units::Centimeters:
        unitScale = 0.1f;
        majorSpacing = 10.0f;
        break;
    case UnitsConverter::Units::Inches:
        unitScale = 0.0393701f;
        majorSpacing = 5.0f;
        break;
    }

    painter.setPen(QColor(80, 80, 80));
    QFont font(QStringLiteral("Arial"), 9, QFont::Bold);
    painter.setFont(font);

    const float startX =
        std::floor(viewportWorld.left() / majorSpacing) * majorSpacing;
    const float endX =
        std::ceil(viewportWorld.right() / majorSpacing) * majorSpacing;

    for (float x = startX; x <= endX; x += majorSpacing) {
        QPoint screenPos = worldToScreen(QVector2D(x, static_cast<float>(viewportWorld.top())));
        if (screenPos.x() >= WIDTH && screenPos.x() <= widgetWidth - 10) {
            float displayValue = x * unitScale;
            QString text;
            if (displayValue == static_cast<int>(displayValue)) {
                text = QString::number(static_cast<int>(displayValue));
            } else {
                text = QString::number(displayValue, 'f', 1);
            }
            QRect textRect = painter.fontMetrics().boundingRect(text);
            painter.drawText(screenPos.x() - textRect.width() / 2, 20, text);
        }
    }

    const float startY =
        std::floor(viewportWorld.top() / majorSpacing) * majorSpacing;
    const float endY =
        std::ceil(viewportWorld.bottom() / majorSpacing) * majorSpacing;

    for (float y = startY; y <= endY; y += majorSpacing) {
        QPoint screenPos =
            worldToScreen(QVector2D(static_cast<float>(viewportWorld.left()), y));
        if (screenPos.y() >= HEIGHT && screenPos.y() <= widgetHeight - 10) {
            float displayValue = y * unitScale;
            QString text;
            if (displayValue == static_cast<int>(displayValue)) {
                text = QString::number(static_cast<int>(displayValue));
            } else {
                text = QString::number(displayValue, 'f', 1);
            }
            painter.save();
            painter.translate(15, screenPos.y() + 5);
            painter.rotate(-90);
            painter.drawText(0, 0, text);
            painter.restore();
        }
    }

    QPoint originScreen = worldToScreen(QVector2D(0, 0));
    if (originScreen.x() >= WIDTH && originScreen.x() <= widgetWidth - 10 &&
        originScreen.y() >= HEIGHT && originScreen.y() <= widgetHeight - 10) {
        painter.setPen(QColor(200, 0, 0));
        QFont originFont(QStringLiteral("Arial"), 8, QFont::Bold);
        painter.setFont(originFont);
        painter.drawText(originScreen.x() - 3, 18, QStringLiteral("0"));
        painter.save();
        painter.translate(15, originScreen.y() + 3);
        painter.rotate(-90);
        painter.drawText(0, 0, QStringLiteral("0"));
        painter.restore();
    }

    painter.restore();
}
