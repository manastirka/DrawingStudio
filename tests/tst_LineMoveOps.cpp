#include "LineMoveOps.h"

#include "DrawingPrimitive.h"

#include <QtTest>

class tst_LineMoveOps : public QObject {
    Q_OBJECT

private slots:
    void isCopyAlongModifier_ctrlOrMeta();
    void resolveConstraint_usesOwnDirection();
    void constrainDelta_projectsOntoAxis();
    void constrainDelta_altFrees();
    void collectConstrained_filters();
};

void tst_LineMoveOps::isCopyAlongModifier_ctrlOrMeta()
{
    QVERIFY(LineMoveOps::isCopyAlongModifier(Qt::ControlModifier));
    QVERIFY(LineMoveOps::isCopyAlongModifier(Qt::MetaModifier));
    QVERIFY(!LineMoveOps::isCopyAlongModifier(Qt::NoModifier));
    QVERIFY(!LineMoveOps::isCopyAlongModifier(Qt::ShiftModifier));
}

void tst_LineMoveOps::resolveConstraint_usesOwnDirection()
{
    LinePrimitive line(QVector2D(0, 0), QVector2D(10, 0));
    line.setMoveConstraintDirection(QVector2D(0, 1));
    const QVector2D dir = LineMoveOps::resolveConstraint(
        &line, [](const QUuid &) { return nullptr; });
    QCOMPARE(dir.x(), 0.0f);
    QCOMPARE(dir.y(), 1.0f);
}

void tst_LineMoveOps::constrainDelta_projectsOntoAxis()
{
    LinePrimitive line(QVector2D(0, 0), QVector2D(10, 0));
    line.setMoveConstraintDirection(QVector2D(1, 0));
    std::vector<DrawingPrimitive *> sel{&line};
    const QVector2D out = LineMoveOps::constrainDelta(
        QVector2D(3, 4), Qt::NoModifier, sel,
        [](const QUuid &) { return nullptr; });
    QCOMPARE(out.x(), 3.0f);
    QCOMPARE(out.y(), 0.0f);
}

void tst_LineMoveOps::constrainDelta_altFrees()
{
    LinePrimitive line(QVector2D(0, 0), QVector2D(10, 0));
    line.setMoveConstraintDirection(QVector2D(1, 0));
    std::vector<DrawingPrimitive *> sel{&line};
    const QVector2D out = LineMoveOps::constrainDelta(
        QVector2D(3, 4), Qt::AltModifier, sel,
        [](const QUuid &) { return nullptr; });
    QCOMPARE(out, QVector2D(3, 4));
}

void tst_LineMoveOps::collectConstrained_filters()
{
    LinePrimitive free(QVector2D(0, 0), QVector2D(1, 0));
    LinePrimitive locked(QVector2D(0, 0), QVector2D(1, 0));
    locked.setMoveConstraintDirection(QVector2D(1, 0));
    std::vector<DrawingPrimitive *> sel{&free, &locked};
    const auto sources = LineMoveOps::collectConstrainedLines(
        sel, [](const QUuid &) { return nullptr; });
    QCOMPARE(sources.size(), size_t(1));
    QCOMPARE(sources[0], &locked);
}

QTEST_MAIN(tst_LineMoveOps)
#include "tst_LineMoveOps.moc"
