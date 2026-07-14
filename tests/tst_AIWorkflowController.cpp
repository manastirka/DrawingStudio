#include "AIImageClient.h"
#include "AIWorkflowController.h"

#include <QCoreApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

class FakeAIImageClient : public AIImageClient {
public:
    explicit FakeAIImageClient(QObject *parent = nullptr)
        : AIImageClient(parent)
    {
    }

    void generate(const Request &request) override
    {
        if (request.prompt.trimmed().isEmpty()) {
            emit failed(QStringLiteral("Prompt is empty."));
            return;
        }
        if (isBusy()) {
            emit failed(QStringLiteral("An AI job is already running."));
            return;
        }
        setBusy(true);
        emit started(QStringLiteral("fake-generate"));
        const QString prompt = request.prompt;
        QTimer::singleShot(0, this, [this, prompt]() {
            QImage img(4, 4, QImage::Format_RGB32);
            img.fill(Qt::green);
            finishWithImage(img, prompt);
        });
    }

    void edit(const Request &request) override { generate(request); }
};

class tst_AIWorkflowController : public QObject {
    Q_OBJECT

private slots:
    void cancelWhenIdle_setsStatus();
    void cancelWhileFakeBusy_clearsBusy();
    void fakeFinished_withNoCanvas_resetsJobState();
};

void tst_AIWorkflowController::cancelWhenIdle_setsStatus()
{
    AIWorkflowController ctrl;
    QString status;
    AIWorkflowController::Host host;
    host.setStatusText = [&](const QString &s) { status = s; };
    ctrl.setHost(host);

    ctrl.cancelActiveJob();
    QVERIFY(status.contains(QStringLiteral("No AI job"), Qt::CaseInsensitive));
    QVERIFY(!ctrl.isBusy());
}

void tst_AIWorkflowController::cancelWhileFakeBusy_clearsBusy()
{
    AIWorkflowController ctrl;
    QString status;
    AIWorkflowController::Host host;
    host.setStatusText = [&](const QString &s) { status = s; };
    host.clearStatusMessage = []() {};
    host.showStatusMessage = [](const QString &, int) {};
    ctrl.setHost(host);

    auto *fake = new FakeAIImageClient;
    ctrl.setAIImageClient(fake);
    QCOMPARE(ctrl.aiImageClient(), fake);

    AIImageClient::Request req;
    req.prompt = QStringLiteral("busy job");
    fake->generate(req);
    QVERIFY(ctrl.isBusy());

    ctrl.cancelActiveJob();
    QVERIFY(!ctrl.isBusy());
    QVERIFY(status.contains(QStringLiteral("cancel"), Qt::CaseInsensitive));

    // Drain queued finish callback; should not re-busy the controller.
    QTest::qWait(30);
    QCoreApplication::processEvents();
    QVERIFY(!ctrl.isBusy());
}

void tst_AIWorkflowController::fakeFinished_withNoCanvas_resetsJobState()
{
    // finished handler with job=None just resets; with Generate would call import (needs canvas).
    // Here we only verify client finished leaves isBusy false when job kind is None.
    AIWorkflowController ctrl;
    AIWorkflowController::Host host;
    host.setStatusText = [](const QString &) {};
    host.clearStatusMessage = []() {};
    host.showStatusMessage = [](const QString &, int) {};
    ctrl.setHost(host);

    auto *fake = new FakeAIImageClient;
    ctrl.setAIImageClient(fake);

    QSignalSpy doneSpy(fake, &AIImageClient::finished);
    AIImageClient::Request req;
    req.prompt = QStringLiteral("ok");
    fake->generate(req);
    QTRY_COMPARE_WITH_TIMEOUT(doneSpy.count(), 1, 2000);
    QVERIFY(!fake->isBusy());
    QVERIFY(!ctrl.isBusy());
}

QTEST_MAIN(tst_AIWorkflowController)
#include "tst_AIWorkflowController.moc"
