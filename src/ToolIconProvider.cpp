#include "ToolIconProvider.h"

#include <QColor>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <QString>

QIcon ToolIconProvider::createMonoIcon(
    const std::function<void(QPainter&, const QRectF&)> &draw)
{
    // Affinity/Photoshop style: bright glyph; solid white when tool is checked
    // (button chrome turns blue).
    const QColor offColor(220, 224, 230);
    const QColor onColor(255, 255, 255);
    const QColor disabledColor(220, 224, 230, 70);

    const int logical = 22;
    const qreal dpr = 3.0;
    const QRectF drawRect(2.5, 2.5, logical - 5.0, logical - 5.0);

    auto makePixmap = [&](const QColor& color) {
        QPixmap pixmap(int(logical * dpr), int(logical * dpr));
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform
                               | QPainter::TextAntialiasing);
        QPen pen(color, 1.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        draw(painter, drawRect);
        return pixmap;
    };

    QIcon icon;
    icon.addPixmap(makePixmap(offColor), QIcon::Normal, QIcon::Off);
    icon.addPixmap(makePixmap(onColor),  QIcon::Normal, QIcon::On);
    icon.addPixmap(makePixmap(offColor), QIcon::Active, QIcon::Off);
    icon.addPixmap(makePixmap(onColor),  QIcon::Active, QIcon::On);
    icon.addPixmap(makePixmap(offColor), QIcon::Selected, QIcon::Off);
    icon.addPixmap(makePixmap(onColor),  QIcon::Selected, QIcon::On);
    icon.addPixmap(makePixmap(disabledColor), QIcon::Disabled, QIcon::Off);
    icon.addPixmap(makePixmap(disabledColor), QIcon::Disabled, QIcon::On);
    return icon;
}

QIcon ToolIconProvider::createSelectIcon()
{
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QColor c = p.pen().color();
        const qreal L = r.left(), T = r.top();
        QPainterPath arrow;
        arrow.moveTo(L + 1.2, T + 0.6);
        arrow.lineTo(L + 1.2, T + 14.0);
        arrow.lineTo(L + 4.6, T + 10.8);
        arrow.lineTo(L + 6.8, T + 15.6);
        arrow.lineTo(L + 8.9, T + 14.7);
        arrow.lineTo(L + 6.6, T + 9.8);
        arrow.lineTo(L + 11.2, T + 9.8);
        arrow.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(arrow);
    });
}

QIcon ToolIconProvider::createLineIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QPointF a(r.left() + 1.5, r.bottom() - 1.5);
        const QPointF b(r.right() - 1.5, r.top() + 1.5);
        p.drawLine(a, b);
        const QColor c = p.pen().color();
        p.setBrush(c);
        p.drawEllipse(a, 1.6, 1.6);
        p.drawEllipse(b, 1.6, 1.6);
    });
}

QIcon ToolIconProvider::createAngleLineIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QPointF origin(r.left() + 2.5, r.bottom() - 2.5);
        p.drawLine(origin, QPointF(r.right() - 1.5, r.bottom() - 2.5));
        p.drawLine(origin, QPointF(r.left() + 11.0, r.top() + 2.0));
        p.drawArc(QRectF(origin.x() - 5.5, origin.y() - 5.5, 11.0, 11.0), 0 * 16, 55 * 16);
    });
}

QIcon ToolIconProvider::createCurveIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        QPainterPath path;
        path.moveTo(r.left() + 1.0, r.bottom() - 2.0);
        path.cubicTo(r.left() + 2.0, r.top() + 1.0,
                     r.right() - 2.0, r.top() + 1.0,
                     r.right() - 1.0, r.bottom() - 2.0);
        p.drawPath(path);
    });
}

