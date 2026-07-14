#include "DrawingPrimitive.h"

#include <QColor>
#include <QtTest>

#include <memory>

/**
 * toJson → createFromJson round-trips for core primitive types.
 */
class tst_PrimitiveRoundTrip : public QObject {
    Q_OBJECT

private slots:
    void line_roundTrip();
    void rectangle_roundTrip();
    void circle_roundTrip();
    void ellipse_roundTrip();
    void text_roundTrip();
    void polygon_roundTrip();
    void curve_roundTrip();
    void bezier_roundTrip();
    void spline_roundTrip();
    void arc_roundTrip();
    void dimension_roundTrip();
    void colorAndWidth_preserved();
};

static std::unique_ptr<DrawingPrimitive> roundTrip(const DrawingPrimitive &src)
{
    return DrawingPrimitive::createFromJson(src.toJson());
}

void tst_PrimitiveRoundTrip::line_roundTrip()
{
    LinePrimitive src(QVector2D(1.5f, 2.5f), QVector2D(10.f, 20.f));
    src.setColor(QColor(QStringLiteral("#112233")));
    src.setLineWidth(3.5f);

    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Line);
    auto *line = dynamic_cast<LinePrimitive *>(out.get());
    QVERIFY(line);
    QCOMPARE(line->startPoint(), src.startPoint());
    QCOMPARE(line->endPoint(), src.endPoint());
    QCOMPARE(line->color(), src.color());
    QCOMPARE(line->lineWidth(), src.lineWidth());
}

void tst_PrimitiveRoundTrip::rectangle_roundTrip()
{
    RectanglePrimitive src(QVector2D(0, 0), QVector2D(40, 30));
    src.setFilled(true);
    src.setFillColor(QColor(Qt::cyan));

    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Rectangle);
    auto *rect = dynamic_cast<RectanglePrimitive *>(out.get());
    QVERIFY(rect);
    QCOMPARE(rect->topLeft(), src.topLeft());
    QCOMPARE(rect->bottomRight(), src.bottomRight());
    QCOMPARE(rect->filled(), true);
}

void tst_PrimitiveRoundTrip::circle_roundTrip()
{
    CirclePrimitive src(QVector2D(5, 5), 12.f);
    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Circle);
    auto *c = dynamic_cast<CirclePrimitive *>(out.get());
    QVERIFY(c);
    QCOMPARE(c->center(), src.center());
    QCOMPARE(c->radius(), src.radius());
}

void tst_PrimitiveRoundTrip::ellipse_roundTrip()
{
    EllipsePrimitive src(QVector2D(3, 4), 8.f, 6.f);
    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Ellipse);
    auto *e = dynamic_cast<EllipsePrimitive *>(out.get());
    QVERIFY(e);
    QCOMPARE(e->center(), src.center());
    QCOMPARE(e->radiusX(), src.radiusX());
    QCOMPARE(e->radiusY(), src.radiusY());
}

void tst_PrimitiveRoundTrip::text_roundTrip()
{
    TextPrimitive src(QVector2D(9, 10), QStringLiteral("Hello"));
    src.setFontSize(18.f);
    src.setBold(true);

    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Text);
    auto *t = dynamic_cast<TextPrimitive *>(out.get());
    QVERIFY(t);
    QCOMPARE(t->text(), QStringLiteral("Hello"));
    QCOMPARE(t->fontSize(), 18.f);
    QCOMPARE(t->isBold(), true);
}

void tst_PrimitiveRoundTrip::polygon_roundTrip()
{
    PolygonPrimitive src;
    src.addPoint(QVector2D(0, 0));
    src.addPoint(QVector2D(10, 0));
    src.addPoint(QVector2D(5, 8));
    src.setClosed(true);

    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Polygon);
    auto *p = dynamic_cast<PolygonPrimitive *>(out.get());
    QVERIFY(p);
    QCOMPARE(static_cast<int>(p->points().size()), 3);
    QCOMPARE(p->isClosed(), true);
}

