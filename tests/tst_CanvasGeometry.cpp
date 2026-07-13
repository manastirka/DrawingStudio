#include "CanvasGeometry.h"

#include <QtTest>
#include <QtMath>
#include <cmath>

class tst_CanvasGeometry : public QObject {
    Q_OBJECT

private slots:
    void projectPointOntoSegment_midpoint();
    void projectPointOntoSegment_clampsToEnds();
    void projectPointOntoSegment_degenerate();

    void computeCircle_unitCircle();
    void computeCircle_collinearFails();

    void pointInPolygon_square();
    void pointInPolygon_tooFewVertices();

    void snapAngle_15degSteps();
    void snapAngle_altBypassesSnap();
};

void tst_CanvasGeometry::projectPointOntoSegment_midpoint()
{
    const QVector2D a(0, 0);
    const QVector2D b(10, 0);
    const QVector2D p(5, 4);
    const QVector2D hit = CanvasGeometry::projectPointOntoLineSegment(p, a, b);
    QCOMPARE(hit.x(), 5.0f);
    QCOMPARE(hit.y(), 0.0f);
}

void tst_CanvasGeometry::projectPointOntoSegment_clampsToEnds()
{
    const QVector2D a(0, 0);
    const QVector2D b(10, 0);
    QCOMPARE(CanvasGeometry::projectPointOntoLineSegment(QVector2D(-5, 3), a, b), a);
    QCOMPARE(CanvasGeometry::projectPointOntoLineSegment(QVector2D(15, -2), a, b), b);
}

void tst_CanvasGeometry::projectPointOntoSegment_degenerate()
{
    const QVector2D a(3, 4);
    QCOMPARE(CanvasGeometry::projectPointOntoLineSegment(QVector2D(0, 0), a, a), a);
}

void tst_CanvasGeometry::computeCircle_unitCircle()
{
    // Points on unit circle around origin
    QVector2D c;
    float r = 0.f;
    const bool ok = CanvasGeometry::computeCircleThroughPoints(
        QVector2D(1, 0), QVector2D(0, 1), QVector2D(-1, 0), c, r);
    QVERIFY(ok);
    QVERIFY(std::abs(c.x()) < 1e-3f);
    QVERIFY(std::abs(c.y()) < 1e-3f);
    QVERIFY(std::abs(r - 1.0f) < 1e-3f);
}

void tst_CanvasGeometry::computeCircle_collinearFails()
{
    QVector2D c;
    float r = 0.f;
    QVERIFY(!CanvasGeometry::computeCircleThroughPoints(
        QVector2D(0, 0), QVector2D(1, 0), QVector2D(2, 0), c, r));
}

void tst_CanvasGeometry::pointInPolygon_square()
{
    const std::vector<QVector2D> square = {
        {0, 0}, {10, 0}, {10, 10}, {0, 10}};
    QVERIFY(CanvasGeometry::pointInPolygon(QVector2D(5, 5), square));
    QVERIFY(!CanvasGeometry::pointInPolygon(QVector2D(15, 5), square));
    QVERIFY(!CanvasGeometry::pointInPolygon(QVector2D(-1, -1), square));
}

void tst_CanvasGeometry::pointInPolygon_tooFewVertices()
{
    QVERIFY(!CanvasGeometry::pointInPolygon(QVector2D(0, 0),
                                            {{0, 0}, {1, 0}}));
}

void tst_CanvasGeometry::snapAngle_15degSteps()
{
    // Baseline along +X; free end at ~20° should snap to 15°
    const QVector2D origin(0, 0);
    const float ang = 20.0f * static_cast<float>(M_PI) / 180.0f;
    const QVector2D raw(std::cos(ang) * 100.f, std::sin(ang) * 100.f);
    const QVector2D snapped = CanvasGeometry::snapAngleLineEndpoint(
        origin, raw, QVector2D(0, 0), QVector2D(10, 0), Qt::NoModifier);

    const float outAng =
        std::atan2(snapped.y(), snapped.x()) * 180.0f / static_cast<float>(M_PI);
    QVERIFY(std::abs(outAng - 15.0f) < 0.5f);
    QVERIFY(std::abs(snapped.length() - 100.f) < 0.1f);
}

void tst_CanvasGeometry::snapAngle_altBypassesSnap()
{
    const QVector2D origin(0, 0);
    const float ang = 20.0f * static_cast<float>(M_PI) / 180.0f;
    const QVector2D raw(std::cos(ang) * 50.f, std::sin(ang) * 50.f);
    const QVector2D free = CanvasGeometry::snapAngleLineEndpoint(
        origin, raw, QVector2D(0, 0), QVector2D(10, 0), Qt::AltModifier);
    QCOMPARE(free, raw);
}

QTEST_MAIN(tst_CanvasGeometry)
#include "tst_CanvasGeometry.moc"
