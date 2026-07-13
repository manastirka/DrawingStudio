#include "CanvasCoords.h"

#include <QImage>
#include <QPainter>
#include <QtTest>
#include <cmath>

class tst_CanvasCoords : public QObject {
    Q_OBJECT

private slots:
    void screenToWorld_centerIsViewCenter();
    void worldToScreen_roundTrip();
    void pixelSnap_rounds();
    void mouseOffsetFromCenter();
    void applyWorldTransform_centersOrigin();
};

void tst_CanvasCoords::screenToWorld_centerIsViewCenter()
{
    const QVector2D center(10.0f, 20.0f);
    const QVector2D w = CanvasCoords::screenToWorld(QPoint(400, 300), 800, 600,
                                                    1.0f, center);
    QCOMPARE(w.x(), 10.0f);
    QCOMPARE(w.y(), 20.0f);
}

void tst_CanvasCoords::worldToScreen_roundTrip()
{
    const QVector2D center(5.0f, -3.0f);
    const float zoom = 2.0f;
    const QVector2D world(15.0f, 7.0f);
    const QPoint screen =
        CanvasCoords::worldToScreen(world, 800, 600, zoom, center);
    const QVector2D back =
        CanvasCoords::screenToWorld(screen, 800, 600, zoom, center);
    QVERIFY(std::abs(back.x() - world.x()) < 1.0f); // int screen rounding
    QVERIFY(std::abs(back.y() - world.y()) < 1.0f);
}

void tst_CanvasCoords::pixelSnap_rounds()
{
    const QVector2D w = CanvasCoords::screenToWorld(QPoint(401, 299), 800, 600,
                                                    1.0f, QVector2D(0, 0),
                                                    true);
    QCOMPARE(w.x(), 1.0f);
    QCOMPARE(w.y(), 1.0f);
}

void tst_CanvasCoords::mouseOffsetFromCenter()
{
    const QVector2D off =
        CanvasCoords::mouseOffsetFromCenter(QPoint(500, 200), 800, 600);
    QCOMPARE(off.x(), 100.0f);
    QCOMPARE(off.y(), 100.0f); // 600/2 - 200
}

void tst_CanvasCoords::applyWorldTransform_centersOrigin()
{
    // Smoke: transform maps world origin to widget center (800x600, zoom 1)
    QImage img(800, 600, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    CanvasCoords::applyWorldTransform(p, 800, 600, 1.0f, QVector2D(0, 0));
    // After transform, world (0,0) → device (400, 300)
    const QPointF device = p.transform().map(QPointF(0, 0));
    QCOMPARE(device.x(), 400.0);
    QCOMPARE(device.y(), 300.0);
}

QTEST_MAIN(tst_CanvasCoords)
#include "tst_CanvasCoords.moc"