void tst_PrimitiveRoundTrip::curve_roundTrip()
{
    CurvePrimitive src;
    src.addControlPoint(QVector2D(0, 0));
    src.addControlPoint(QVector2D(20, 40));
    src.addControlPoint(QVector2D(40, 0));
    src.setClosed(false);
    src.setCurveType(2);

    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Curve);
    auto *c = dynamic_cast<CurvePrimitive *>(out.get());
    QVERIFY(c);
    QCOMPARE(static_cast<int>(c->controlPoints().size()), 3);
    QCOMPARE(c->controlPoints()[1], QVector2D(20, 40));
    QCOMPARE(c->curveType(), 2);
}

void tst_PrimitiveRoundTrip::bezier_roundTrip()
{
    BezierCurvePrimitive src;
    src.setControlPoints({
        QVector2D(0, 0),
        QVector2D(10, 30),
        QVector2D(30, 30),
        QVector2D(40, 0),
    });
    src.setSubdivisionLevel(40);

    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::BezierCurve);
    auto *b = dynamic_cast<BezierCurvePrimitive *>(out.get());
    QVERIFY(b);
    QCOMPARE(static_cast<int>(b->controlPoints().size()), 4);
    QCOMPARE(b->controlPoints()[0], QVector2D(0, 0));
    QCOMPARE(b->controlPoints()[3], QVector2D(40, 0));
    QCOMPARE(b->subdivisionLevel(), 40);
}

void tst_PrimitiveRoundTrip::spline_roundTrip()
{
    SplinePrimitive src;
    src.addPoint(QVector2D(1, 1));
    src.addPoint(QVector2D(5, 9));
    src.addPoint(QVector2D(12, 2));
    src.setClosed(true);
    src.setSmoothness(0.75f);

    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Spline);
    auto *s = dynamic_cast<SplinePrimitive *>(out.get());
    QVERIFY(s);
    QCOMPARE(static_cast<int>(s->points().size()), 3);
    QCOMPARE(s->isClosed(), true);
    QCOMPARE(s->smoothness(), 0.75f);
}

void tst_PrimitiveRoundTrip::arc_roundTrip()
{
    ArcPrimitive src(QVector2D(10, 10), 25.f, 15.f, 120.f);
    src.setColor(QColor(Qt::magenta));

    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Arc);
    auto *a = dynamic_cast<ArcPrimitive *>(out.get());
    QVERIFY(a);
    QCOMPARE(a->center(), QVector2D(10, 10));
    QCOMPARE(a->radius(), 25.f);
    QCOMPARE(a->startAngle(), 15.f);
    QCOMPARE(a->endAngle(), 120.f);
}

void tst_PrimitiveRoundTrip::dimension_roundTrip()
{
    DimensionPrimitive src(QVector2D(0, 0), QVector2D(100, 0));
    src.setUnitsString(QStringLiteral("mm"));
    src.setPixelsPerUnit(2.f);
    src.recalculateMeasurement();

    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->type(), PrimitiveType::Dimension);
    auto *d = dynamic_cast<DimensionPrimitive *>(out.get());
    QVERIFY(d);
    QCOMPARE(d->startPoint(), QVector2D(0, 0));
    QCOMPARE(d->endPoint(), QVector2D(100, 0));
    QCOMPARE(d->getUnitsString(), QStringLiteral("mm"));
    QCOMPARE(d->pixelsPerUnit(), 2.f);
}

void tst_PrimitiveRoundTrip::colorAndWidth_preserved()
{
    LinePrimitive src(QVector2D(0, 0), QVector2D(1, 1));
    src.setColor(QColor(200, 10, 20));
    src.setLineWidth(7.f);
    auto out = roundTrip(src);
    QVERIFY(out);
    QCOMPARE(out->color(), QColor(200, 10, 20));
    QCOMPARE(out->lineWidth(), 7.f);
}

QTEST_MAIN(tst_PrimitiveRoundTrip)
#include "tst_PrimitiveRoundTrip.moc"
