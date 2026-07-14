#include "CommandManager.h"
#include "Commands.h"
#include "DrawingCanvas.h"
#include "DrawingCommandDispatcher.h"
#include "DrawingPrimitive.h"
#include "LayerManager.h"

#include <QJsonObject>
#include <QtTest>

#include <memory>

/**
 * Headless-ish dispatcher tests (QT_QPA_PLATFORM=offscreen recommended).
 * Canvas emits commandRequested; we execute via CommandManager like MainWindow.
 */
class tst_DrawingCommandDispatcher : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void drawLineAddsPrimitive();
    void unknownActionSetsErrorResult();
    void undoHostCallbackInvoked();

private:
    void wireCommandExecution();

    DrawingCanvas *m_canvas = nullptr;
    LayerManager *m_layers = nullptr;
    CommandManager *m_cmds = nullptr;
    DrawingCommandDispatcher *m_dispatcher = nullptr;
    int m_undoCalls = 0;
};

void tst_DrawingCommandDispatcher::wireCommandExecution()
{
    connect(m_canvas, &DrawingCanvas::commandRequested, this,
            [this](Command *raw) {
                std::unique_ptr<Command> cmd(raw);
                if (m_cmds)
                    m_cmds->executeCommand(std::move(cmd));
                else if (cmd)
                    cmd->execute();
            });
}

void tst_DrawingCommandDispatcher::init()
{
    m_undoCalls = 0;
    m_layers = new LayerManager;
    m_cmds = new CommandManager;
    m_canvas = new DrawingCanvas;
    m_canvas->setLayerManager(m_layers);
    wireCommandExecution();

    m_dispatcher = new DrawingCommandDispatcher;
    DrawingCommandDispatcher::Host host;
    host.canvas = m_canvas;
    host.layerManager = m_layers;
    host.undo = [this]() { ++m_undoCalls; };
    host.redo = []() {};
    m_dispatcher->setHost(std::move(host));
}

void tst_DrawingCommandDispatcher::cleanup()
{
    delete m_dispatcher;
    m_dispatcher = nullptr;
    delete m_canvas;
    m_canvas = nullptr;
    delete m_cmds;
    m_cmds = nullptr;
    delete m_layers;
    m_layers = nullptr;
}

void tst_DrawingCommandDispatcher::drawLineAddsPrimitive()
{
    const int before = static_cast<int>(m_layers->getAllPrimitives().size());

    QJsonObject params;
    params.insert(QStringLiteral("x1"), 0.0);
    params.insert(QStringLiteral("y1"), 0.0);
    params.insert(QStringLiteral("x2"), 100.0);
    params.insert(QStringLiteral("y2"), 50.0);
    params.insert(QStringLiteral("color"), QStringLiteral("#ff0000"));
    params.insert(QStringLiteral("lineWidth"), 2.0);

    m_dispatcher->execute(QStringLiteral("draw_line"), params);

    const auto prims = m_layers->getAllPrimitives();
    QCOMPARE(static_cast<int>(prims.size()), before + 1);
    QVERIFY(dynamic_cast<LinePrimitive *>(prims.back()) != nullptr);
    QVERIFY(m_cmds->undoStackSize() >= 1);
}

void tst_DrawingCommandDispatcher::unknownActionSetsErrorResult()
{
    m_dispatcher->clearLastResult();
    m_dispatcher->execute(QStringLiteral("not_a_real_action"), {});

    const QJsonObject r = m_dispatcher->lastResult();
    QVERIFY(r.contains(QStringLiteral("success")));
    QCOMPARE(r.value(QStringLiteral("success")).toBool(), false);
    QVERIFY(r.value(QStringLiteral("error")).toString().contains(
        QStringLiteral("Unknown action")));
}

void tst_DrawingCommandDispatcher::undoHostCallbackInvoked()
{
    m_dispatcher->execute(QStringLiteral("undo"), {});
    QCOMPARE(m_undoCalls, 1);
}

QTEST_MAIN(tst_DrawingCommandDispatcher)
#include "tst_DrawingCommandDispatcher.moc"
