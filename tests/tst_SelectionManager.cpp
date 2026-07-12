#include "DrawingPrimitive.h"
#include "SelectionManager.h"
#include <QtTest>
#include <memory>
#include <vector>

// Mock primitive for testing
class MockPrimitive : public DrawingPrimitive {
public:
  MockPrimitive(const QRectF &rect, const QColor &color)
      : DrawingPrimitive(PrimitiveType::Rectangle), m_rect(rect) {
    setColor(color);
  }

  void render(QPainter*) const override {}
  QRectF boundingRect() const override { return m_rect; }
  bool containsPoint(const QVector2D &point, float tolerance) const override {
    return m_rect.contains(point.toPointF());
  }
  std::unique_ptr<DrawingPrimitive> clone() const override {
    return std::make_unique<MockPrimitive>(m_rect, color());
  }
  void translate(const QVector2D &offset) override {
    m_rect.translate(offset.toPointF());
  }

private:
  QRectF m_rect;
};

class tst_SelectionManager : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testSelectionBasics();
  void testSelectObjectsInRect();
  void testSelectByColor();
  void testPipetteMode();

private:
  std::unique_ptr<SelectionManager> m_manager;
  std::vector<std::unique_ptr<DrawingPrimitive>> m_primitives;
};

void tst_SelectionManager::initTestCase() {
  m_manager = std::make_unique<SelectionManager>();
}

void tst_SelectionManager::cleanupTestCase() {
  m_manager.reset();
  m_primitives.clear();
}

void tst_SelectionManager::testSelectionBasics() {
  m_primitives.clear();
  m_primitives.push_back(
      std::make_unique<MockPrimitive>(QRectF(0, 0, 10, 10), Qt::red));
  auto *p1 = m_primitives.back().get();

  // Initial state
  QVERIFY(m_manager->selectedObjects().empty());
  QVERIFY(!m_manager->hasSelection());
  QVERIFY(!m_manager->isSelected(p1));

  // Add to selection
  m_manager->addToSelection(p1);
  QCOMPARE(m_manager->selectedObjects().size(), 1);
  QVERIFY(m_manager->isSelected(p1));
  QVERIFY(p1->isSelected());

  // Remove from selection
  m_manager->removeFromSelection(p1);
  QVERIFY(m_manager->selectedObjects().empty());
  QVERIFY(!p1->isSelected());

  // Clear selection
  m_manager->addToSelection(p1);
  m_manager->clearSelection();
  QVERIFY(m_manager->selectedObjects().empty());
  QVERIFY(!p1->isSelected());
}

void tst_SelectionManager::testSelectObjectsInRect() {
  m_primitives.clear();
  m_primitives.push_back(
      std::make_unique<MockPrimitive>(QRectF(0, 0, 10, 10), Qt::red)); // Inside
  m_primitives.push_back(std::make_unique<MockPrimitive>(QRectF(20, 20, 10, 10),
                                                         Qt::blue)); // Outside
  m_primitives.push_back(std::make_unique<MockPrimitive>(
      QRectF(5, 5, 10, 10), Qt::green)); // Intersecting

  auto *p1 = m_primitives[0].get();
  auto *p2 = m_primitives[1].get();
  auto *p3 = m_primitives[2].get();

  // Select rect covering p1 and p3 partially
  QRectF selectionRect(0, 0, 12, 12);
  m_manager->selectObjectsInRect(selectionRect, m_primitives);

  QVERIFY(m_manager->isSelected(p1));
  QVERIFY(!m_manager->isSelected(p2));
  QVERIFY(m_manager->isSelected(p3)); // Intersects
}

void tst_SelectionManager::testSelectByColor() {
  m_primitives.clear();
  m_primitives.push_back(
      std::make_unique<MockPrimitive>(QRectF(0, 0, 10, 10), Qt::red));
  m_primitives.push_back(
      std::make_unique<MockPrimitive>(QRectF(20, 20, 10, 10), Qt::blue));
  m_primitives.push_back(
      std::make_unique<MockPrimitive>(QRectF(40, 40, 10, 10), Qt::red));

  auto *p1 = m_primitives[0].get();
  auto *p2 = m_primitives[1].get();
  auto *p3 = m_primitives[2].get();

  m_manager->selectByColor(Qt::red, m_primitives);

  QVERIFY(m_manager->isSelected(p1));
  QVERIFY(!m_manager->isSelected(p2));
  QVERIFY(m_manager->isSelected(p3));
}

void tst_SelectionManager::testPipetteMode() {
  QVERIFY(!m_manager->isPipetteMode());
  m_manager->setPipetteMode(true);
  QVERIFY(m_manager->isPipetteMode());

  m_manager->setPipetteTolerance(20);
  QCOMPARE(m_manager->pipetteTolerance(), 20);
}

QTEST_MAIN(tst_SelectionManager)
#include "tst_SelectionManager.moc"
