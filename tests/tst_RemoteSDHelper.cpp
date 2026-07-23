#include <QtTest>

#include "RemoteSDHelper.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQueue>
#include <QTcpServer>
#include <QTcpSocket>

namespace {
class TestHttpServer final : public QObject
{
public:
    TestHttpServer()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket,
                        [this, socket]() {
                    QByteArray request =
                        socket->property("requestBytes").toByteArray();
                    request += socket->readAll();
                    socket->setProperty("requestBytes", request);

                    const qsizetype headerEnd = request.indexOf("\r\n\r\n");
                    if (headerEnd < 0) {
                        return;
                    }

                    qint64 contentLength = 0;
                    const QList<QByteArray> headerLines =
                        request.left(headerEnd).split('\n');
                    for (QByteArray line : headerLines) {
                        line = line.trimmed();
                        if (line.toLower().startsWith("content-length:")) {
                            contentLength =
                                line.mid(sizeof("content-length:") - 1)
                                    .trimmed()
                                    .toLongLong();
                        }
                    }
                    const qint64 completeSize =
                        headerEnd + 4 + contentLength;
                    if (request.size() < completeSize
                        || socket->property("responded").toBool()) {
                        return;
                    }

                    socket->setProperty("responded", true);
                    m_requests.append(request.left(completeSize));
                    const QByteArray body =
                        m_responses.isEmpty() ? QByteArrayLiteral("{}")
                                              : m_responses.dequeue();
                    QByteArray response =
                        QByteArrayLiteral("HTTP/1.1 200 OK\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Connection: close\r\n"
                                          "Content-Length: ");
                    response += QByteArray::number(body.size());
                    response += QByteArrayLiteral("\r\n\r\n");
                    response += body;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket,
                        &QObject::deleteLater);
            }
        });
        const bool listening =
            m_server.listen(QHostAddress::LocalHost, 0);
        Q_ASSERT(listening);
    }

    void enqueue(const QByteArray& body) { m_responses.enqueue(body); }

    QString url() const
    {
        return QStringLiteral("http://127.0.0.1:%1")
            .arg(m_server.serverPort());
    }

    const QList<QByteArray>& requests() const { return m_requests; }

private:
    QTcpServer m_server;
    QQueue<QByteArray> m_responses;
    QList<QByteArray> m_requests;
};

QByteArray tinyPngBase64()
{
    QImage image(2, 2, QImage::Format_ARGB32);
    image.fill(QColor(20, 80, 140, 255));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        return {};
    }
    return bytes.toBase64();
}
} // namespace

class RemoteSDHelperTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsUnsafeServerUrls()
    {
        RemoteSDHelper helper;
        QSignalSpy errors(&helper, &RemoteSDHelper::errorOccurred);

        QVERIFY(!helper.initialize(QStringLiteral("ftp://example.com")));
        QVERIFY(!helper.initialize(
            QStringLiteral("https://user:password@example.com")));
        QVERIFY(!helper.isInitialized());
        QCOMPARE(errors.count(), 2);
    }

    void initializesFromBoundedJsonResponse()
    {
        TestHttpServer server;
        server.enqueue(QByteArrayLiteral(
            R"({"status":"ok","model":"test-model.safetensors"})"));

        RemoteSDHelper helper;
        QSignalSpy models(&helper, &RemoteSDHelper::modelsListReceived);

        QVERIFY(helper.initialize(server.url() + QStringLiteral("/")));
        QVERIFY(helper.isInitialized());
        QCOMPARE(models.count(), 1);
        QCOMPARE(models.first().first().toStringList(),
                 QStringList{QStringLiteral("test-model.safetensors")});
        QCOMPARE(server.requests().size(), 1);
        QVERIFY(server.requests().first().startsWith("GET / HTTP/1.1"));
    }

    void rejectsOversizedMetadataResponse()
    {
        TestHttpServer server;
        server.enqueue(QByteArray(
            RemoteSDHelper::kMaxMetadataResponseBytes + 1, 'x'));

        RemoteSDHelper helper;
        QSignalSpy errors(&helper, &RemoteSDHelper::errorOccurred);

        QVERIFY(!helper.initialize(server.url()));
        QVERIFY(!helper.isInitialized());
        QCOMPARE(errors.count(), 1);
        QVERIFY(errors.first().first().toString().contains(
            QStringLiteral("size limit"), Qt::CaseInsensitive));
    }

    void generatesAsynchronouslyOnTheOwnerThread()
    {
        TestHttpServer server;
        server.enqueue(QByteArrayLiteral(R"({"status":"ok"})"));
        const QByteArray generationResponse =
            QByteArrayLiteral(R"({"image":")") + tinyPngBase64()
            + QByteArrayLiteral(R"("})");
        server.enqueue(generationResponse);

        RemoteSDHelper helper;
        QVERIFY(helper.initialize(server.url()));
        QSignalSpy started(&helper, &RemoteSDHelper::generationStarted);
        QSignalSpy generated(&helper, &RemoteSDHelper::imageGenerated);
        QSignalSpy errors(&helper, &RemoteSDHelper::errorOccurred);

        const QString prompt = QStringLiteral("A safe async request");
        helper.generateImageAsync(prompt, QStringLiteral("blur"), 64, 64, 12,
                                  5.5f, 42);

        QTRY_COMPARE_WITH_TIMEOUT(generated.count(), 1, 3000);
        QCOMPARE(started.count(), 1);
        QCOMPARE(errors.count(), 0);
        const QList<QVariant> arguments = generated.takeFirst();
        const QImage image = qvariant_cast<QImage>(arguments.at(0));
        QCOMPARE(image.size(), QSize(2, 2));
        QCOMPARE(arguments.at(1).toString(), prompt);

        QCOMPARE(server.requests().size(), 2);
        const QByteArray request = server.requests().last();
        QVERIFY(request.startsWith("POST /generate HTTP/1.1"));
        const qsizetype bodyStart = request.indexOf("\r\n\r\n");
        QVERIFY(bodyStart >= 0);
        const QJsonDocument body =
            QJsonDocument::fromJson(request.mid(bodyStart + 4));
        QVERIFY(body.isObject());
        QCOMPARE(body.object().value(QStringLiteral("prompt")).toString(),
                 prompt);
        QCOMPARE(body.object().value(QStringLiteral("seed")).toInt(), 42);
    }

    void rejectsMalformedGeneratedImage()
    {
        TestHttpServer server;
        server.enqueue(QByteArrayLiteral(R"({"status":"ok"})"));
        server.enqueue(QByteArrayLiteral(R"({"images":["not!base64"]})"));

        RemoteSDHelper helper;
        QVERIFY(helper.initialize(server.url()));
        QSignalSpy generated(&helper, &RemoteSDHelper::imageGenerated);
        QSignalSpy errors(&helper, &RemoteSDHelper::errorOccurred);

        helper.generateImageAsync(QStringLiteral("bad response"));

        QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 3000);
        QCOMPARE(generated.count(), 0);
        QVERIFY(errors.first().first().toString().contains(
            QStringLiteral("decode"), Qt::CaseInsensitive));
    }
};

QTEST_MAIN(RemoteSDHelperTest)
#include "tst_RemoteSDHelper.moc"
