#include "Command.h"
#include "CommandManager.h"
#include <QtTest>
#include <memory>

class StubCommand : public Command {
public:
  explicit StubCommand(QString desc = QStringLiteral("Stub"))
      : m_desc(std::move(desc)) {}

  void execute() override {}
  void undo() override {}
  QString description() const override { return m_desc; }

private:
  QString m_desc;
};

class tst_CommandManager : public QObject {
  Q_OBJECT

private slots:
  void dirtyFlagTracksUndoStack();
  void invalidateCleanForcesDirty();
};

void tst_CommandManager::dirtyFlagTracksUndoStack() {
  CommandManager mgr;
  QVERIFY(mgr.isClean());

  mgr.markClean();
  QVERIFY(mgr.isClean());

  mgr.executeCommand(std::make_unique<StubCommand>(QStringLiteral("A")));
  QVERIFY(!mgr.isClean());
  QCOMPARE(mgr.undoStackSize(), size_t(1));

  mgr.undo();
  QVERIFY(mgr.isClean());

  mgr.redo();
  QVERIFY(!mgr.isClean());

  mgr.markClean();
  QVERIFY(mgr.isClean());

  mgr.executeCommand(std::make_unique<StubCommand>(QStringLiteral("B")));
  QVERIFY(!mgr.isClean());
  mgr.undo();
  QVERIFY(mgr.isClean());
}

void tst_CommandManager::invalidateCleanForcesDirty() {
  CommandManager mgr;
  mgr.markClean();
  QVERIFY(mgr.isClean());
  mgr.invalidateClean();
  QVERIFY(!mgr.isClean());
}

QTEST_MAIN(tst_CommandManager)
#include "tst_CommandManager.moc"
