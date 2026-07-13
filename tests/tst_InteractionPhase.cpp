#include "InteractionPhase.h"

#include <QtTest>

class tst_InteractionPhase : public QObject {
    Q_OBJECT

private slots:
    void move_priorityOrder();
    void release_priorityOrder();
    void release_panIsMiddle();
};

void tst_InteractionPhase::move_priorityOrder()
{
    InteractionPhase::MoveContext c;
    c.editingControlPoints = true;
    c.brushing = true;
    QCOMPARE(InteractionPhase::classifyMove(c),
             InteractionPhase::Move::ControlPointEdit);

    c = {};
    c.brushing = true;
    c.panning = true;
    QCOMPARE(InteractionPhase::classifyMove(c),
             InteractionPhase::Move::BrushOrBlur);

    c = {};
    c.isSelectTool = true;
    QCOMPARE(InteractionPhase::classifyMove(c),
             InteractionPhase::Move::SelectHover);

    c = {};
    c.isDrawing = true;
    QCOMPARE(InteractionPhase::classifyMove(c),
             InteractionPhase::Move::ActiveTool);
}

void tst_InteractionPhase::release_priorityOrder()
{
    InteractionPhase::ReleaseContext c;
    c.leftButton = true;
    c.resizingText = true;
    c.moving = true;
    QCOMPARE(InteractionPhase::classifyRelease(c),
             InteractionPhase::Release::ResizeText);

    c = {};
    c.leftButton = true;
    c.isDrawing = true;
    QCOMPARE(InteractionPhase::classifyRelease(c),
             InteractionPhase::Release::DrawingSession);
}

void tst_InteractionPhase::release_panIsMiddle()
{
    InteractionPhase::ReleaseContext c;
    c.middleButton = true;
    c.panning = true;
    c.leftButton = false;
    QCOMPARE(InteractionPhase::classifyRelease(c),
             InteractionPhase::Release::Pan);
}

QTEST_MAIN(tst_InteractionPhase)
#include "tst_InteractionPhase.moc"
