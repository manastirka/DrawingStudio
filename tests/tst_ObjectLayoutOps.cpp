#include "ObjectLayoutOps.h"
#include "DrawingPrimitive.h"

#include <QtTest>
#include <cmath>

class tst_ObjectLayoutOps : public QObject {
    Q_OBJECT

private slots:
    void alignEmpty_returnsZero();
    void alignLeft_movesRightObject();
    void alignPageCenterHorizontal();
    void distributeHorizontal_requiresThree();
    void distributeHorizontal_evenGaps();
    void distributeVertical_evenGaps();
};

void tst_ObjectLayoutOps::alignEmpty_returnsZero()
{
    std::vector<DrawingPrimitive *> empty;
    QCOMPARE(ObjectLayoutOps::alignObjects(empty, ObjectLayoutOps::Align::Left, {}), 0);
}

void tst_ObjectLayoutOps::alignLeft_movesRightObject()
{
    // Geometry corners (boundingRect adds stroke padding — compare relatively).
    RectanglePrimitive a(QVector2D(0, 0), QVector2D(20, 20));
    RectanglePrimitive b(QVector2D(100, 0), QVector2D(120, 20));
    a.setLineWidth(0);
    b.setLineWidth(0);
    std::vector<DrawingPrimitive *> objs{&a, &b};

    const double aLeftBefore = a.boundingRect().left();
    const int n = ObjectLayoutOps::alignObjects(objs, ObjectLayoutOps::Align::Left, {});
    QCOMPARE(n, 2);
    // Both share the left edge; A (already leftmost) stays put.
    QVERIFY(std::abs(a.boundingRect().left() - aLeftBefore) < 0.01);
    QVERIFY(std::abs(b.boundingRect().left() - a.boundingRect().left()) < 0.01);
    QVERIFY(b.boundingRect().left() < 50.0); // moved left from ~100
}

void tst_ObjectLayoutOps::alignPageCenterHorizontal()
{
    RectanglePrimitive a(QVector2D(0, 0), QVector2D(20, 10)); // center x=10
    a.setLineWidth(0);
    const QRectF page(0, 0, 200, 100); // center x=100
    std::vector<DrawingPrimitive *> objs{&a};

    QCOMPARE(ObjectLayoutOps::alignObjects(
                 objs, ObjectLayoutOps::Align::PageCenterHorizontal, page),
             1);
    QVERIFY(std::abs(a.boundingRect().center().x() - 100.0) < 0.5);
}

void tst_ObjectLayoutOps::distributeHorizontal_requiresThree()
{
    RectanglePrimitive a(QVector2D(0, 0), QVector2D(10, 10));
    RectanglePrimitive b(QVector2D(50, 0), QVector2D(60, 10));
    std::vector<DrawingPrimitive *> two{&a, &b};
    QCOMPARE(ObjectLayoutOps::distributeObjects(two, true), 0);
}

void tst_ObjectLayoutOps::distributeHorizontal_evenGaps()
{
    // Three 10-wide rects — after distribute, equal gaps; outer edges fixed.
    RectanglePrimitive a(QVector2D(0, 0), QVector2D(10, 10));
    RectanglePrimitive b(QVector2D(20, 0), QVector2D(30, 10));
    RectanglePrimitive c(QVector2D(90, 0), QVector2D(100, 10));
    a.setLineWidth(0);
    b.setLineWidth(0);
    c.setLineWidth(0);
    std::vector<DrawingPrimitive *> objs{&a, &b, &c};

    const double outerLeft = a.boundingRect().left();
    const double outerRight = c.boundingRect().right();
    QCOMPARE(ObjectLayoutOps::distributeObjects(objs, true), 3);

    QVERIFY(std::abs(a.boundingRect().left() - outerLeft) < 0.01);
    QVERIFY(std::abs(c.boundingRect().right() - outerRight) < 0.01);

    const double gap1 = b.boundingRect().left() - a.boundingRect().right();
    const double gap2 = c.boundingRect().left() - b.boundingRect().right();
    QVERIFY(std::abs(gap1 - gap2) < 0.5);
}

void tst_ObjectLayoutOps::distributeVertical_evenGaps()
{
    RectanglePrimitive a(QVector2D(0, 0), QVector2D(10, 10));
    RectanglePrimitive b(QVector2D(0, 20), QVector2D(10, 30));
    RectanglePrimitive c(QVector2D(0, 90), QVector2D(10, 100));
    a.setLineWidth(0);
    b.setLineWidth(0);
    c.setLineWidth(0);
    std::vector<DrawingPrimitive *> objs{&a, &b, &c};

    const double outerTop = a.boundingRect().top();
    const double outerBottom = c.boundingRect().bottom();
    QCOMPARE(ObjectLayoutOps::distributeObjects(objs, false), 3);
    QVERIFY(std::abs(a.boundingRect().top() - outerTop) < 0.01);
    QVERIFY(std::abs(c.boundingRect().bottom() - outerBottom) < 0.01);

    const double gap1 = b.boundingRect().top() - a.boundingRect().bottom();
    const double gap2 = c.boundingRect().top() - b.boundingRect().bottom();
    QVERIFY(std::abs(gap1 - gap2) < 0.5);
}

QTEST_MAIN(tst_ObjectLayoutOps)
#include "tst_ObjectLayoutOps.moc"
