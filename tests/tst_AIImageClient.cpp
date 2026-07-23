#include "AIImageClient.h"
#include "AISettingsDialog.h"

#include <QCoreApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QImage>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
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

class ScopedSetting {
public:
    explicit ScopedSetting(QString key)
        : m_key(std::move(key))
    {
        QSettings settings;
        m_existed = settings.contains(m_key);
        m_value = settings.value(m_key);
    }

    ~ScopedSetting()
    {
        QSettings settings;
        if (m_existed)
            settings.setValue(m_key, m_value);
        else
            settings.remove(m_key);
        settings.sync();
    }

private:
    QString m_key;
    QVariant m_value;
    bool m_existed = false;
};

class tst_AIImageClient : public QObject {
    Q_OBJECT

private slots:
    void realClient_emptyPrompt_fails();
    void realClient_editWithoutSource_fails();
    void realClient_cancelWhenIdle_isNoOp();
    void fake_generate_emitsFinishedImage();
    void fake_cancel_preventsFinished();
    void testConnection_missingOpenAIKey();
    void testConnection_missingNanoBananaKey();
    void testConnection_unknownProvider();
    void testConnection_invalidRemoteSdUrl();
    void testConnection_refusesRemoteSdRedirect();
    void realClient_remoteSdRejectsRedirect();
    void realClient_remoteSdRejectsOversizedResponse();
    void connectionTestDraftDoesNotPersist();
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

void tst_AIImageClient::testConnection_missingOpenAIKey()
{
    QSettings s;
    s.setValue(QStringLiteral("AI/openaiApiKey"), QString());
    s.setValue(QStringLiteral("AI/apiKey"), QString());
    s.sync();

    AIImageClient client;
    QSignalSpy spy(&client, &AIImageClient::connectionTestFinished);
    QVERIFY(spy.isValid());

    client.testConnection(QStringLiteral("openai"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
    QVERIFY(spy.at(0).at(1).toString().contains(QStringLiteral("Missing"), Qt::CaseInsensitive));
}

void tst_AIImageClient::testConnection_missingNanoBananaKey()
{
    QSettings s;
    s.setValue(QStringLiteral("AI/nanoBananaApiKey"), QString());
    s.setValue(QStringLiteral("AI/googleApiKey"), QString());
    s.sync();

    AIImageClient client;
    QSignalSpy spy(&client, &AIImageClient::connectionTestFinished);

    client.testConnection(QStringLiteral("nanobanana"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
    QVERIFY(spy.at(0).at(1).toString().contains(QStringLiteral("Missing"), Qt::CaseInsensitive));
}

void tst_AIImageClient::testConnection_unknownProvider()
{
    AIImageClient client;
    QSignalSpy spy(&client, &AIImageClient::connectionTestFinished);

    client.testConnection(QStringLiteral("not-a-provider"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
    QVERIFY(spy.at(0).at(1).toString().contains(QStringLiteral("Unknown"), Qt::CaseInsensitive));
}

void tst_AIImageClient::testConnection_invalidRemoteSdUrl()
{
    AIImageClient client;
    QSignalSpy spy(&client, &AIImageClient::connectionTestFinished);
    AIImageClient::ConnectionTestConfig config;
    config.remoteSdUrl = QStringLiteral("file:///tmp/not-a-server");

    client.testConnection(QStringLiteral("remotesd"), config);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
    QVERIFY(spy.at(0).at(1).toString().contains(QStringLiteral("HTTP")));
    QCOMPARE(AIImageClient::defaultRemoteSdUrl(),
             QStringLiteral("http://127.0.0.1:8000"));
}

void tst_AIImageClient::testConnection_refusesRemoteSdRedirect()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *socket = server.nextPendingConnection();
        QVERIFY(socket);
        connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
            socket->readAll();
            socket->write(
                "HTTP/1.1 302 Found\r\n"
                "Location: http://127.0.0.1:9/redirected\r\n"
                "Content-Length: 0\r\nConnection: close\r\n\r\n");
            socket->disconnectFromHost();
        });
    });

    AIImageClient client;
    QSignalSpy spy(&client, &AIImageClient::connectionTestFinished);
    AIImageClient::ConnectionTestConfig config;
    config.remoteSdUrl =
        QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());

    client.testConnection(QStringLiteral("remotesd"), config);

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
    QVERIFY(spy.at(0).at(1).toString().contains(QStringLiteral("Redirect")));
}

void tst_AIImageClient::realClient_remoteSdRejectsRedirect()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *socket = server.nextPendingConnection();
        QVERIFY(socket);
        connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
            socket->readAll();
            socket->write(
                "HTTP/1.1 307 Temporary Redirect\r\n"
                "Location: http://127.0.0.1:9/capture-prompt\r\n"
                "Content-Length: 0\r\nConnection: close\r\n\r\n");
            socket->disconnectFromHost();
        });
    });

    ScopedSetting remoteUrlGuard(QStringLiteral("AI/remoteSDUrl"));
    QSettings settings;
    settings.setValue(
        QStringLiteral("AI/remoteSDUrl"),
        QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
    settings.sync();

    AIImageClient client;
    QSignalSpy failSpy(&client, &AIImageClient::failed);
    QSignalSpy doneSpy(&client, &AIImageClient::finished);
    AIImageClient::Request request;
    request.provider = QStringLiteral("remotesd");
    request.prompt = QStringLiteral("private prompt must not be redirected");
    client.generate(request);

    QTRY_COMPARE_WITH_TIMEOUT(failSpy.count(), 1, 2000);
    QCOMPARE(doneSpy.count(), 0);
    QVERIFY(failSpy.at(0).at(0).toString().contains(QStringLiteral("Redirect")));
    QVERIFY(!client.isBusy());
}

