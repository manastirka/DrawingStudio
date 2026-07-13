#include "MathGraphRenderer.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <cmath>

MathGraphRenderer::Result MathGraphRenderer::render(const Options &opt)
{
    Result out;
    MathExpression expr(opt.expression);
    if (!opt.namedValues.isEmpty())
        expr.setNamedValues(opt.namedValues);
    if (!expr.isValid()) {
        out.error = expr.error().isEmpty() ? QStringLiteral("Invalid expression.")
                                           : expr.error();
        return out;
    }
    if (!(opt.xMax > opt.xMin) || opt.width < 40 || opt.height < 40 ||
        opt.samples < 2) {
        out.error = QStringLiteral("Invalid graph domain or size.");
        return out;
    }

    QVector<QPointF> pts = expr.sample(opt.xMin, opt.xMax, opt.samples);
    out.pointCount = pts.size();
    if (pts.isEmpty()) {
        out.error = QStringLiteral(
            "No plottable points — check domain (e.g. ln(x) needs x > 0).");
        return out;
    }

    double yMin = opt.yMin;
    double yMax = opt.yMax;
    if (opt.autoY) {
        yMin = pts.first().y();
        yMax = yMin;
        for (const QPointF &p : pts) {
            yMin = std::min<qreal>(yMin, p.y());
            yMax = std::max<qreal>(yMax, p.y());
        }
        if (!(yMax > yMin)) {
            yMin -= 1.0;
            yMax += 1.0;
        } else {
            const double pad = (yMax - yMin) * 0.08;
            yMin -= pad;
            yMax += pad;
        }
    }

    QImage img(opt.width, opt.height, QImage::Format_ARGB32_Premultiplied);
    img.fill(opt.background);

    const int m = opt.margin;
    const QRect plot(m, m, opt.width - 2 * m, opt.height - 2 * m);
    if (plot.width() < 10 || plot.height() < 10) {
        out.error = QStringLiteral("Plot area too small.");
        return out;
    }

    auto mapX = [&](double x) -> double {
        return plot.left() +
               (x - opt.xMin) / (opt.xMax - opt.xMin) * plot.width();
    };
    auto mapY = [&](double y) -> double {
        return plot.bottom() -
               (y - yMin) / (yMax - yMin) * plot.height();
    };

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setFont(opt.font);

    // Grid
    if (opt.showGrid) {
        p.setPen(QPen(opt.gridColor, 1.0));
        const int gx = 10;
        const int gy = 8;
        for (int i = 0; i <= gx; ++i) {
            const double x = opt.xMin + (opt.xMax - opt.xMin) * i / gx;
            const double px = mapX(x);
            p.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
        }
        for (int i = 0; i <= gy; ++i) {
            const double y = yMin + (yMax - yMin) * i / gy;
            const double py = mapY(y);
            p.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
        }
    }

    // Axes
    if (opt.showAxes) {
        p.setPen(QPen(opt.axisColor, 1.6));
        const bool xAxisIn = (0 >= yMin && 0 <= yMax);
        const bool yAxisIn = (0 >= opt.xMin && 0 <= opt.xMax);
        if (xAxisIn) {
            const double y0 = mapY(0);
            p.drawLine(QPointF(plot.left(), y0), QPointF(plot.right(), y0));
        } else {
            p.drawLine(QPointF(plot.left(), plot.bottom()),
                       QPointF(plot.right(), plot.bottom()));
        }
        if (yAxisIn) {
            const double x0 = mapX(0);
            p.drawLine(QPointF(x0, plot.top()), QPointF(x0, plot.bottom()));
        } else {
            p.drawLine(QPointF(plot.left(), plot.top()),
                       QPointF(plot.left(), plot.bottom()));
        }

        // Arrow heads
        auto arrow = [&](QPointF tip, QPointF dir) {
            const QPointF n(-dir.y(), dir.x());
            const QPointF a = tip - dir * 10 + n * 4;
            const QPointF b = tip - dir * 10 - n * 4;
            p.drawLine(tip, a);
            p.drawLine(tip, b);
        };
        if (xAxisIn)
            arrow(QPointF(plot.right(), mapY(0)), QPointF(1, 0));
        if (yAxisIn)
            arrow(QPointF(mapX(0), plot.top()), QPointF(0, -1));
    }

    // Ticks + labels
    if (opt.showTicks || opt.showLabels) {
        p.setPen(opt.textColor);
        QFontMetricsF fm(opt.font);
        const int xt = 6;
        for (int i = 0; i <= xt; ++i) {
            const double x = opt.xMin + (opt.xMax - opt.xMin) * i / xt;
            const double px = mapX(x);
            if (opt.showTicks) {
                p.setPen(QPen(opt.axisColor, 1.0));
                const double yBase =
                    (0 >= yMin && 0 <= yMax) ? mapY(0) : plot.bottom();
                p.drawLine(QPointF(px, yBase - 4), QPointF(px, yBase + 4));
            }
            if (opt.showLabels) {
                p.setPen(opt.textColor);
                const QString lab = QString::number(x, 'g', 3);
                p.drawText(QPointF(px - fm.horizontalAdvance(lab) * 0.5,
                                   plot.bottom() + fm.height()),
                           lab);
            }
        }
        const int yt = 5;
        for (int i = 0; i <= yt; ++i) {
            const double y = yMin + (yMax - yMin) * i / yt;
            const double py = mapY(y);
            if (opt.showTicks) {
                p.setPen(QPen(opt.axisColor, 1.0));
                const double xBase =
                    (0 >= opt.xMin && 0 <= opt.xMax) ? mapX(0) : plot.left();
                p.drawLine(QPointF(xBase - 4, py), QPointF(xBase + 4, py));
            }
            if (opt.showLabels) {
                p.setPen(opt.textColor);
                const QString lab = QString::number(y, 'g', 3);
                p.drawText(QPointF(plot.left() - fm.horizontalAdvance(lab) - 6,
                                   py + fm.ascent() * 0.35),
                           lab);
            }
        }
        if (opt.showLabels) {
            p.drawText(QPointF(plot.right() - 10, (0 >= yMin && 0 <= yMax)
                                                       ? mapY(0) - 6
                                                       : plot.bottom() - 6),
                       opt.xLabel.isEmpty() ? QStringLiteral("x") : opt.xLabel);
            p.drawText(QPointF((0 >= opt.xMin && 0 <= opt.xMax) ? mapX(0) + 6
                                                                : plot.left() + 6,
                               plot.top() + fm.ascent()),
                       opt.yLabel.isEmpty() ? QStringLiteral("y") : opt.yLabel);
        }
    }

    if (opt.showTitle) {
        p.setPen(opt.textColor);
        QFont title = opt.font;
        title.setBold(true);
        title.setPointSizeF(opt.font.pointSizeF() + 1);
        p.setFont(title);
        const QString t =
            opt.title.isEmpty()
                ? QStringLiteral("y = %1").arg(opt.expression.trimmed())
                : opt.title;
        QFontMetricsF fm(title);
        p.drawText(QPointF((opt.width - fm.horizontalAdvance(t)) * 0.5, m - 10),
                   t);
        p.setFont(opt.font);
    }

    // Curve — break on large jumps (asymptotes)
    p.setPen(QPen(opt.curveColor, opt.curveWidth, Qt::SolidLine, Qt::RoundCap,
                  Qt::RoundJoin));
    QPainterPath path;
    bool have = false;
    QPointF prev;
    for (const QPointF &pt : pts) {
        if (!std::isfinite(pt.y())) {
            have = false;
            continue;
        }
        const QPointF mapped(mapX(pt.x()), mapY(pt.y()));
        if (!plot.contains(mapped.toPoint()) &&
            (mapped.y() < plot.top() - 50 || mapped.y() > plot.bottom() + 50)) {
            have = false;
            continue;
        }
        if (!have) {
            path.moveTo(mapped);
            have = true;
        } else {
            const double dy = std::fabs(mapped.y() - prev.y());
            if (dy > plot.height() * 0.45)
                path.moveTo(mapped);
            else
                path.lineTo(mapped);
        }
        prev = mapped;
    }
    p.drawPath(path);

    if (opt.showOrigin && 0 >= opt.xMin && 0 <= opt.xMax && 0 >= yMin &&
        0 <= yMax) {
        p.setBrush(opt.axisColor);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(mapX(0), mapY(0)), 3.0, 3.0);
    }

    p.end();
    out.image = img;
    return out;
}
