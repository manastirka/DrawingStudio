#include "SelectionPolicy.h"

#include "DrawingPrimitive.h"

#include <QtTest>
#include <cmath>

class tst_SelectionPolicy : public QObject {
    Q_OBJECT

private slots:
    void geometryCPs_pathsYes_shapesNo();
    void rotationHandle_imageAndRect();
    void externalRotation_onlyNonSelfRotators();
    void applyRotation_genericDegrees();
};

void tst_SelectionPolicy::geometryCPs_pathsYes_shapesNo()
{
    LinePrimitive line(QVector2D(0, 0), QVector2D(1, 0));
    RectanglePrimitive rect(QVector2D(0, 0), QVector2D(1, 1));
    QVERIFY(SelectionPolicy::showsGeometryControlPoints(&line));
    QVERIFY(!SelectionPolicy::showsGeometryControlPoints(&rect));
}

void tst_SelectionPolicy::rotationHandle_imageAndRect()
{
    RectanglePrimitive rect(QVector2D(0, 0), QVector2D(1, 1));
    LinePrimitive line(QVector2D(0, 0), QVector2D(1, 0));
    QVERIFY(SelectionPolicy::supportsRotationHandle(&rect));
    QVERIFY(!SelectionPolicy::supportsRotationHandle(&line));
}

void tst_SelectionPolicy::externalRotation_onlyNonSelfRotators()
{
    RectanglePrimitive rect(QVector2D(0, 0), QVector2D(10, 10));
    QVERIFY(!SelectionPolicy::usesExternalRotation(&rect));
    rect.setRotationDegrees(45.0f);
    QVERIFY(SelectionPolicy::usesExternalRotation(&rect));
}

void tst_SelectionPolicy::applyRotation_genericDegrees()
{
    RectanglePrimitive rect(QVector2D(0, 0), QVector2D(10, 10));
    SelectionPolicy::applyObjectRotation(&rect, 30.0f);
    QCOMPARE(SelectionPolicy::objectRotationDegrees(&rect), 30.0f);
}

QTEST_MAIN(tst_SelectionPolicy)
#include "tst_SelectionPolicy.moc"