void tst_AIImageClient::realClient_remoteSdRejectsOversizedResponse()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *socket = server.nextPendingConnection();
        QVERIFY(socket);
        connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
            socket->readAll();
            const qint64 declaredSize =
                AIImageClient::kMaxNetworkResponseBytes + 1;
            socket->write(
                QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n")
                + QByteArrayLiteral("Content-Length: ")
                + QByteArray::number(declaredSize)
                + QByteArrayLiteral("\r\nConnection: keep-alive\r\n\r\n"));
            socket->flush();
        });
    });

    ScopedSetting remoteUrlGuard(QStringLiteral("AI/remoteSDUrl"));
    QSettings settings;
    settings.setValue(
        QStringLiteral("AI/remoteSDUrl"),
        QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
    settings.sync();

    AIImageClient client;
    QSignalSpy failSpy(&client, &AIImageClient::failed);
    AIImageClient::Request request;
    request.provider = QStringLiteral("remotesd");
    request.prompt = QStringLiteral("oversized response test");
    client.generate(request);

    QTRY_COMPARE_WITH_TIMEOUT(failSpy.count(), 1, 2000);
    QVERIFY(failSpy.at(0).at(0).toString().contains(QStringLiteral("exceeds")));
    QVERIFY(!client.isBusy());
}

void tst_AIImageClient::connectionTestDraftDoesNotPersist()
{
    QSettings settings;
    const QString providerKey = QStringLiteral("AI/provider");
    const QString apiKey = QStringLiteral("AI/openaiApiKey");
    const bool hadProvider = settings.contains(providerKey);
    const bool hadApiKey = settings.contains(apiKey);
    const QVariant previousProvider = settings.value(providerKey);
    const QVariant previousApiKey = settings.value(apiKey);

    settings.setValue(providerKey, QStringLiteral("openai"));
    settings.setValue(apiKey, QStringLiteral("persisted-test-key"));
    settings.sync();

    {
        AISettingsDialog dialog;
        auto *keyEdit = dialog.findChild<QLineEdit *>(
            QStringLiteral("openaiApiKeyEdit"));
        auto *testButton = dialog.findChild<QPushButton *>(
            QStringLiteral("testAiConnectionButton"));
        QVERIFY(keyEdit);
        QVERIFY(testButton);

        keyEdit->clear();
        QTest::mouseClick(testButton, Qt::LeftButton);

        QCOMPARE(settings.value(apiKey).toString(),
                 QStringLiteral("persisted-test-key"));
        QVERIFY(testButton->isEnabled());
    }

    if (hadProvider)
        settings.setValue(providerKey, previousProvider);
    else
        settings.remove(providerKey);
    if (hadApiKey)
        settings.setValue(apiKey, previousApiKey);
    else
        settings.remove(apiKey);
    settings.sync();
}

QTEST_MAIN(tst_AIImageClient)
#include "tst_AIImageClient.moc"
