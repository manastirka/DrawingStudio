#include "MagneticSnap.h"
#include "SmartDrawingConstraints.h"

#include <QtTest>
#include <cmath>

class tst_MagneticSnap : public QObject {
    Q_OBJECT

private slots:
    void nearestWithin_picksClosest();
    void nearestWithin_outsideToleranceKeepsPos();
    void snap_disabledReturnsPos();
    void snap_enabledSnaps();

    void smartLine_shiftLocksHorizontal();
    void smartRect_shiftLocksSquare();
    void smartEllipse_softNearCircle();
};

void tst_MagneticSnap::nearestWithin_picksClosest()
{
    const std::vector<QVector2D> pts = {{0, 0}, {10, 0}, {100, 100}};
    const QVector2D hit =
        MagneticSnap::nearestWithin(QVector2D(9, 1), 5.0f, pts);
    QCOMPARE(hit, QVector2D(10, 0));
}

void tst_MagneticSnap::nearestWithin_outsideToleranceKeepsPos()
{
    const std::vector<QVector2D> pts = {{0, 0}, {10, 0}};
    const QVector2D pos(50, 50);
    QCOMPARE(MagneticSnap::nearestWithin(pos, 5.0f, pts), pos);
}

void tst_MagneticSnap::snap_disabledReturnsPos()
{
    MagneticSnap snap;
    snap.setEnabled(false);
    snap.setTolerance(20.0f);
    const QVector2D pos(1, 1);
    QCOMPARE(snap.snap(pos, {{0, 0}}), pos);
}

void tst_MagneticSnap::snap_enabledSnaps()
{
    MagneticSnap snap;
    snap.setEnabled(true);
    snap.setTolerance(20.0f);
    const QVector2D hit = snap.snap(QVector2D(3, 4), {{0, 0}, {100, 0}});
    QCOMPARE(hit, QVector2D(0, 0));
}

void tst_MagneticSnap::smartLine_shiftLocksHorizontal()
{
    const QVector2D start(0, 0);
    const QVector2D raw(30, 10);
    QString hint;
    const QVector2D out = SmartDrawingConstraints::apply(
        SmartDrawingConstraints::ToolKind::LineOrMeasure, start, raw,
        Qt::ShiftModifier, &hint);
    QCOMPARE(out.y(), 0.0f);
    QCOMPARE(out.x(), 30.0f);
    QVERIFY(hint.contains(QStringLiteral("horizontal")));
}

void tst_MagneticSnap::smartRect_shiftLocksSquare()
{
    const QVector2D start(0, 0);
    const QVector2D raw(40, 10);
    QString hint;
    const QVector2D out = SmartDrawingConstraints::apply(
        SmartDrawingConstraints::ToolKind::Rectangle, start, raw,
        Qt::ShiftModifier, &hint);
    QVERIFY(std::abs(std::abs(out.x() - start.x()) -
                     std::abs(out.y() - start.y())) < 0.01f);
    QVERIFY(hint.contains(QStringLiteral("square")));
}

void tst_MagneticSnap::smartEllipse_softNearCircle()
{
    // Near-equal aspect without Shift → soft snap to circle
    const QVector2D start(0, 0);
    const QVector2D raw(50, 48); // ratio ~0.96
    QString hint;
    const QVector2D out = SmartDrawingConstraints::apply(
        SmartDrawingConstraints::ToolKind::Ellipse, start, raw, Qt::NoModifier,
        &hint);
    QVERIFY(std::abs(std::abs(out.x()) - std::abs(out.y())) < 0.01f);
    QVERIFY(!hint.isEmpty());
}

QTEST_MAIN(tst_MagneticSnap)
#include "tst_MagneticSnap.moc"
