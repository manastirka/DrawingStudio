#pragma once

#include <QImage>
#include <QMetaType>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <vector>

class SAM2Client : public QObject {
  Q_OBJECT

public:
  explicit SAM2Client(QObject *parent = nullptr);

  struct SegmentationResult {
    QImage mask; // Binary mask (white = subject, black = background)
    std::vector<QPointF> contour; // Outline points
    float confidence;             // Segmentation confidence (0-1)
    bool success;
  };

  struct MaskCandidate {
    int id;
    std::vector<QPointF> contour;
    QImage mask; // Full-resolution binary mask (white = subject)
    float score;
    float stability;
    float predicted_iou;
    float area_percent;
  };

  struct MultiSegmentationResult {
    std::vector<MaskCandidate> candidates;
    int total_found;
    bool success;
  };

  // Check if service is available
  void checkHealth();

  // Automatic segmentation (finds main subject)
  void segmentImage(const QImage &image);

  // Human detection using YOLO + SAM2
  void segmentHumans(const QImage &image);

  // Point-based segmentation (user clicks on subject)
  void segmentWithPoint(const QImage &image, const QPointF &point);

  // Box-based segmentation (user draws box around subject)
  void segmentWithBox(const QImage &image, const QRectF &box);

  // Set service URL (default: http://localhost:5001)
  void setServiceUrl(const QString &url) { m_serviceUrl = url; }
  QString serviceUrl() const { return m_serviceUrl; }

  // Access methods for internal use
  QNetworkAccessManager *getNetworkManager() { return m_networkManager; }

signals:
  void segmentationComplete(const SegmentationResult &result);
  void multiSegmentationComplete(const MultiSegmentationResult &result);
  void segmentationFailed(const QString &error);
  void healthCheckComplete(bool available, const QString &device);
  void segmentationProgress(int percentage, const QString &message);

private:
  QNetworkAccessManager *m_networkManager;
  QString m_serviceUrl;

  QByteArray imageToBase64(const QImage &image);
  QImage base64ToMask(const QString &base64, int width, int height);
  void handleSegmentationResponse(QNetworkReply *reply);
  void handleAllObjectsResponse(QNetworkReply *reply);
};

Q_DECLARE_METATYPE(SAM2Client::SegmentationResult)
Q_DECLARE_METATYPE(SAM2Client::MultiSegmentationResult)
