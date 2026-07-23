#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"

#include <QColor>
#include <QtTest>

#include <limits>
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
    void invalidVectorGeometryRejected();
    void oversizedVectorGeometryRejected();
    void curveParametersAreBounded();
    void commonStyleParametersAreBounded();
    void malformedCommonStyleRejected();
    void malformedScalarGeometryRejected();
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

void tst_PrimitiveRoundTrip::invalidVectorGeometryRejected()
{
    PolygonPrimitive polygon;
    polygon.addPoint(QVector2D(0, 0));
    polygon.addPoint(QVector2D(10, 0));
    polygon.addPoint(QVector2D(5, 10));
    QJsonObject json = polygon.toJson();

    QJsonArray points = json["points"].toArray();
    QJsonObject malformed = points.at(1).toObject();
    malformed["x"] = QStringLiteral("not-a-number");
    points[1] = malformed;
    json["points"] = points;
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    json = polygon.toJson();
    points = json["points"].toArray();
    QJsonObject extreme = points.at(1).toObject();
    extreme["x"] = 1.0e12;
    points[1] = extreme;
    json["points"] = points;
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    json = polygon.toJson();
    json["type"] = 8.5;
    QVERIFY(!DrawingPrimitive::createFromJson(json));
}

void tst_PrimitiveRoundTrip::oversizedVectorGeometryRejected()
{
    QJsonObject point;
    point["x"] = 0.0;
    point["y"] = 0.0;
    QJsonArray points;
    for (qsizetype i = 0;
         i <= DrawingPrimitive::kMaxSerializedPointsPerPrimitive; ++i) {
        points.append(point);
    }

    QJsonObject json;
    json["type"] = static_cast<int>(PrimitiveType::Polygon);
    json["points"] = points;
    QVERIFY(!DrawingPrimitive::createFromJson(json));
}

void tst_PrimitiveRoundTrip::curveParametersAreBounded()
{
    CurvePrimitive curve;
    curve.setCurveType(-100);
    QCOMPARE(curve.curveType(), 0);
    curve.setCurveType(100);
    QCOMPARE(curve.curveType(), 2);

    BezierCurvePrimitive bezier;
    bezier.setSubdivisionLevel(-100);
    QCOMPARE(bezier.subdivisionLevel(), 10);
    QJsonObject bezierJson = bezier.toJson();
    bezierJson["subdivisionLevel"] = 10000;
    auto bezierOut = DrawingPrimitive::createFromJson(bezierJson);
    auto *boundedBezier = dynamic_cast<BezierCurvePrimitive *>(bezierOut.get());
    QVERIFY(boundedBezier);
    QCOMPARE(boundedBezier->subdivisionLevel(), 200);

    SplinePrimitive spline;
    spline.setSmoothness(-5.0f);
    spline.setTension(5.0f);
    spline.setInterpolationType(50);
    QCOMPARE(spline.smoothness(), 0.0f);
    QCOMPARE(spline.tension(), 1.0f);
    QCOMPARE(spline.interpolationType(), 2);

    QJsonObject splineJson = spline.toJson();
    splineJson["smoothness"] = 100.0;
    splineJson["tension"] = -100.0;
    splineJson["interpolationType"] = -10;
    auto splineOut = DrawingPrimitive::createFromJson(splineJson);
    auto *boundedSpline = dynamic_cast<SplinePrimitive *>(splineOut.get());
    QVERIFY(boundedSpline);
    QCOMPARE(boundedSpline->smoothness(), 1.0f);
    QCOMPARE(boundedSpline->tension(), 0.0f);
    QCOMPARE(boundedSpline->interpolationType(), 0);
}

