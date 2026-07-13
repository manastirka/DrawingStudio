#include "PaperModel.h"

#include <QtTest>
#include <cmath>

class tst_PaperModel : public QObject {
    Q_OBJECT

private slots:
    void defaultsA4();
    void setFormatUpdatesSize();
    void customKeepsSize();
    void formatNameA4();
    void worldRectCentered();
    void sizeForFormatTable();
};

void tst_PaperModel::defaultsA4()
{
    PaperModel paper;
    QCOMPARE(paper.format(), PaperModel::Format::A4);
    QCOMPARE(paper.sizeMm().width(), 210.0);
    QCOMPARE(paper.sizeMm().height(), 297.0);
    QCOMPARE(paper.color(), QColor(255, 255, 255));
}

void tst_PaperModel::setFormatUpdatesSize()
{
    PaperModel paper;
    paper.setFormat(PaperModel::Format::A3);
    QCOMPARE(paper.format(), PaperModel::Format::A3);
    QCOMPARE(paper.sizeMm(), QSizeF(297.0, 420.0));
}

void tst_PaperModel::customKeepsSize()
{
    PaperModel paper;
    paper.setFormat(PaperModel::Format::Letter);
    paper.setSizeMm(QSizeF(100, 200));
    paper.setFormat(PaperModel::Format::Custom);
    QCOMPARE(paper.sizeMm(), QSizeF(100, 200));
}

void tst_PaperModel::formatNameA4()
{
    PaperModel paper;
    QVERIFY(paper.formatName().contains(QStringLiteral("A4")));
}

void tst_PaperModel::worldRectCentered()
{
    PaperModel paper;
    paper.setSizeMm(QSizeF(100, 200)); // mm
    const float ppm = 2.0f;            // 2 px per mm
    const QRectF r = paper.worldRect(ppm);
    QCOMPARE(r.width(), 200.0);
    QCOMPARE(r.height(), 400.0);
    QVERIFY(std::abs(r.center().x()) < 1e-4);
    QVERIFY(std::abs(r.center().y()) < 1e-4);
}

void tst_PaperModel::sizeForFormatTable()
{
    QCOMPARE(PaperModel::sizeForFormat(PaperModel::Format::A0),
             QSizeF(841.0, 1189.0));
    QCOMPARE(PaperModel::sizeForFormat(PaperModel::Format::Letter),
             QSizeF(216.0, 279.0));
    QVERIFY(PaperModel::sizeForFormat(PaperModel::Format::Custom).isEmpty());
}

QTEST_MAIN(tst_PaperModel)
#include "tst_PaperModel.moc"
