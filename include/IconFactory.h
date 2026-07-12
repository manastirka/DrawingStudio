#pragma once

// Header-only vector icon factory for the text/property panels.
//
// Every icon is painted with QPainter into a QPixmap rendered at 2x device
// pixel ratio so glyphs stay crisp on retina/HiDPI displays. Foreground colour
// is parameterised so callers can render a normal (light gray) or accent/checked
// (blue) variant. No external assets and no Qt6::Svg dependency.

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QColor>
#include <QRectF>
#include <QPointF>

namespace IconFactory {

// Palette-friendly defaults matching the dark Fusion theme.
inline QColor defaultColor() { return QColor(0xe0, 0xe0, 0xe0); } // #e0e0e0
inline QColor accentColor()  { return QColor(0x2a, 0x82, 0xda); } // #2a82da

namespace detail {

// Logical icon edge in points; physical pixmap is 2x for retina crispness.
constexpr int kLogicalSize = 20;
constexpr qreal kScale = 2.0;

// Creates a transparent pixmap sized for a `logical`-point square icon at 2x
// device pixel ratio and prepares an antialiased painter. Once the pixmap has
// a device pixel ratio, QPainter automatically works in logical coordinates
// (0..logical), so no explicit scale transform is applied.
inline QPixmap makeCanvas(QPainter& painter, int logical = kLogicalSize)
{
    QPixmap pm(static_cast<int>(logical * kScale), static_cast<int>(logical * kScale));
    pm.setDevicePixelRatio(kScale);
    pm.fill(Qt::transparent);
    painter.begin(&pm);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    return pm;
}

// Draws a centered letter using a bold/italic/underline font configuration.
inline QPixmap letterGlyph(const QColor& color, const QString& letter,
                           bool bold, bool italic, bool underline,
                           qreal pointSize = 13.0)
{
    QPainter p;
    QPixmap pm = makeCanvas(p);

    QFont f("Arial", 1);
    f.setPointSizeF(pointSize);
    f.setBold(bold);
    f.setItalic(italic);
    f.setUnderline(underline);
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRectF(0, 0, kLogicalSize, kLogicalSize), Qt::AlignCenter, letter);
    p.end();
    return pm;
}

} // namespace detail

// --- Font style toggles ----------------------------------------------------

inline QIcon bold(const QColor& color = defaultColor())
{
    return QIcon(detail::letterGlyph(color, QStringLiteral("B"), true, false, false, 14.0));
}

inline QIcon italic(const QColor& color = defaultColor())
{
    return QIcon(detail::letterGlyph(color, QStringLiteral("I"), false, true, false, 14.0));
}

inline QIcon underline(const QColor& color = defaultColor())
{
    // Draw the "U" then a dedicated underline stroke for a heavier, clearer bar.
    QPainter p;
    QPixmap pm = detail::makeCanvas(p);

    QFont f("Arial", 1);
    f.setPointSizeF(13.0);
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRectF(0, 0, detail::kLogicalSize, detail::kLogicalSize - 3),
               Qt::AlignCenter, QStringLiteral("U"));

    QPen bar(color);
    bar.setWidthF(1.6);
    bar.setCapStyle(Qt::RoundCap);
    p.setPen(bar);
    p.drawLine(QPointF(5.5, 16.5), QPointF(14.5, 16.5));
    p.end();
    return QIcon(pm);
}

// --- Alignment glyphs (universal horizontal-bar style) ----------------------

namespace detail {

inline QIcon alignGlyph(const QColor& color, Qt::Alignment align, bool justify)
{
    QPainter p;
    QPixmap pm = makeCanvas(p);

    QPen pen(color);
    pen.setWidthF(1.7);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);

    const qreal left = 4.0;
    const qreal right = kLogicalSize - 4.0;
    const qreal full = right - left;
    const qreal ys[4] = {5.5, 8.5, 11.5, 14.5};
    // Alternating bar lengths give the classic "paragraph" look.
    const qreal lens[4] = {full, full * 0.62, full * 0.85, full * 0.5};

    for (int i = 0; i < 4; ++i) {
        qreal len = justify ? full : lens[i];
        qreal x0 = left;
        if (!justify) {
            if (align == Qt::AlignRight)       x0 = right - len;
            else if (align == Qt::AlignHCenter) x0 = left + (full - len) / 2.0;
        }
        p.drawLine(QPointF(x0, ys[i]), QPointF(x0 + len, ys[i]));
    }
    p.end();
    return QIcon(pm);
}

} // namespace detail

inline QIcon alignLeft(const QColor& color = defaultColor())
{
    return detail::alignGlyph(color, Qt::AlignLeft, false);
}

inline QIcon alignCenter(const QColor& color = defaultColor())
{
    return detail::alignGlyph(color, Qt::AlignHCenter, false);
}

inline QIcon alignRight(const QColor& color = defaultColor())
{
    return detail::alignGlyph(color, Qt::AlignRight, false);
}

inline QIcon alignJustify(const QColor& color = defaultColor())
{
    return detail::alignGlyph(color, Qt::AlignLeft, true);
}

