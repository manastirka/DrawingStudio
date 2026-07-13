#include "TextResizeOps.h"

#include "DrawingPrimitive.h"

#include <QtTest>

class tst_TextResizeOps : public QObject {
    Q_OBJECT

private slots:
    void fromInitial_readsBounds();
    void shouldCommit_xOrWidthOnly();
    void capture_readsText();
};

void tst_TextResizeOps::fromInitial_readsBounds()
{
    const auto s =
        TextResizeOps::fromInitial(QRectF(10, 20, 100, 40), 14.0f);
    QCOMPARE(s.position, QVector2D(10, 20));
    QCOMPARE(s.width, 100.0f);
    QCOMPARE(s.height, 40.0f);
    QCOMPARE(s.fontSize, 14.0f);
}

void tst_TextResizeOps::shouldCommit_xOrWidthOnly()
{
    TextResizeOps::State before =
        TextResizeOps::fromInitial(QRectF(0, 0, 50, 20), 12.0f);
    TextResizeOps::State after = before;
    QVERIFY(!TextResizeOps::shouldCommit(before, after));
    after.position.setY(5.0f); // y-only change — legacy ignores
    QVERIFY(!TextResizeOps::shouldCommit(before, after));
    after.position.setX(1.0f);
    QVERIFY(TextResizeOps::shouldCommit(before, after));
    after = before;
    after.width = 60.0f;
    QVERIFY(TextResizeOps::shouldCommit(before, after));
}

void tst_TextResizeOps::capture_readsText()
{
    TextPrimitive text(QVector2D(5, 6), QStringLiteral("Hi"));
    text.setTextBoxWidth(80.0f);
    text.setTextBoxHeight(30.0f);
    text.setFontSize(16.0f);
    const auto s = TextResizeOps::capture(&text);
    QCOMPARE(s.position, QVector2D(5, 6));
    QCOMPARE(s.width, 80.0f);
    QCOMPARE(s.height, 30.0f);
    QCOMPARE(s.fontSize, 16.0f);
}

QTEST_MAIN(tst_TextResizeOps)
#include "tst_TextResizeOps.moc"
