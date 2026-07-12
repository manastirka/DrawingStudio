#include "ColorPalette.h"
#include <QPushButton>
#include <QGridLayout>
#include <QSignalMapper>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QDebug>

// Default color palette - Pantone-inspired professional colors
const QVector<QColor> ColorPalette::s_defaultColors = {
    // Basic colors
    QColor(0, 0, 0),         // Black
    QColor(255, 255, 255),   // White
    QColor(128, 128, 128),   // Gray
    QColor(64, 64, 64),      // Dark Gray
    
    // Reds - Pantone inspired
    QColor(200, 16, 46),     // True Red
    QColor(227, 6, 19),      // Fiery Red
    QColor(175, 32, 36),     // Ribbon Red
    QColor(193, 39, 45),     // Flame Scarlet
    QColor(206, 22, 32),     // Racing Red
    QColor(142, 40, 41),     // Burgundy
    
    // Oranges
    QColor(255, 88, 0),      // Orange Tiger
    QColor(255, 103, 31),    // Vibrant Orange
    QColor(247, 127, 0),     // Tangerine
    QColor(221, 117, 58),    // Apricot
    QColor(214, 93, 14),     // Autumn Glory
    
    // Yellows & Golds
    QColor(254, 221, 0),     // Lemon Chrome
    QColor(255, 200, 8),     // Golden Yellow
    QColor(255, 179, 0),     // Marigold
    QColor(252, 209, 22),    // Buttercup
    QColor(207, 162, 44),    // Honey Gold
    
    // Greens
    QColor(0, 122, 51),      // Kelly Green
    QColor(0, 163, 104),     // Emerald
    QColor(0, 177, 64),      // Classic Green
    QColor(122, 184, 0),     // Greenery (Pantone 2017)
    QColor(0, 132, 61),      // Jelly Bean
    QColor(0, 104, 55),      // Verdant Green
    QColor(0, 158, 96),      // Bosphorus
    QColor(137, 176, 174),   // Yucca
    
    // Blues
    QColor(0, 114, 188),     // Brilliant Blue
    QColor(0, 131, 143),     // Enamel Blue
    QColor(0, 163, 224),     // Blue Atoll
    QColor(0, 114, 206),     // Dazzling Blue
    QColor(0, 51, 160),      // Reflex Blue
    QColor(0, 71, 187),      // Royal Blue
    QColor(0, 102, 153),     // Mykonos Blue
    QColor(102, 153, 204),   // Placid Blue
    
    // Purples & Violets
    QColor(111, 78, 155),    // Ultra Violet (Pantone 2018)
    QColor(102, 45, 145),    // Purple
    QColor(146, 39, 143),    // Purple Orchid
    QColor(195, 68, 122),    // Fuchsia Rose
    QColor(213, 43, 127),    // Pink Yarrow
    QColor(175, 110, 169),   // Bodacious
    
    // Pinks
    QColor(255, 105, 180),   // Hot Pink
    QColor(247, 152, 196),   // Carnation Pink
    QColor(255, 182, 193),   // Powder Pink
    QColor(219, 112, 147),   // Pale Violet Red
    QColor(244, 194, 194),   // Almond Blossom
    
    // Browns & Neutrals
    QColor(121, 85, 72),     // Cognac
    QColor(150, 113, 91),    // Toasted Almond
    QColor(175, 141, 120),   // Warm Taupe
    QColor(139, 115, 85),    // Toffee
    QColor(188, 152, 126),   // Café au Lait
    
    // Pastels
    QColor(173, 216, 230),   // Powder Blue
    QColor(221, 242, 228),   // Hint of Mint
    QColor(255, 239, 213),   // Papaya Whip
    QColor(250, 218, 221),   // Pale Dogwood
    QColor(245, 228, 217),   // Peach Nougat
    
    // Additional vibrant colors
    QColor(0, 188, 212),     // Turquoise
    QColor(0, 150, 136),     // Teal
    QColor(156, 39, 176),    // Deep Purple
    QColor(233, 30, 99),     // Deep Pink
    QColor(255, 152, 0),     // Deep Orange
    QColor(205, 220, 57),    // Lime
};

ColorPalette::ColorPalette(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_currentColor(QColor(0, 0, 0)) // Default to black
    , m_currentButton(nullptr)
    , m_colorSize(24)
    , m_selectedIndex(0)
{
    setupPalette();
    setMouseTracking(true);
    
    // Set fixed height but allow horizontal resizing
    setFixedHeight(m_colorSize);
    setMinimumWidth(200); // Minimum width to show at least some colors
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

ColorPalette::~ColorPalette()
{
}

void ColorPalette::setupPalette()
{
    // No layout needed - we'll paint directly
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setContentsMargins(0, 0, 0, 0);
}

void ColorPalette::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false); // Sharp edges for pixel-perfect alignment
    
    // Only paint colors that are visible in the current widget width
    int visibleColors = qMin(s_defaultColors.size(), width() / m_colorSize + 1);
    
    for (int i = 0; i < visibleColors && i < s_defaultColors.size(); ++i) {
        QRect colorRect(i * m_colorSize, 0, m_colorSize, m_colorSize);
        
        // Skip if outside visible area
        if (colorRect.left() >= width()) {
            break;
        }
        
        // Fill with color
        painter.fillRect(colorRect, s_defaultColors[i]);
        
        // Draw selection border if this is the selected color
        if (i == m_selectedIndex) {
            painter.setPen(QPen(Qt::white, 2));
            painter.drawRect(colorRect.adjusted(1, 1, -1, -1));
        }
    }
}

void ColorPalette::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int index = event->pos().x() / m_colorSize;
        if (index >= 0 && index < s_defaultColors.size()) {
            m_selectedIndex = index;
            m_currentColor = s_defaultColors[index];
            emit colorChanged(m_currentColor);
            update();
        }
    }
}

void ColorPalette::createColorButton(const QColor &color, int index)
{
    // This method is no longer used - keeping for compatibility
}

void ColorPalette::onColorButtonClicked(const QColor &color)
{
    // This method is no longer used - keeping for compatibility
}

void ColorPalette::setCurrentColor(const QColor &color)
{
    if (m_currentColor == color) {
        return;
    }
    
    m_currentColor = color;
    
    // Find the index of this color
    for (int i = 0; i < s_defaultColors.size(); ++i) {
        if (s_defaultColors[i] == color) {
            m_selectedIndex = i;
            break;
        }
    }
    
    update();
}

void ColorPalette::updateButtonStyles()
{
    // This method is no longer used - keeping for compatibility
}
