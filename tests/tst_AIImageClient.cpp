#include "AIImageClient.h"

#include <QCoreApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

/**
 * Fake client: no network — completes generate/edit on the next event-loop tick.
 */
class FakeAIImageClient : public AIImageClient {
public:
    explicit FakeAIImageClient(QObject *parent = nullptr)
        : AIImageClient(parent)
    {
    }

    void generate(const Request &request) override
    {
        if (request.prompt.trimmed().isEmpty()) {
            setBusy(true);
            failWith(QStringLiteral("Prompt is empty."));
            return;
        }
        if (isBusy()) {
            failWith(QStringLiteral("An AI job is already running."));
            return;
        }
        setBusy(true);
        emit started(QStringLiteral("fake-generate"));
        const QString prompt = request.prompt;
        QTimer::singleShot(0, this, [this, prompt]() {
            QImage img(8, 8, QImage::Format_RGB32);
            img.fill(QColor(30, 144, 255));
            finishWithImage(img, prompt);
        });
    }

    void edit(const Request &request) override
    {
        if (request.sourceImage.isNull()) {
            setBusy(true);
            failWith(QStringLiteral("No source/reference image for AI edit."));
            return;
        }
        generate(request);
    }
};

class tst_AIImageClient : public QObject {
    Q_OBJECT

private slots:
    void realClient_emptyPrompt_fails();
    void realClient_editWithoutSource_fails();
    void realClient_cancelWhenIdle_isNoOp();
    void fake_generate_emitsFinishedImage();
    void fake_cancel_preventsFinished();
};

void tst_AIImageClient::realClient_emptyPrompt_fails()
{
    AIImageClient client;
    QSignalSpy failSpy(&client, &AIImageClient::failed);
    QSignalSpy doneSpy(&client, &AIImageClient::finished);
    QVERIFY(failSpy.isValid());

    AIImageClient::Request req;
    req.prompt = QStringLiteral("   ");
    client.generate(req);

    QCOMPARE(failSpy.count(), 1);
    QCOMPARE(doneSpy.count(), 0);
    QVERIFY(failSpy.at(0).at(0).toString().contains(QStringLiteral("empty"), Qt::CaseInsensitive));
    QVERIFY(!client.isBusy());
}

void tst_AIImageClient::realClient_editWithoutSource_fails()
{
    AIImageClient client;
    QSignalSpy failSpy(&client, &AIImageClient::failed);

    AIImageClient::Request req;
    req.prompt = QStringLiteral("make it blue");
    client.edit(req);

    QCOMPARE(failSpy.count(), 1);
    QVERIFY(failSpy.at(0).at(0).toString().contains(QStringLiteral("source"), Qt::CaseInsensitive));
}

void tst_AIImageClient::realClient_cancelWhenIdle_isNoOp()
{
    AIImageClient client;
    QSignalSpy failSpy(&client, &AIImageClient::failed);
    client.cancel();
    QCOMPARE(failSpy.count(), 0);
    QVERIFY(!client.isBusy());
}

void tst_AIImageClient::fake_generate_emitsFinishedImage()
{
    FakeAIImageClient client;
    QSignalSpy startSpy(&client, &AIImageClient::started);
    QSignalSpy doneSpy(&client, &AIImageClient::finished);
    QSignalSpy failSpy(&client, &AIImageClient::failed);

    AIImageClient::Request req;
    req.prompt = QStringLiteral("solid blue");
    client.generate(req);
    QVERIFY(client.isBusy());

    QTRY_COMPARE_WITH_TIMEOUT(doneSpy.count(), 1, 2000);
    QCOMPARE(failSpy.count(), 0);
    QCOMPARE(startSpy.count(), 1);
    QVERIFY(!client.isBusy());

    const QImage img = doneSpy.at(0).at(0).value<QImage>();
    QCOMPARE(img.width(), 8);
    QCOMPARE(img.height(), 8);
    QCOMPARE(doneSpy.at(0).at(1).toString(), QStringLiteral("solid blue"));
}

void tst_AIImageClient::fake_cancel_preventsFinished()
{
    FakeAIImageClient client;
    QSignalSpy doneSpy(&client, &AIImageClient::finished);
    QSignalSpy failSpy(&client, &AIImageClient::failed);

    AIImageClient::Request req;
    req.prompt = QStringLiteral("will cancel");
    client.generate(req);
    QVERIFY(client.isBusy());
    client.cancel();

    // Allow the queued singleShot to run; finishWithImage should no-op when idle.
    QTest::qWait(50);
    QCoreApplication::processEvents();

    QCOMPARE(doneSpy.count(), 0);
    // cancel emits failed("AI job cancelled.")
    QVERIFY(failSpy.count() >= 1);
    QVERIFY(!client.isBusy());
}

QTEST_MAIN(tst_AIImageClient)
#include "tst_AIImageClient.moc"
