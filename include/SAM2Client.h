#pragma once

#include <QImage>
#include <QMetaType>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <vector>

class SAM2Client : public QObject {
  Q_OBJECT

public:
  explicit SAM2Client(QObject *parent = nullptr);

  static constexpr qsizetype kMinAuthTokenBytes = 16;

  // Shared by all clients in this process. SAM2ServiceManager sets this before
  // launching or probing the local helper service.
  static void setDefaultAuthToken(const QByteArray &token);

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

  // Set a loopback HTTP service URL (default: http://127.0.0.1:5001).
  // Non-loopback URLs are rejected so the bearer token cannot be exfiltrated.
  void setServiceUrl(const QString &url);
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
  static void authorizeRequest(QNetworkRequest &request);
  static bool isAuthenticatedProtocol(const QNetworkReply *reply);
  void handleSegmentationResponse(QNetworkReply *reply);
  void handleAllObjectsResponse(QNetworkReply *reply);
};

Q_DECLARE_METATYPE(SAM2Client::SegmentationResult)
Q_DECLARE_METATYPE(SAM2Client::MultiSegmentationResult)
