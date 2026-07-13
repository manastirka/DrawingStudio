#include "CanvasZoom.h"

#include <QtTest>
#include <cmath>

class tst_CanvasZoom : public QObject {
    Q_OBJECT

private slots:
    void resolveScroll_prefersPixelDelta();
    void resolveScroll_usesAngleDelta();
    void resolveScroll_zeroWhenEmpty();

    void applyZoom_inIncreasesLevel();
    void applyZoom_outDecreasesLevel();
    void applyZoom_clampsMax();
    void applyZoom_keepsCursorWorldPoint();
    void applyZoom_ignoresTinyDelta();

    void zoomByFactor_inAndOut();
    void resetView_setsOriginAndOne();
    void clampSensitivity_bounds();
};

void tst_CanvasZoom::resolveScroll_prefersPixelDelta()
{
    QCOMPARE(CanvasZoom::resolveScrollDeltaY(QPoint(0, 12), QPoint(0, 120)),
             12.0f);
}

void tst_CanvasZoom::resolveScroll_usesAngleDelta()
{
    // angleDelta 120 → /8 = 15
    QCOMPARE(CanvasZoom::resolveScrollDeltaY(QPoint(), QPoint(0, 120)), 15.0f);
}

void tst_CanvasZoom::resolveScroll_zeroWhenEmpty()
{
    QCOMPARE(CanvasZoom::resolveScrollDeltaY(QPoint(), QPoint()), 0.0f);
}

void tst_CanvasZoom::applyZoom_inIncreasesLevel()
{
    float z = 1.0f;
    QVector2D center(0, 0);
    QVERIFY(CanvasZoom::applyZoomToCursor(z, center, 1.2f, 10.0f,
                                          QVector2D(0, 0)));
    QVERIFY(z > 1.0f);
}

void tst_CanvasZoom::applyZoom_outDecreasesLevel()
{
    float z = 1.0f;
    QVector2D center(0, 0);
    QVERIFY(CanvasZoom::applyZoomToCursor(z, center, 1.2f, -10.0f,
                                          QVector2D(0, 0)));
    QVERIFY(z < 1.0f);
}

void tst_CanvasZoom::applyZoom_clampsMax()
{
    float z = 19.0f;
    QVector2D center(0, 0);
    CanvasZoom::applyZoomToCursor(z, center, 2.0f, 10.0f, QVector2D(0, 0),
                                  0.05f, 20.0f);
    QCOMPARE(z, 20.0f);
}

void tst_CanvasZoom::applyZoom_keepsCursorWorldPoint()
{
    // World under cursor: viewCenter + mouseOffset / zoom
    float z = 1.0f;
    QVector2D center(10.0f, 20.0f);
    const QVector2D mouse(100.0f, 50.0f);
    const QVector2D worldBefore = center + mouse / z;

    QVERIFY(CanvasZoom::applyZoomToCursor(z, center, 2.0f, 10.0f, mouse));
    const QVector2D worldAfter = center + mouse / z;

    QCOMPARE(worldAfter.x(), worldBefore.x());
    QCOMPARE(worldAfter.y(), worldBefore.y());
}

void tst_CanvasZoom::applyZoom_ignoresTinyDelta()
{
    float z = 1.0f;
    QVector2D center(0, 0);
    QVERIFY(!CanvasZoom::applyZoomToCursor(z, center, 1.2f, 0.05f,
                                           QVector2D(0, 0)));
    QCOMPARE(z, 1.0f);
}

void tst_CanvasZoom::zoomByFactor_inAndOut()
{
    float z = 1.0f;
    QVERIFY(CanvasZoom::zoomByFactor(z, CanvasZoom::kButtonZoomFactor));
    QVERIFY(z > 1.0f);
    QVERIFY(CanvasZoom::zoomByFactor(z, 1.0f / CanvasZoom::kButtonZoomFactor));
    QCOMPARE(z, 1.0f);
}

void tst_CanvasZoom::resetView_setsOriginAndOne()
{
    float z = 4.0f;
    QVector2D center(50.0f, -20.0f);
    CanvasZoom::resetView(z, center);
    QCOMPARE(z, 1.0f);
    QCOMPARE(center, QVector2D(0.0f, 0.0f));
}

void tst_CanvasZoom::clampSensitivity_bounds()
{
    QCOMPARE(CanvasZoom::clampSensitivity(1.0f), CanvasZoom::kMinSensitivity);
    QCOMPARE(CanvasZoom::clampSensitivity(5.0f), CanvasZoom::kMaxSensitivity);
    QCOMPARE(CanvasZoom::clampSensitivity(1.5f), 1.5f);
}

QTEST_MAIN(tst_CanvasZoom)
#include "tst_CanvasZoom.moc"
