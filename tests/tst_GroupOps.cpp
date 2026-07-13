#include "GroupOps.h"

#include "DrawingPrimitive.h"

#include <QtTest>

class tst_GroupOps : public QObject {
    Q_OBJECT

private slots:
    void canGroup_requiresTwo();
    void assignAndClear_roundTrip();
    void anyGrouped_detects();
    void collectMembers_filtersById();
};

void tst_GroupOps::canGroup_requiresTwo()
{
    LinePrimitive a(QVector2D(0, 0), QVector2D(1, 0));
    LinePrimitive b(QVector2D(0, 1), QVector2D(1, 1));
    std::vector<DrawingPrimitive *> one{&a};
    std::vector<DrawingPrimitive *> two{&a, &b};
    QVERIFY(!GroupOps::canGroup(one));
    QVERIFY(GroupOps::canGroup(two));
}

void tst_GroupOps::assignAndClear_roundTrip()
{
    LinePrimitive a(QVector2D(0, 0), QVector2D(1, 0));
    LinePrimitive b(QVector2D(0, 1), QVector2D(1, 1));
    std::vector<DrawingPrimitive *> sel{&a, &b};
    const QUuid gid = GroupOps::assignNewGroup(sel);
    QVERIFY(!gid.isNull());
    QCOMPARE(a.groupId(), gid);
    QCOMPARE(b.groupId(), gid);
    GroupOps::clearGroups(sel);
    QVERIFY(a.groupId().isNull());
    QVERIFY(b.groupId().isNull());
}

void tst_GroupOps::anyGrouped_detects()
{
    LinePrimitive a(QVector2D(0, 0), QVector2D(1, 0));
    LinePrimitive b(QVector2D(0, 1), QVector2D(1, 1));
    std::vector<DrawingPrimitive *> sel{&a, &b};
    QVERIFY(!GroupOps::anyGrouped(sel));
    a.setGroupId(QUuid::createUuid());
    QVERIFY(GroupOps::anyGrouped(sel));
}

void tst_GroupOps::collectMembers_filtersById()
{
    // Legacy list path
    auto p1 = std::make_unique<LinePrimitive>(QVector2D(0, 0), QVector2D(1, 0));
    auto p2 = std::make_unique<LinePrimitive>(QVector2D(0, 1), QVector2D(1, 1));
    auto p3 = std::make_unique<LinePrimitive>(QVector2D(0, 2), QVector2D(1, 2));
    const QUuid gid = QUuid::createUuid();
    p1->setGroupId(gid);
    p2->setGroupId(gid);
    // p3 ungrouped
    GroupOps::PrimitiveList legacy;
    DrawingPrimitive *raw1 = p1.get();
    DrawingPrimitive *raw2 = p2.get();
    legacy.push_back(std::move(p1));
    legacy.push_back(std::move(p2));
    legacy.push_back(std::move(p3));

    const auto members = GroupOps::collectMembers(nullptr, legacy, gid);
    QCOMPARE(members.size(), size_t(2));
    QVERIFY(members[0] == raw1 || members[1] == raw1);
    QVERIFY(members[0] == raw2 || members[1] == raw2);
}

QTEST_MAIN(tst_GroupOps)
#include "tst_GroupOps.moc"
