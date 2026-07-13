#include "ControlPointHit.h"

#include <QtTest>

class tst_ControlPointHit : public QObject {
    Q_OBJECT

private slots:
    void firstWithin_returnsFirstMatch();
    void closestWithin_picksNearest();
    void misses_returnMinusOne();
};

void tst_ControlPointHit::firstWithin_returnsFirstMatch()
{
    const std::vector<QVector2D> pts{{0, 0}, {1, 0}, {2, 0}};
    QCOMPARE(ControlPointHit::firstWithin(pts, QVector2D(0.5f, 0), 1.0f), 0);
}

void tst_ControlPointHit::closestWithin_picksNearest()
{
    const std::vector<QVector2D> pts{{0, 0}, {1, 0}, {2, 0}};
    QCOMPARE(ControlPointHit::closestWithin(pts, QVector2D(1.1f, 0), 1.0f), 1);
}

void tst_ControlPointHit::misses_returnMinusOne()
{
    const std::vector<QVector2D> pts{{0, 0}, {10, 0}};
    QCOMPARE(ControlPointHit::firstWithin(pts, QVector2D(5, 0), 1.0f), -1);
    QCOMPARE(ControlPointHit::closestWithin(pts, QVector2D(5, 0), 1.0f), -1);
}

QTEST_MAIN(tst_ControlPointHit)
#include "tst_ControlPointHit.moc"
