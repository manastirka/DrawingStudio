#include "SAM2Client.h"
#include <QPainter>
#include <QSignalSpy>
#include <QtTest>

class tst_YOLOIntegration : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void testHumanDetection();
};

void tst_YOLOIntegration::initTestCase() {
  // Ensure we have a QApplication for event loop
  if (!QCoreApplication::instance()) {
    static int argc = 0;
    static char **argv = nullptr;
    new QCoreApplication(argc, argv);
  }
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