QIcon ToolIconProvider::createBezierIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QPointF a1(r.left() + 1.5, r.bottom() - 1.5);
        const QPointF a2(r.right() - 1.5, r.top() + 2.0);
        const QPointF h1(r.left() + 5.5, r.top() + 3.0);
        const QPointF h2(r.right() - 5.5, r.bottom() - 3.0);
        QPainterPath path;
        path.moveTo(a1);
        path.cubicTo(h1, h2, a2);
        p.drawPath(path);
        QPen thin = p.pen();
        thin.setWidthF(1.15);
        p.setPen(thin);
        p.drawLine(a1, h1);
        p.drawLine(a2, h2);
        const QColor c = thin.color();
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(a1, 1.7, 1.7);
        p.drawEllipse(a2, 1.7, 1.7);
        p.setBrush(Qt::NoBrush);
        p.setPen(thin);
        p.drawEllipse(h1, 1.5, 1.5);
        p.drawEllipse(h2, 1.5, 1.5);
    });
}

QIcon ToolIconProvider::createSplineIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.save();
        p.translate(r.center());
        p.rotate(-40);
        const qreal h = r.height();
        p.drawRoundedRect(QRectF(-1.7, -h * 0.42, 3.4, h * 0.55), 0.8, 0.8);
        QPolygonF tip;
        tip << QPointF(-1.7, h * 0.13) << QPointF(1.7, h * 0.13) << QPointF(0.0, h * 0.42);
        p.drawPolygon(tip);
        const QColor c = p.pen().color();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(0.0, h * 0.42), 1.1, 1.1);
        p.restore();
    });
}

QIcon ToolIconProvider::createPolygonIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const qreal cx = r.center().x(), cy = r.center().y();
        const qreal rad = qMin(r.width(), r.height()) * 0.48;
        QPolygonF poly;
        for (int i = 0; i < 6; ++i) {
            const qreal a = -M_PI / 2.0 + i * M_PI / 3.0;
            poly << QPointF(cx + rad * qCos(a), cy + rad * qSin(a));
        }
        p.drawPolygon(poly);
    });
}

QIcon ToolIconProvider::createRectangleIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.drawRoundedRect(r.adjusted(0.8, 2.0, -0.8, -2.0), 1.5, 1.5);
    });
}

QIcon ToolIconProvider::createEllipseIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.drawEllipse(r.adjusted(0.2, 2.4, -0.2, -2.4));
    });
}

QIcon ToolIconProvider::createCircleIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        // Perfect circle — no crosshair (reads cleaner vs ellipse).
        p.drawEllipse(r.adjusted(1.0, 1.0, -1.0, -1.0));
    });
}

QIcon ToolIconProvider::createArcIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QRectF ar = r.adjusted(1.2, 1.2, -1.2, -1.2);
        p.drawArc(ar, -40 * 16, 250 * 16);
        const qreal cx = ar.center().x(), cy = ar.center().y();
        const qreal rad = ar.width() * 0.5;
        auto tick = [&](qreal deg) {
            const qreal a = deg * M_PI / 180.0;
            const QPointF dir(qCos(a), -qSin(a));
            const QPointF pt(cx + rad * dir.x(), cy + rad * dir.y());
            p.drawLine(pt - dir * 2.0, pt + dir * 1.2);
        };
        tick(-40.0);
        tick(210.0);
    });
}

QIcon ToolIconProvider::createEraserIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.save();
        p.translate(r.center());
        p.rotate(-28);
        const QRectF body(-r.width() * 0.42, -r.height() * 0.24,
                          r.width() * 0.84, r.height() * 0.48);
        p.drawRoundedRect(body, 1.8, 1.8);
        const qreal divX = body.left() + body.width() * 0.38;
        p.drawLine(QPointF(divX, body.top() + 0.4), QPointF(divX, body.bottom() - 0.4));
        QPainterPath tip;
        tip.addRoundedRect(QRectF(body.left(), body.top(), body.width() * 0.38, body.height()), 1.8, 1.8);
        QColor c = p.pen().color();
        c.setAlpha(55);
        p.fillPath(tip, c);
        p.restore();
    });
}

