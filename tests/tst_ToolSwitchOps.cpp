#include "ToolSwitchOps.h"

#include <QtTest>

class tst_ToolSwitchOps : public QObject {
    Q_OBJECT

private slots:
    void isSwitch_detectsChange();
    void isLeavingTextTool();
    void resetCreationStages_zerosAll();
};

void tst_ToolSwitchOps::isSwitch_detectsChange()
{
    QVERIFY(ToolSwitchOps::isSwitch(DrawingTool::Select, DrawingTool::Line));
    QVERIFY(!ToolSwitchOps::isSwitch(DrawingTool::Line, DrawingTool::Line));
}

void tst_ToolSwitchOps::isLeavingTextTool()
{
    QVERIFY(ToolSwitchOps::isLeavingTextTool(DrawingTool::Text,
                                             DrawingTool::Select));
    QVERIFY(!ToolSwitchOps::isLeavingTextTool(DrawingTool::Select,
                                              DrawingTool::Text));
    QVERIFY(!ToolSwitchOps::isLeavingTextTool(DrawingTool::Text,
                                              DrawingTool::Text));
}

void tst_ToolSwitchOps::resetCreationStages_zerosAll()
{
    int b = 2, a = 1, arc = 3;
    ToolSwitchOps::resetCreationStages(b, a, arc);
    QCOMPARE(b, 0);
    QCOMPARE(a, 0);
    QCOMPARE(arc, 0);
}

QTEST_MAIN(tst_ToolSwitchOps)
#include "tst_ToolSwitchOps.moc"
