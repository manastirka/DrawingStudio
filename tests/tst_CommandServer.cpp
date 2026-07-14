#include "CommandServer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QtTest>

#include <memory>

class tst_CommandServer : public QObject {
    Q_OBJECT

private slots:
    void startStopOnEphemeralPort();
    void statusWithoutCanvas();
    void commandMissingActionReturns400();
    void commandInvalidJsonReturns400();
    void commandEmitsSignalAndUsesResultProvider();
    void unknownPathReturns404();
    void undoRedoEmitCommands();
};

// Same-thread QTcpServer needs processEvents while the client connects.
static bool connectLocal(QTcpSocket &sock, quint16 port, int timeoutMs = 3000)
{
    sock.connectToHost(QHostAddress::LocalHost, port);
    QElapsedTimer t;
    t.start();
    while (sock.state() != QAbstractSocket::ConnectedState && t.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (sock.state() == QAbstractSocket::UnconnectedState
            && sock.error() != QAbstractSocket::SocketTimeoutError
            && sock.error() != QAbstractSocket::UnknownSocketError) {
            // keep trying until timeout for connection-in-progress
        }
    }
    return sock.state() == QAbstractSocket::ConnectedState;
}

static QByteArray httpExchange(const QString &method, const QString &path,
                               const QByteArray &body, quint16 port)
{
    QTcpSocket sock;
    if (!connectLocal(sock, port))
        return {};

    QByteArray req;
    req += method.toUtf8() + " " + path.toUtf8() + " HTTP/1.1\r\n";
    req += "Host: 127.0.0.1\r\n";
    req += "Connection: close\r\n";
    if (!body.isEmpty()) {
        req += "Content-Type: application/json\r\n";
        req += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    }
    req += "\r\n";
    req += body;
    sock.write(req);
    sock.flush();

    QByteArray resp;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 3000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (sock.bytesAvailable() > 0)
            resp += sock.readAll();
        if (sock.state() == QAbstractSocket::UnconnectedState)
            break;
    }
    if (sock.bytesAvailable() > 0)
        resp += sock.readAll();
    return resp;
}

static QJsonObject httpJson(const QString &method, const QString &path,
                            const QByteArray &body, quint16 port)
{
    const QByteArray resp = httpExchange(method, path, body, port);
    const int sep = resp.indexOf("\r\n\r\n");
    if (sep < 0)
        return {};
    return QJsonDocument::fromJson(resp.mid(sep + 4)).object();
}

static int httpStatus(const QString &method, const QString &path,
                      const QByteArray &body, quint16 port)
{
    const QByteArray resp = httpExchange(method, path, body, port);
    const QByteArray line = resp.split('\n').value(0).trimmed();
    const QList<QByteArray> parts = line.split(' ');
    if (parts.size() < 2)
        return -1;
    return parts[1].toInt();
}

void tst_CommandServer::startStopOnEphemeralPort()
{
    CommandServer server;
    QVERIFY(server.start(0)); // OS-assigned port
    QVERIFY(server.isRunning());
    server.stop();
    QVERIFY(!server.isRunning());
}

void tst_CommandServer::statusWithoutCanvas()
{
    CommandServer server;
    QVERIFY(server.start(0));
    // QTcpServer doesn't expose port easily after start(0) if we don't store it —
    // CommandServer::start uses listen; re-read via server internals is private.
    // Use fixed high port in ephemeral range instead.
    server.stop();

    const quint16 port = 19191;
    QVERIFY(server.start(port));

    const QJsonObject json = httpJson(QStringLiteral("GET"), QStringLiteral("/api/status"),
                                      {}, port);
    QCOMPARE(json.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    server.stop();
}

void tst_CommandServer::commandMissingActionReturns400()
{
    CommandServer server;
    const quint16 port = 19192;
    QVERIFY(server.start(port));

    const int code = httpStatus(QStringLiteral("POST"), QStringLiteral("/api/command"),
                                QByteArrayLiteral("{}"), port);
    QCOMPARE(code, 400);
    server.stop();
}

void tst_CommandServer::commandInvalidJsonReturns400()
{
    CommandServer server;
    const quint16 port = 19193;
    QVERIFY(server.start(port));

    const int code = httpStatus(QStringLiteral("POST"), QStringLiteral("/api/command"),
                                QByteArrayLiteral("not-json"), port);
    QCOMPARE(code, 400);
    server.stop();
}

void tst_CommandServer::commandEmitsSignalAndUsesResultProvider()
{
    CommandServer server;
    const quint16 port = 19194;
    QVERIFY(server.start(port));

    QJsonObject last;
    last.insert(QStringLiteral("success"), false);
    last.insert(QStringLiteral("error"), QStringLiteral("Unknown action: nope"));
    server.setCommandResultProvider([last]() { return last; });

    QSignalSpy spy(&server, &CommandServer::commandReceived);
    QVERIFY(spy.isValid());

    const QByteArray body =
        QByteArrayLiteral(R"({"action":"nope","params":{}})");
    const QJsonObject json = httpJson(QStringLiteral("POST"), QStringLiteral("/api/command"),
                                      body, port);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("nope"));
    QCOMPARE(json.value(QStringLiteral("status")).toString(), QStringLiteral("error"));
    QCOMPARE(json.value(QStringLiteral("message")).toString(),
             QStringLiteral("Unknown action: nope"));
    server.stop();
}

void tst_CommandServer::unknownPathReturns404()
{
    CommandServer server;
    const quint16 port = 19195;
    QVERIFY(server.start(port));

    const int code = httpStatus(QStringLiteral("GET"), QStringLiteral("/api/does-not-exist"),
                                {}, port);
    QCOMPARE(code, 404);
    server.stop();
}

void tst_CommandServer::undoRedoEmitCommands()
{
    CommandServer server;
    const quint16 port = 19196;
    QVERIFY(server.start(port));

    QSignalSpy spy(&server, &CommandServer::commandReceived);
    QVERIFY(spy.isValid());

    QCOMPARE(httpStatus(QStringLiteral("POST"), QStringLiteral("/api/undo"), {}, port), 200);
    QCOMPARE(httpStatus(QStringLiteral("POST"), QStringLiteral("/api/redo"), {}, port), 200);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("undo"));
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("redo"));
    server.stop();
}

QTEST_MAIN(tst_CommandServer)
#include "tst_CommandServer.moc"