QIcon ToolIconProvider::createFillIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.save();
        p.translate(r.center().x() - 0.5, r.center().y() - 0.8);
        p.rotate(-28);
        QPainterPath bucket;
        bucket.moveTo(-4.6, -4.2);
        bucket.lineTo(4.6, -4.2);
        bucket.lineTo(3.2, 5.0);
        bucket.lineTo(-3.2, 5.0);
        bucket.closeSubpath();
        p.drawPath(bucket);
        p.drawArc(QRectF(-2.8, -7.6, 5.6, 5.0), 20 * 16, 140 * 16);
        p.restore();
        QPainterPath drop;
        const qreal dx = r.right() - 2.8, dy = r.bottom() - 4.2;
        drop.moveTo(dx, dy);
        drop.cubicTo(dx + 2.2, dy + 1.8, dx + 1.6, dy + 4.4, dx - 0.2, dy + 4.4);
        drop.cubicTo(dx - 1.8, dy + 4.4, dx - 2.0, dy + 2.0, dx, dy);
        const QColor c = p.pen().color();
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawPath(drop);
    });
}

QIcon ToolIconProvider::createBrushIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        // Photoshop-style brush tip: outer soft ring + solid core.
        const QPointF c = r.center();
        const qreal outer = qMin(r.width(), r.height()) * 0.42;
        p.drawEllipse(c, outer, outer);
        QPen thin = p.pen();
        thin.setWidthF(1.15);
        thin.setStyle(Qt::DashLine);
        p.setPen(thin);
        p.drawEllipse(c, outer * 0.62, outer * 0.62);
        // Solid core
        const QColor col = thin.color();
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(c, outer * 0.28, outer * 0.28);
    });
}

QIcon ToolIconProvider::createBlurIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const qreal cx = r.center().x();
        const qreal top = r.top() + 0.8;
        const qreal bot = r.bottom() - 0.8;
        const qreal w = r.width() * 0.32;
        QPainterPath drop;
        drop.moveTo(cx, top);
        drop.cubicTo(cx + w * 0.15, top + 3.0, cx + w, bot - w * 1.1, cx + w, bot - w);
        drop.cubicTo(cx + w, bot + 0.2, cx - w, bot + 0.2, cx - w, bot - w);
        drop.cubicTo(cx - w, bot - w * 1.1, cx - w * 0.15, top + 3.0, cx, top);
        p.drawPath(drop);
        QPen thin = p.pen();
        thin.setWidthF(1.1);
        p.setPen(thin);
        p.drawLine(QPointF(cx - w * 0.35, top + 5.0), QPointF(cx - w * 0.15, top + 8.5));
    });
}

QIcon ToolIconProvider::createMeasureIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QPointF a(r.left() + 1.2, r.bottom() - 2.5);
        const QPointF b(r.right() - 1.2, r.top() + 2.5);
        p.drawLine(a, b);
        QLineF line(a, b);
        const QPointF dir = line.unitVector().p2() - line.unitVector().p1();
        const QPointF n(-dir.y(), dir.x());
        auto endTick = [&](const QPointF &pt) { p.drawLine(pt - n * 2.6, pt + n * 2.6); };
        endTick(a); endTick(b);
        for (int i = 1; i <= 3; ++i) {
            const QPointF pt = a + (b - a) * (i / 4.0);
            p.drawLine(pt - n * 1.4, pt + n * 1.4);
        }
    });
}

QIcon ToolIconProvider::createImageIcon()
{
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QRectF fr = r.adjusted(0.8, 1.6, -0.8, -1.6);
        p.drawRoundedRect(fr, 1.6, 1.6);
        p.drawEllipse(QPointF(fr.right() - 3.4, fr.top() + 3.2), 1.7, 1.7);
        QPainterPath mt;
        mt.moveTo(fr.left() + 0.8, fr.bottom() - 0.8);
        mt.lineTo(fr.left() + fr.width() * 0.32, fr.top() + fr.height() * 0.48);
        mt.lineTo(fr.left() + fr.width() * 0.50, fr.top() + fr.height() * 0.68);
        mt.lineTo(fr.left() + fr.width() * 0.72, fr.top() + fr.height() * 0.38);
        mt.lineTo(fr.right() - 0.8, fr.bottom() - 0.8);
        p.drawPath(mt);
    });
}

