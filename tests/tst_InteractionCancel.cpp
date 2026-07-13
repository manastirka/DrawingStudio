#include "InteractionCancel.h"

#include <QtTest>

class tst_InteractionCancel : public QObject {
    Q_OBJECT

private slots:
    void applyEscape_clearsFlags();
    void endSpacePan_onlyWhenActive();
};

void tst_InteractionCancel::applyEscape_clearsFlags()
{
    bool drawing = true;
    bool panning = true;
    bool selecting = true;
    bool cleared = false;
    bool arrow = false;
    bool updated = false;

    InteractionCancel::Targets t;
    t.isDrawing = &drawing;
    t.isPanning = &panning;
    t.setIsSelecting = [&](bool v) { selecting = v; };
    t.clearSelection = [&]() { cleared = true; };
    t.setArrowCursor = [&]() { arrow = true; };
    t.requestUpdate = [&]() { updated = true; };

    InteractionCancel::applyEscape(t);
    QVERIFY(!drawing);
    QVERIFY(!panning);
    QVERIFY(!selecting);
    QVERIFY(cleared);
    QVERIFY(arrow);
    QVERIFY(updated);
}

void tst_InteractionCancel::endSpacePan_onlyWhenActive()
{
    bool panning = false;
    bool arrow = false;
    InteractionCancel::endSpacePan(&panning, [&]() { arrow = true; });
    QVERIFY(!arrow);

    panning = true;
    InteractionCancel::endSpacePan(&panning, [&]() { arrow = true; });
    QVERIFY(!panning);
    QVERIFY(arrow);
}

QTEST_MAIN(tst_InteractionCancel)
#include "tst_InteractionCancel.moc"