void tst_PrimitiveRoundTrip::commonStyleParametersAreBounded()
{
    LinePrimitive line(QVector2D(), QVector2D(10, 10));
    line.setLineWidth(-5.0f);
    line.setOpacityMultiplier(2.0f);
    line.setRotationDegrees(1000.0f);
    line.setShadowOffset(-20000.0f, 20000.0f);
    line.setShadowBlur(5000.0f);
    QCOMPARE(line.lineWidth(), 0.0f);
    QCOMPARE(line.opacityMultiplier(), 1.0f);
    QCOMPARE(line.rotationDegrees(), 360.0f);
    QCOMPARE(line.shadowOffsetX(), -DrawingPrimitive::kMaxShadowOffset);
    QCOMPARE(line.shadowOffsetY(), DrawingPrimitive::kMaxShadowOffset);
    QCOMPARE(line.shadowBlur(), DrawingPrimitive::kMaxShadowBlur);

    line.setLineWidth(7.0f);
    line.setLineWidth(std::numeric_limits<float>::quiet_NaN());
    QCOMPARE(line.lineWidth(), 7.0f);

    line.setOpacityMultiplier(0.4f);
    auto cloned = line.clone();
    QCOMPARE(cloned->opacityMultiplier(), 0.4f);

    QJsonObject json = line.toJson();
    json["lineWidth"] = 5000.0;
    json["opacityMultiplier"] = -10.0;
    json["rotationDegrees"] = -1000.0;
    json["shadowOffsetX"] = 20000.0;
    json["shadowOffsetY"] = -20000.0;
    json["shadowBlur"] = -10.0;
    auto restored = DrawingPrimitive::createFromJson(json);
    QVERIFY(restored);
    QCOMPARE(restored->lineWidth(), DrawingPrimitive::kMaxLineWidth);
    QCOMPARE(restored->opacityMultiplier(), 0.0f);
    QCOMPARE(restored->rotationDegrees(), -360.0f);
    QCOMPARE(restored->shadowOffsetX(), DrawingPrimitive::kMaxShadowOffset);
    QCOMPARE(restored->shadowOffsetY(), -DrawingPrimitive::kMaxShadowOffset);
    QCOMPARE(restored->shadowBlur(), 0.0f);
}

void tst_PrimitiveRoundTrip::malformedCommonStyleRejected()
{
    LinePrimitive line(QVector2D(), QVector2D(10, 10));
    QJsonObject json = line.toJson();
    json["lineWidth"] = QStringLiteral("wide");
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    json = line.toJson();
    json["lineStyle"] = 999;
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    json = line.toJson();
    json["shadowColor"] = QStringLiteral("not-a-color");
    QVERIFY(!DrawingPrimitive::createFromJson(json));
}

void tst_PrimitiveRoundTrip::malformedScalarGeometryRejected()
{
    LinePrimitive line(QVector2D(), QVector2D(10, 10));
    QJsonObject json = line.toJson();
    json.remove(QStringLiteral("startX"));
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    json = line.toJson();
    json["endY"] = QStringLiteral("ten");
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    RectanglePrimitive rectangle(QVector2D(), QVector2D(20, 10));
    json = rectangle.toJson();
    json["bottomRightX"] =
        DrawingPrimitive::kMaxSerializedCoordinateMagnitude + 1.0;
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    CirclePrimitive circle(QVector2D(5, 5), 10.0f);
    json = circle.toJson();
    json["radius"] = -1.0;
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    ArcPrimitive arc(QVector2D(), 10.0f, 0.0f, 90.0f);
    json = arc.toJson();
    json["startAngle"] =
        DrawingPrimitive::kMaxSerializedCoordinateMagnitude + 1.0;
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    TextPrimitive text(QVector2D(4, 8), QStringLiteral("safe"));
    json = text.toJson();
    json["positionY"] = QJsonValue();
    QVERIFY(!DrawingPrimitive::createFromJson(json));

    QImage imageData(2, 2, QImage::Format_ARGB32);
    imageData.fill(Qt::black);
    ImagePrimitive image(imageData, QVector2D(), QVector2D(20, 20));
    json = image.toJson();
    json["sizeX"] = 0.0;
    QVERIFY(!DrawingPrimitive::createFromJson(json));
}

QTEST_MAIN(tst_PrimitiveRoundTrip)
#include "tst_PrimitiveRoundTrip.moc"