QIcon ToolIconProvider::createHandIcon()
{
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QColor c = p.pen().color();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal cx = r.center().x();
        const qreal T = r.top();
        p.drawRoundedRect(QRectF(cx - 4.8, T + 7.4, 9.6, 7.4), 3.2, 3.2);
        const qreal fw = 2.15;
        const qreal xs[4] = { cx - 4.2, cx - 1.5, cx + 1.2, cx + 3.9 };
        const qreal tops[4] = { T + 3.4, T + 1.8, T + 2.4, T + 4.2 };
        for (int i = 0; i < 4; ++i) {
            p.drawRoundedRect(QRectF(xs[i] - fw * 0.5, tops[i], fw, T + 11.0 - tops[i]), 1.05, 1.05);
        }
        p.save();
        p.translate(cx - 4.6, T + 9.0);
        p.rotate(-48);
        p.drawRoundedRect(QRectF(-1.1, -1.0, 2.2, 6.2), 1.05, 1.05);
        p.restore();
    });
}

QIcon ToolIconProvider::createTextIcon()
{
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QColor c = p.pen().color();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal cx = r.center().x();
        const qreal top = r.top() + 1.4;
        const qreal bot = r.bottom() - 1.2;
        const qreal barH = 2.35;
        const qreal stemW = 2.5;
        const qreal barL = r.left() + 1.6;
        const qreal barR = r.right() - 1.6;
        p.drawRect(QRectF(barL, top, barR - barL, barH));
        p.drawRect(QRectF(barL, top, 1.7, barH + 1.6));
        p.drawRect(QRectF(barR - 1.7, top, 1.7, barH + 1.6));
        p.drawRect(QRectF(cx - stemW * 0.5, top, stemW, bot - top - 1.5));
        p.drawRect(QRectF(cx - 3.8, bot - 1.7, 7.6, 1.7));
    });
}

QIcon ToolIconProvider::iconForTool(DrawingTool tool)
{
    switch (tool) {
    case DrawingTool::Select: return createSelectIcon();
    case DrawingTool::Move: return createHandIcon();
    case DrawingTool::Line: return createLineIcon();
    case DrawingTool::AngleLine: return createAngleLineIcon();
    case DrawingTool::Curve: return createCurveIcon();
    case DrawingTool::BezierCurve: return createBezierIcon();
    case DrawingTool::Spline: return createSplineIcon();
    case DrawingTool::Polygon: return createPolygonIcon();
    case DrawingTool::Rectangle: return createRectangleIcon();
    case DrawingTool::Ellipse: return createEllipseIcon();
    case DrawingTool::Circle: return createCircleIcon();
    case DrawingTool::Arc: return createArcIcon();
    case DrawingTool::Eraser: return createEraserIcon();
    case DrawingTool::Fill: return createFillIcon();
    case DrawingTool::Brush: return createBrushIcon();
    case DrawingTool::Blur: return createBlurIcon();
    case DrawingTool::Measure: return createMeasureIcon();
    case DrawingTool::Image: return createImageIcon();
    case DrawingTool::Text: return createTextIcon();
    }
    return createSelectIcon();
}

QString ToolIconProvider::displayNameForTool(DrawingTool tool)
{
    switch (tool) {
    case DrawingTool::Select: return QStringLiteral("Select");
    case DrawingTool::Move: return QStringLiteral("Hand");
    case DrawingTool::Line: return QStringLiteral("Line");
    case DrawingTool::AngleLine: return QStringLiteral("Angle Line");
    case DrawingTool::Curve: return QStringLiteral("Curve");
    case DrawingTool::BezierCurve: return QStringLiteral("Bezier");
    case DrawingTool::Spline: return QStringLiteral("Spline");
    case DrawingTool::Polygon: return QStringLiteral("Polygon");
    case DrawingTool::Rectangle: return QStringLiteral("Rectangle");
    case DrawingTool::Ellipse: return QStringLiteral("Ellipse");
    case DrawingTool::Circle: return QStringLiteral("Circle");
    case DrawingTool::Arc: return QStringLiteral("Arc");
    case DrawingTool::Eraser: return QStringLiteral("Eraser");
    case DrawingTool::Fill: return QStringLiteral("Fill");
    case DrawingTool::Brush: return QStringLiteral("Brush");
    case DrawingTool::Blur: return QStringLiteral("Blur");
    case DrawingTool::Measure: return QStringLiteral("Measure");
    case DrawingTool::Image: return QStringLiteral("Image");
    case DrawingTool::Text: return QStringLiteral("Text");
    }
    return QStringLiteral("Tool");
}

