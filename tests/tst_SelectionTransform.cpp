#include "SelectionTransform.h"

#include <QImage>
#include <QPainter>
#include <QtTest>
#include <cmath>

class tst_SelectionTransform : public QObject {
    Q_OBJECT

private slots:
    void handlePositions_cornersAndRotate();
    void hitTest_corner();
    void hitTest_miss();
    void cursorForRotate();
    void resizeFromCorner_basic();
    void resizeKeepAspect_shift();
    void resizeFromCenter_alt();
    void drawHandles_smoke();
};

void tst_SelectionTransform::handlePositions_cornersAndRotate()
{
    const QRectF br(0, 0, 100, 50);
    QPointF out[9];
    SelectionTransform::handlePositions(br, 1.0f, out);
    QCOMPARE(out[0], br.topLeft());
    QCOMPARE(out[4], br.bottomRight());
    QCOMPARE(out[1].x(), br.center().x());
    // Rotate handle above top (larger Y in this coord system)
    QVERIFY(out[SelectionTransform::kHandleRotate].y() > br.top());
}

void tst_SelectionTransform::hitTest_corner()
{
    const QRectF br(0, 0, 100, 100);
    const int hit = SelectionTransform::hitTest(
        br, QVector2D(0, 0), 1.0f, true);
    QCOMPARE(hit, 0); // top-left
}

void tst_SelectionTransform::hitTest_miss()
{
    const QRectF br(0, 0, 100, 100);
    const int hit = SelectionTransform::hitTest(
        br, QVector2D(1000, 1000), 1.0f, true);
    QCOMPARE(hit, -1);
}

void tst_SelectionTransform::cursorForRotate()
{
    QCOMPARE(SelectionTransform::cursorForHandle(SelectionTransform::kHandleRotate),
             Qt::PointingHandCursor);
    QCOMPARE(SelectionTransform::cursorForHandle(3), Qt::SizeHorCursor);
}

void tst_SelectionTransform::resizeFromCorner_basic()
{
    const QRectF orig(0, 0, 100, 50);
    // Drag bottom-right (4) to (150, 80)
    const QRectF nr = SelectionTransform::computeResizedBounds(
        orig, 4, QVector2D(150, 80), Qt::NoModifier);
    QVERIFY(nr.width() > 100);
    QVERIFY(nr.height() > 50);
    QVERIFY(std::abs(nr.left() - 0) < 0.1);
    QVERIFY(std::abs(nr.top() - 0) < 0.1);
}

void tst_SelectionTransform::resizeKeepAspect_shift()
{
    const QRectF orig(0, 0, 100, 50); // aspect 2:1
    // Drag BR with Shift — aspect should stay ~2
    const QRectF nr = SelectionTransform::computeResizedBounds(
        orig, 4, QVector2D(200, 200), Qt::ShiftModifier);
    const float aspect = static_cast<float>(nr.width() / nr.height());
    QVERIFY(std::abs(aspect - 2.0f) < 0.05f);
}

void tst_SelectionTransform::resizeFromCenter_alt()
{
    const QRectF orig(0, 0, 100, 100);
    const QRectF nr = SelectionTransform::computeResizedBounds(
        orig, 4, QVector2D(100, 100), Qt::AltModifier);
    // Center should stay near original center
    QVERIFY(std::abs(nr.center().x() - orig.center().x()) < 1.0);
    QVERIFY(std::abs(nr.center().y() - orig.center().y()) < 1.0);
}

void tst_SelectionTransform::drawHandles_smoke()
{
    QImage img(128, 128, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    SelectionTransform::drawHandles(p, QRectF(20, 20, 80, 60), 1.0f, true);
    p.end();
    QVERIFY(!img.isNull());
}

QTEST_MAIN(tst_SelectionTransform)
#include "tst_SelectionTransform.moc"
