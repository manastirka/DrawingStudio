#include "PrimitiveJsonOps.h"

#include "DrawingPrimitive.h"

#include <QtTest>

class tst_PrimitiveJsonOps : public QObject {
    Q_OBJECT

private slots:
    void snapshot_skipsNulls();
    void snapshot_capturesGroupIdChange();
};

void tst_PrimitiveJsonOps::snapshot_skipsNulls()
{
    LinePrimitive a(QVector2D(0, 0), QVector2D(1, 0));
    std::vector<DrawingPrimitive *> objs{nullptr, &a, nullptr};
    const auto states = PrimitiveJsonOps::snapshotStates(objs);
    QCOMPARE(states.size(), size_t(1));
}

void tst_PrimitiveJsonOps::snapshot_capturesGroupIdChange()
{
    LinePrimitive a(QVector2D(0, 0), QVector2D(1, 0));
    std::vector<DrawingPrimitive *> objs{&a};
    const auto before = PrimitiveJsonOps::snapshotStates(objs);
    a.setGroupId(QUuid::createUuid());
    const auto after = PrimitiveJsonOps::snapshotStates(objs);
    QVERIFY(before[0] != after[0]);
}

QTEST_MAIN(tst_PrimitiveJsonOps)
#include "tst_PrimitiveJsonOps.moc"