QKeySequence ToolIconProvider::defaultShortcutForTool(DrawingTool tool)
{
    switch (tool) {
    case DrawingTool::Select: return QKeySequence(QStringLiteral("V"));
    case DrawingTool::Move: return QKeySequence(QStringLiteral("H"));
    case DrawingTool::Line: return QKeySequence(QStringLiteral("L"));
    case DrawingTool::AngleLine: return QKeySequence(QStringLiteral("Shift+L"));
    case DrawingTool::Curve: return QKeySequence(QStringLiteral("C"));
    case DrawingTool::BezierCurve: return QKeySequence(QStringLiteral("B"));
    case DrawingTool::Spline: return QKeySequence(QStringLiteral("P"));
    case DrawingTool::Polygon: return QKeySequence(QStringLiteral("G"));
    case DrawingTool::Rectangle: return QKeySequence(QStringLiteral("R"));
    case DrawingTool::Ellipse: return QKeySequence(QStringLiteral("E"));
    case DrawingTool::Circle: return QKeySequence(QStringLiteral("O"));
    case DrawingTool::Arc: return QKeySequence(QStringLiteral("A"));
    case DrawingTool::Eraser: return QKeySequence(QStringLiteral("X"));
    case DrawingTool::Fill: return QKeySequence(QStringLiteral("F"));
    case DrawingTool::Brush: return QKeySequence(QStringLiteral("D"));
    case DrawingTool::Blur: return QKeySequence(QStringLiteral("U"));
    case DrawingTool::Measure: return QKeySequence(QStringLiteral("M"));
    case DrawingTool::Image: return QKeySequence(QStringLiteral("I"));
    case DrawingTool::Text: return QKeySequence(QStringLiteral("T"));
    }
    return {};
}

QIcon ToolIconProvider::loadCustomIcon(const QString &iconName,
                                 std::function<QIcon()> fallbackGenerator) {
  // Directly use the provided generator.
  if (fallbackGenerator) {
      return fallbackGenerator();
  }
  return QIcon();
}

QIcon ToolIconProvider::loadIconFromFile(const QString& filename)
{
    QString iconPath = QString(":/icons/%1").arg(filename);
    QIcon icon(iconPath);
    
    // If resource loading fails, try loading from file system
    if (icon.isNull()) {
        // Try relative path from build directory
        QString filePath = QString("../resources/icons/%1").arg(filename);
        if (QFile::exists(filePath)) {
            icon = QIcon(filePath);
        } else {
            // Try absolute path
            QString absolutePath = QString("/Users/Lukovic/Apps/DrawingStudio/resources/icons/%1").arg(filename);
            if (QFile::exists(absolutePath)) {
                icon = QIcon(absolutePath);
            }
        }
    }
    
    return icon;
}

