#include "ColorfulSpinner.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

ColorfulSpinner::ColorfulSpinner(QWidget* parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_angle(0)
    , m_progress(0)
    , m_isAnimating(false)
{
    m_timer->setInterval(50);  // 20 FPS for smooth animation
    connect(m_timer, &QTimer::timeout, this, &ColorfulSpinner::rotate);
    
    setMinimumSize(32, 32);
    setAttribute(Qt::WA_TranslucentBackground);
}

void ColorfulSpinner::start() {
    m_isAnimating = true;
    m_timer->start();
}

void ColorfulSpinner::stop() {
    m_isAnimating = false;
    m_timer->stop();
    update();
}

void ColorfulSpinner::setProgress(int percentage) {
    m_progress = qBound(0, percentage, 100);
    update();
}

void ColorfulSpinner::rotate() {
    m_angle = (m_angle + 15) % 360;  // Rotate 15 degrees per frame
    update();
}

QColor ColorfulSpinner::interpolateColor(int segmentIndex, float colorProgress) const {
    // Define rainbow colors for 8 segments
    static const QColor rainbowColors[] = {
        QColor(255, 0, 0),      // Red
        QColor(255, 127, 0),    // Orange
        QColor(255, 255, 0),    // Yellow
        QColor(0, 255, 0),      // Green
        QColor(0, 255, 255),    // Cyan
        QColor(0, 0, 255),      // Blue
        QColor(139, 0, 255),    // Purple
        QColor(255, 0, 255)     // Magenta
    };
    
    // Start with grayscale
    int grayValue = 255 - (segmentIndex * 30);  // Darker segments
    grayValue = qBound(50, grayValue, 255);
    QColor grayColor(grayValue, grayValue, grayValue);
    
    // Get target rainbow color
    QColor targetColor = rainbowColors[segmentIndex % 8];
    
    // Interpolate between gray and color based on progress
    int r = grayColor.red() + (targetColor.red() - grayColor.red()) * colorProgress;
    int g = grayColor.green() + (targetColor.green() - grayColor.green()) * colorProgress;
    int b = grayColor.blue() + (targetColor.blue() - grayColor.blue()) * colorProgress;
    
    return QColor(r, g, b);
}

void ColorfulSpinner::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int side = qMin(width(), height());
    painter.setViewport((width() - side) / 2, (height() - side) / 2, side, side);
    painter.setWindow(-50, -50, 100, 100);
    
    // Rotate the entire coordinate system
    painter.rotate(m_angle);
    
    // Calculate color transition progress (0.0 to 1.0)
    float colorProgress = m_progress / 100.0f;
    
    // Draw 8 segments in a circle
    const int numSegments = 8;
    const float segmentAngle = 360.0f / numSegments;
    
    for (int i = 0; i < numSegments; ++i) {
        float startAngle = i * segmentAngle;
        
        // Create gradient for each segment (fade out towards the end)
        float opacity = 1.0f - (i / (float)numSegments) * 0.7f;
        
        // Get interpolated color
        QColor segmentColor = interpolateColor(i, colorProgress);
        segmentColor.setAlphaF(opacity);
        
        // Draw arc segment
        QPainterPath path;
        path.moveTo(0, 0);
        path.arcTo(-35, -35, 70, 70, startAngle, segmentAngle * 0.8f);  // 0.8 for gaps
        path.lineTo(0, 0);
        
        painter.fillPath(path, segmentColor);
    }
    
    // Draw center circle (white with slight transparency)
    QColor centerColor = Qt::white;
    centerColor.setAlphaF(0.9f);
    painter.setBrush(centerColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), 15, 15);
}