// --- Font size ------------------------------------------------------------

namespace detail {

// Big "A" paired with a small +/- (or small "A") to imply grow/shrink.
inline QIcon fontSizeGlyph(const QColor& color, bool increase)
{
    QPainter p;
    QPixmap pm = makeCanvas(p);
    p.setPen(color);

    QFont big("Arial", 1);
    big.setPointSizeF(13.0);
    big.setBold(true);
    p.setFont(big);
    p.drawText(QRectF(0, 1, 13, kLogicalSize - 1), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("A"));

    QPen pen(color);
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    const qreal cx = 15.5;
    const qreal cy = 6.0;
    const qreal r = 2.6;
    p.drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy)); // horizontal (minus)
    if (increase) {
        p.drawLine(QPointF(cx, cy - r), QPointF(cx, cy + r)); // vertical -> plus
    }
    p.end();
    return QIcon(pm);
}

} // namespace detail

inline QIcon fontIncrease(const QColor& color = defaultColor())
{
    return detail::fontSizeGlyph(color, true);
}

inline QIcon fontDecrease(const QColor& color = defaultColor())
{
    return detail::fontSizeGlyph(color, false);
}

// --- Sub / superscript -----------------------------------------------------

namespace detail {

inline QIcon scriptGlyph(const QColor& color, bool superscript)
{
    QPainter p;
    QPixmap pm = makeCanvas(p);
    p.setPen(color);

    QFont base("Arial", 1);
    base.setPointSizeF(12.0);
    p.setFont(base);
    p.drawText(QRectF(2, superscript ? 3 : 0, 12, kLogicalSize - 3),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("x"));

    QFont small("Arial", 1);
    small.setPointSizeF(8.0);
    small.setBold(true);
    p.setFont(small);
    const qreal sy = superscript ? -3.0 : 4.0;
    p.drawText(QRectF(11, 2 + sy, 8, kLogicalSize - 4),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("2"));
    p.end();
    return QIcon(pm);
}

} // namespace detail

inline QIcon subscript(const QColor& color = defaultColor())
{
    return detail::scriptGlyph(color, false);
}

inline QIcon superscript(const QColor& color = defaultColor())
{
    return detail::scriptGlyph(color, true);
}

// --- Text colour (letter "A" over a colour swatch bar) ----------------------

inline QIcon textColor(const QColor& swatch,
                       const QColor& glyphColor = defaultColor())
{
    QPainter p;
    QPixmap pm = detail::makeCanvas(p);

    QFont f("Arial", 1);
    f.setPointSizeF(12.0);
    f.setBold(true);
    p.setFont(f);
    p.setPen(glyphColor);
    p.drawText(QRectF(0, -1, detail::kLogicalSize, detail::kLogicalSize - 2),
               Qt::AlignCenter, QStringLiteral("A"));

    // Colour swatch bar under the glyph reflecting the current text colour.
    QRectF bar(3.0, 15.5, detail::kLogicalSize - 6.0, 3.0);
    QPainterPath path;
    path.addRoundedRect(bar, 1.2, 1.2);
    p.fillPath(path, swatch);
    p.end();
    return QIcon(pm);
}

// --- Effect glyphs (shadow / stroke / gradient) -----------------------------

inline QIcon shadow(const QColor& color = defaultColor())
{
    QPainter p;
    QPixmap pm = detail::makeCanvas(p);

    // Offset "shadow" square behind a foreground outlined square.
    QColor shade = color;
    shade.setAlpha(90);
    QPainterPath back;
    back.addRoundedRect(QRectF(7.0, 7.0, 9.5, 9.5), 1.5, 1.5);
    p.fillPath(back, shade);

    QPen pen(color);
    pen.setWidthF(1.5);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(3.5, 3.5, 9.5, 9.5), 1.5, 1.5);
    p.end();
    return QIcon(pm);
}

inline QIcon stroke(const QColor& color = defaultColor())
{
    QPainter p;
    QPixmap pm = detail::makeCanvas(p);

    // Thick outlined circle with a hollow center = "outline/stroke".
    QPen pen(color);
    pen.setWidthF(2.4);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(4.0, 4.0, 12.0, 12.0));
    p.end();
    return QIcon(pm);
}

inline QIcon gradient(const QColor& color = defaultColor())
{
    QPainter p;
    QPixmap pm = detail::makeCanvas(p);

    QRectF box(3.5, 3.5, 13.0, 13.0);
    QLinearGradient g(box.topLeft(), box.bottomRight());
    QColor c0 = color; c0.setAlpha(235);
    QColor c1 = color; c1.setAlpha(45);
    g.setColorAt(0.0, c0);
    g.setColorAt(1.0, c1);

    QPainterPath path;
    path.addRoundedRect(box, 2.0, 2.0);
    p.fillPath(path, g);

    QPen pen(color);
    pen.setWidthF(1.2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(box, 2.0, 2.0);
    p.end();
    return QIcon(pm);
}

} // namespace IconFactory
