#include "SAM2Client.h"
#include <QElapsedTimer>
#include <QPainter>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class tst_YOLOIntegration : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void testAuthenticatedHealthRequest();
  void testRejectsLegacyHealthResponse();
  void testRejectsNonLoopbackServiceUrl();
  void testHumanDetection();
};

namespace {
const QByteArray kTestToken("drawingstudio-sam2-test-token-32-bytes");

QByteArray receiveRequest(QTcpServer &server, QTcpSocket *&socket) {
  QElapsedTimer elapsed;
  elapsed.start();
  while (!server.hasPendingConnections() && elapsed.elapsed() < 2000)
    QTest::qWait(10);
  socket = server.nextPendingConnection();
  if (!socket)
    return {};
  elapsed.restart();
  while (socket->bytesAvailable() == 0 && elapsed.elapsed() < 2000)
    QTest::qWait(10);
  return socket->readAll();
}

void sendHealthResponse(QTcpSocket *socket, bool authenticatedProtocol) {
  const QByteArray body(
      R"({"status":"ok","sam2_loaded":true,"device":"test","auth_required":true,"protocol_version":"2"})");
  QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n";
  if (authenticatedProtocol)
    response += "X-DrawingStudio-SAM2-Protocol: 2\r\n";
  response += "Content-Length: " + QByteArray::number(body.size())
      + "\r\nConnection: close\r\n\r\n" + body;
  socket->write(response);
  socket->disconnectFromHost();
}
} // namespace

void tst_YOLOIntegration::initTestCase() {
  // Ensure we have a QApplication for event loop
  if (!QCoreApplication::instance()) {
    static int argc = 0;
    static char **argv = nullptr;
    new QCoreApplication(argc, argv);
  }
  SAM2Client::setDefaultAuthToken(kTestToken);
}

void tst_YOLOIntegration::testAuthenticatedHealthRequest() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));

  SAM2Client client;
  client.setServiceUrl(
      QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
  QSignalSpy healthSpy(&client, &SAM2Client::healthCheckComplete);
  client.checkHealth();

  QTcpSocket *socket = nullptr;
  const QByteArray request = receiveRequest(server, socket);
  QVERIFY(socket);
  QVERIFY(!request.isEmpty());
  QVERIFY(request.contains("Authorization: Bearer " + kTestToken));
  sendHealthResponse(socket, true);

  QTRY_COMPARE_WITH_TIMEOUT(healthSpy.count(), 1, 2000);
  QCOMPARE(healthSpy.at(0).at(0).toBool(), true);
  QCOMPARE(healthSpy.at(0).at(1).toString(), QStringLiteral("test"));
}

void tst_YOLOIntegration::testRejectsLegacyHealthResponse() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));

  SAM2Client client;
  client.setServiceUrl(
      QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
  QSignalSpy healthSpy(&client, &SAM2Client::healthCheckComplete);
  client.checkHealth();

  QTcpSocket *socket = nullptr;
  const QByteArray request = receiveRequest(server, socket);
  QVERIFY(socket);
  QVERIFY(!request.isEmpty());
  sendHealthResponse(socket, false);

  QTRY_COMPARE_WITH_TIMEOUT(healthSpy.count(), 1, 2000);
  QCOMPARE(healthSpy.at(0).at(0).toBool(), false);
  QVERIFY(healthSpy.at(0).at(1).toString().contains("authenticated protocol"));
}

void tst_YOLOIntegration::testRejectsNonLoopbackServiceUrl() {
  SAM2Client client;
  const QString original = client.serviceUrl();
  client.setServiceUrl(QStringLiteral("http://example.com/sam2"));
  QCOMPARE(client.serviceUrl(), original);
}

void tst_YOLOIntegration::testHumanDetection() {
  SAM2Client client;

  // Skip if the local SAM2 service isn't reachable/ready.
  QSignalSpy healthSpy(&client, &SAM2Client::healthCheckComplete);
  client.checkHealth();
  if (!healthSpy.wait(2000)) {
    QSKIP("SAM2 service not reachable (health check timeout)");
  }
  {
    const QList<QVariant> args = healthSpy.takeFirst();
    const bool available = args.at(0).toBool();
    const QString deviceOrError = args.at(1).toString();
    if (!available) {
      QSKIP(qPrintable(QString("SAM2 service not available: %1").arg(deviceOrError)));
    }
  }

  // Create a dummy image (black with a white square)
  QImage image(640, 480, QImage::Format_RGB32);
  image.fill(Qt::black);
  QPainter painter(&image);
  painter.fillRect(100, 100, 50, 100, Qt::white); // "Human" shape
  painter.end();

  qDebug() << "Starting YOLO detection test...";

  QSignalSpy spy(&client, &SAM2Client::multiSegmentationComplete);
  QSignalSpy spyError(&client, &SAM2Client::segmentationFailed);

  client.segmentHumans(image);

  // Wait for either success or failure (timeout 10s). This is an integration
  // test for request/response wiring; accuracy depends on the installed model.
  qDebug() << "Waiting for response...";
  QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0 || spyError.count() > 0, 10000);

  if (spyError.count() > 0) {
    QList<QVariant> args = spyError.takeFirst();
    qDebug() << "Segmentation failed:" << args.at(0).toString();
  } else {
    qDebug() << "Received response!";
  }
}

QTEST_MAIN(tst_YOLOIntegration)
#include "tst_YOLOIntegration.moc"
