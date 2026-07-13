#include "UnitsConverter.h"

#include <QtTest>
#include <cmath>

class tst_UnitsConverter : public QObject {
    Q_OBJECT

private slots:
    void defaults();
    void unitsString();
    void worldToUnits_mm();
    void worldToUnits_cm();
    void worldToUnits_in();
    void unitsToWorld_roundTrip();
    void pixelsPerUnit();
};

void tst_UnitsConverter::defaults()
{
    UnitsConverter u;
    QCOMPARE(u.units(), UnitsConverter::Units::Millimeters);
    QCOMPARE(u.pixelsPerMM(), UnitsConverter::DEFAULT_PIXELS_PER_MM);
    QCOMPARE(u.unitsString(), QStringLiteral("mm"));
}

void tst_UnitsConverter::unitsString()
{
    UnitsConverter u;
    u.setUnits(UnitsConverter::Units::Centimeters);
    QCOMPARE(u.unitsString(), QStringLiteral("cm"));
    u.setUnits(UnitsConverter::Units::Inches);
    QCOMPARE(u.unitsString(), QStringLiteral("in"));
}

void tst_UnitsConverter::worldToUnits_mm()
{
    UnitsConverter u;
    u.setUnits(UnitsConverter::Units::Millimeters);
    u.setPixelsPerMM(10.0f); // 10 px = 1 mm
    QCOMPARE(u.worldToUnits(50.0f), 5.0f);
}

void tst_UnitsConverter::worldToUnits_cm()
{
    UnitsConverter u;
    u.setUnits(UnitsConverter::Units::Centimeters);
    u.setPixelsPerMM(10.0f); // 10 px = 1 mm → 100 px = 1 cm
    QCOMPARE(u.worldToUnits(100.0f), 1.0f);
}

void tst_UnitsConverter::worldToUnits_in()
{
    UnitsConverter u;
    u.setUnits(UnitsConverter::Units::Inches);
    u.setPixelsPerMM(25.4f); // 25.4 px = 1 mm → 25.4*25.4 px = 1 in
    const float oneInchPx = 25.4f * 25.4f;
    QVERIFY(std::abs(u.worldToUnits(oneInchPx) - 1.0f) < 1e-4f);
}

void tst_UnitsConverter::unitsToWorld_roundTrip()
{
    UnitsConverter u;
    u.setUnits(UnitsConverter::Units::Millimeters);
    u.setPixelsPerMM(3.77953f);
    const float world = 100.0f;
    const float units = u.worldToUnits(world);
    QVERIFY(std::abs(u.unitsToWorld(units) - world) < 0.01f);

    u.setUnits(UnitsConverter::Units::Inches);
    const float unitsIn = u.worldToUnits(world);
    QVERIFY(std::abs(u.unitsToWorld(unitsIn) - world) < 0.01f);
}

void tst_UnitsConverter::pixelsPerUnit()
{
    UnitsConverter u;
    u.setPixelsPerMM(4.0f);
    u.setUnits(UnitsConverter::Units::Millimeters);
    QCOMPARE(u.pixelsPerUnit(), 4.0);
    u.setUnits(UnitsConverter::Units::Centimeters);
    QCOMPARE(u.pixelsPerUnit(), 40.0);
    u.setUnits(UnitsConverter::Units::Inches);
    QCOMPARE(u.pixelsPerUnit(), 4.0 * 25.4);
}

QTEST_MAIN(tst_UnitsConverter)
#include "tst_UnitsConverter.moc"
