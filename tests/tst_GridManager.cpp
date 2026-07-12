#include "GridManager.h"
#include <QtTest>

class tst_GridManager : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testDefaults();
  void testSetters();
  void testSnapToGrid();
  void testSnapToGridDisabled();
};

void tst_GridManager::initTestCase() {}

void tst_GridManager::cleanupTestCase() {}

void tst_GridManager::testDefaults() {
  GridManager grid;
  QCOMPARE(grid.isVisible(), true);
  QCOMPARE(grid.isSnapEnabled(), true);
  QCOMPARE(grid.gridSize(), GridManager::DEFAULT_GRID_SIZE);
}

void tst_GridManager::testSetters() {
  GridManager grid;

  grid.setVisible(false);
  QCOMPARE(grid.isVisible(), false);

  grid.setSnapEnabled(false);
  QCOMPARE(grid.isSnapEnabled(), false);

  grid.setGridSize(10.0f);
  QCOMPARE(grid.gridSize(), 10.0f);

  QColor color(255, 0, 0);
  grid.setGridColor(color);
  QCOMPARE(grid.gridColor(), color);
}

void tst_GridManager::testSnapToGrid() {
  GridManager grid;
  grid.setGridSize(10.0f);
  grid.setSnapEnabled(true);

  // Test exact match
  QVector2D p1(10.0f, 20.0f);
  QCOMPARE(grid.snapToGrid(p1), p1);

  // Test rounding down
  QVector2D p2(12.0f, 22.0f);
  QCOMPARE(grid.snapToGrid(p2), QVector2D(10.0f, 20.0f));

  // Test rounding up
  QVector2D p3(18.0f, 28.0f);
  QCOMPARE(grid.snapToGrid(p3), QVector2D(20.0f, 30.0f));

  // Test negative coordinates
  QVector2D p4(-12.0f, -22.0f);
  QCOMPARE(grid.snapToGrid(p4), QVector2D(-10.0f, -20.0f));
}

void tst_GridManager::testSnapToGridDisabled() {
  GridManager grid;
  grid.setGridSize(10.0f);
  grid.setSnapEnabled(false);

  QVector2D p1(12.34f, 56.78f);
  QCOMPARE(grid.snapToGrid(p1), p1);
}

QTEST_MAIN(tst_GridManager)
#include "tst_GridManager.moc"