QIcon ToolIconProvider::loadSVGIcon(const QString& iconName, const QSize& size)
{
    QString pngPath = QString(":/ai_icons/%1.png").arg(iconName);
    
    // Try loading from resources first
    if (QFile::exists(pngPath)) {
        QPixmap pixmap(pngPath);
        if (!pixmap.isNull()) {
            // Use normal icon size
             QSize newSize = QSize(24, 24); // Normal icon size
            QPixmap scaledPixmap = pixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            
            // Remove background by making it fully transparent
            QImage image = scaledPixmap.toImage();
            image = image.convertToFormat(QImage::Format_ARGB32);
            
            // Make white pixels transparent
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    QRgb pixel = image.pixel(x, y);
                    QColor color(pixel);
                    
                    // If pixel is white or very light, make it transparent
                    if (color.red() > 240 && color.green() > 240 && color.blue() > 240) {
                        image.setPixel(x, y, qRgba(0, 0, 0, 0));
                    }
                }
            }
            
            scaledPixmap = QPixmap::fromImage(image);
            
            // Create icon with transparency
            QIcon icon(scaledPixmap);
            return icon;
        }
    }
    
    // Try loading from file system (relative to build directory)
    QString filePath = QString("../resources/ai_icons/%1.png").arg(iconName);
    if (QFile::exists(filePath)) {
        QPixmap pixmap(filePath);
        if (!pixmap.isNull()) {
            // Use normal icon size
             QSize newSize = QSize(24, 24); // Normal icon size
            QPixmap scaledPixmap = pixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            
            // Remove background by making it fully transparent
            QImage image = scaledPixmap.toImage();
            image = image.convertToFormat(QImage::Format_ARGB32);
            
            // Make white pixels transparent
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    QRgb pixel = image.pixel(x, y);
                    QColor color(pixel);
                    
                    // If pixel is white or very light, make it transparent
                    if (color.red() > 240 && color.green() > 240 && color.blue() > 240) {
                        image.setPixel(x, y, qRgba(0, 0, 0, 0));
                    }
                }
            }
            
            scaledPixmap = QPixmap::fromImage(image);
            
            // Create icon with transparency
            QIcon icon(scaledPixmap);
            return icon;
        }
    }
    
    // Try absolute path
    QString absolutePath = QString("/Users/Lukovic/Apps/DrawingStudio/resources/ai_icons/%1.png").arg(iconName);
    if (QFile::exists(absolutePath)) {
        QPixmap pixmap(absolutePath);
        if (!pixmap.isNull()) {
            // Use normal icon size
             QSize newSize = QSize(24, 24); // Normal icon size
            QPixmap scaledPixmap = pixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            
            // Remove background by making it fully transparent
            QImage image = scaledPixmap.toImage();
            image = image.convertToFormat(QImage::Format_ARGB32);
            
            // Make white pixels transparent
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    QRgb pixel = image.pixel(x, y);
                    QColor color(pixel);
                    
                    // If pixel is white or very light, make it transparent
                    if (color.red() > 240 && color.green() > 240 && color.blue() > 240) {
                        image.setPixel(x, y, qRgba(0, 0, 0, 0));
                    }
                }
            }
            
            scaledPixmap = QPixmap::fromImage(image);
            
            // Create icon with transparency
            QIcon icon(scaledPixmap);
            
            
            return icon;
        }
    }
    
    // Fallback to programmatic icon
    qDebug() << "PNG icon not found:" << iconName << ", using programmatic fallback";
    return QIcon();
}

QIcon ToolIconProvider::loadAIIcon(const QString& toolName, const QSize& size)
{
    // Use modern programmatic icons directly
    if (toolName == "select") return createSelectIcon();
    if (toolName == "line") return createLineIcon();
    if (toolName == "angleline") return createAngleLineIcon();
    if (toolName == "curve") return createCurveIcon();
    if (toolName == "bezier") return createBezierIcon();
    if (toolName == "spline") return createSplineIcon();
    if (toolName == "polygon") return createPolygonIcon();
    if (toolName == "rectangle") return createRectangleIcon();
    if (toolName == "ellipse") return createEllipseIcon();
    if (toolName == "eraser") return createEraserIcon();
    if (toolName == "fill") return createFillIcon();
    if (toolName == "brush") return createBrushIcon();
    if (toolName == "blur") return createBlurIcon();
    if (toolName == "hand") return createHandIcon();
    if (toolName == "measure") return createMeasureIcon();
    if (toolName == "image") return createImageIcon();
    if (toolName == "text") return createTextIcon();
    
    // Default fallback
    return QIcon();
}

