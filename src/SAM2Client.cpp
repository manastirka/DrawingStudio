#include "SAM2Client.h"
#include <QBuffer>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QHostAddress>
#include <QTimer>

namespace {
constexpr int kHealthTimeoutMs = 2000;
constexpr int kSegmentationTimeoutMs = 60000;
constexpr int kProgressTimeoutMs = 1500;
constexpr const char *kProtocolHeader = "X-DrawingStudio-SAM2-Protocol";
constexpr const char *kProtocolVersion = "2";

QByteArray g_authToken;

constexpr const char *kTimedOutProperty = "sam2_timed_out";
constexpr const char *kTimeoutContextProperty = "sam2_timeout_context";

void attachTimeout(QNetworkReply *reply, int timeoutMs,
                   const QString &context) {
  if (!reply || timeoutMs <= 0) {
    return;
  }

  auto *timer = new QTimer(reply);
  timer->setSingleShot(true);
  timer->setInterval(timeoutMs);

  QObject::connect(timer, &QTimer::timeout, reply, [reply, context]() {
    // Mark the reply so callers can distinguish timeouts from other errors.
    reply->setProperty(kTimedOutProperty, true);
    reply->setProperty(kTimeoutContextProperty, context);
    qWarning() << "SAM2: Request timed out (" << context
               << "), aborting:" << reply->url();
    reply->abort();
  });

  QObject::connect(reply, &QNetworkReply::finished, timer, &QTimer::stop);
  QObject::connect(reply, &QNetworkReply::finished, timer, &QObject::deleteLater);
  timer->start();
}

QString timeoutErrorMessage(const QNetworkReply *reply) {
  if (!reply) {
    return {};
  }

  if (!reply->property(kTimedOutProperty).toBool()) {
    return {};
  }

  const QString context = reply->property(kTimeoutContextProperty).toString();
  if (!context.isEmpty()) {
    return QString("%1 request timed out").arg(context);
  }
  return QStringLiteral("Request timed out");
}

bool parseJsonObject(const QByteArray &data, QJsonObject &out,
                     QString &errorMessage) {
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    errorMessage = parseError.errorString();
    return false;
  }
  if (doc.isNull() || !doc.isObject()) {
    errorMessage = QStringLiteral("JSON root is not an object");
    return false;
  }

  out = doc.object();
  return true;
}
} // namespace

void SAM2Client::setDefaultAuthToken(const QByteArray &token) {
  g_authToken = token.trimmed();
}

void SAM2Client::authorizeRequest(QNetworkRequest &request) {
  request.setRawHeader("Accept", "application/json");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  if (g_authToken.size() >= kMinAuthTokenBytes) {
    request.setRawHeader("Authorization", "Bearer " + g_authToken);
  }
}

bool SAM2Client::isAuthenticatedProtocol(const QNetworkReply *reply) {
  return reply && reply->rawHeader(kProtocolHeader) == kProtocolVersion;
}

SAM2Client::SAM2Client(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_serviceUrl("http://127.0.0.1:5001")
{
  if (g_authToken.isEmpty()) {
    const QByteArray environmentToken =
        qEnvironmentVariable("DRAWINGSTUDIO_SAM2_TOKEN").trimmed().toUtf8();
    if (environmentToken.size() >= kMinAuthTokenBytes)
      g_authToken = environmentToken;
  }

  // Ensure custom signal payloads work in queued connections / tests.
  qRegisterMetaType<SAM2Client::SegmentationResult>(
      "SAM2Client::SegmentationResult");
  qRegisterMetaType<SAM2Client::MultiSegmentationResult>(
      "SAM2Client::MultiSegmentationResult");
}

void SAM2Client::setServiceUrl(const QString &urlString) {
  QUrl url(urlString);
  const QHostAddress address(url.host());
  const bool isLoopback = url.host().compare(QStringLiteral("localhost"),
                                              Qt::CaseInsensitive) == 0
      || address.isLoopback();
  if (!url.isValid() || url.scheme() != QStringLiteral("http") || !isLoopback) {
    qWarning() << "SAM2: Refusing non-loopback service URL" << urlString;
    return;
  }

  QString normalized = url.toString(QUrl::RemoveFragment | QUrl::RemoveQuery);
  while (normalized.endsWith('/'))
    normalized.chop(1);
  m_serviceUrl = normalized;
}

void SAM2Client::checkHealth() {
  QUrl url(m_serviceUrl + "/health");
  QNetworkRequest request(url);
  authorizeRequest(request);

  QNetworkReply *reply = m_networkManager->get(request);
  attachTimeout(reply, kHealthTimeoutMs, "Health check");

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QString timeoutMsg = timeoutErrorMessage(reply);
    if (!timeoutMsg.isEmpty()) {
      emit healthCheckComplete(false, timeoutMsg);
      reply->deleteLater();
      return;
    }

    if (reply->error() == QNetworkReply::NoError) {
      if (!isAuthenticatedProtocol(reply)) {
        emit healthCheckComplete(
            false, QStringLiteral("SAM2 service is not using the authenticated protocol"));
        reply->deleteLater();
        return;
      }
      QByteArray response = reply->readAll();
      QJsonObject obj;
      QString parseError;
      if (!parseJsonObject(response, obj, parseError)) {
        emit healthCheckComplete(false,
                                 QString("Invalid JSON response: %1")
                                     .arg(parseError));
        reply->deleteLater();
        return;
      }

      bool available = obj["sam2_loaded"].toBool(false);
      QString device = obj["device"].toString("unknown");

      if (!obj["auth_required"].toBool(false) ||
          obj["protocol_version"].toString() != QString::fromLatin1(kProtocolVersion)) {
        emit healthCheckComplete(
            false, QStringLiteral("SAM2 service authentication contract is invalid"));
        reply->deleteLater();
        return;
      }

      emit healthCheckComplete(available, device);
    } else {
      emit healthCheckComplete(false, reply->errorString());
    }

    reply->deleteLater();
  });
}

void SAM2Client::segmentImage(const QImage &image) {
  // Use segment_all endpoint to detect ALL objects in the image
  qDebug() << "SAM2: Sending image for multi-object detection...";

  QNetworkRequest request(QUrl(m_serviceUrl + "/segment_all"));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  authorizeRequest(request);

  QJsonObject json;
  json["image"] = QString::fromUtf8(imageToBase64(image));

  QJsonDocument doc(json);
  QByteArray data = doc.toJson();

  QNetworkReply *reply = m_networkManager->post(request, data);
  attachTimeout(reply, kSegmentationTimeoutMs, "Segmentation");

  // Start progress polling
  QTimer *progressTimer = new QTimer(this);
  progressTimer->setInterval(200); // Poll every 200ms

  connect(progressTimer, &QTimer::timeout, this, [this]() {
    // Poll progress endpoint
    QNetworkRequest progressRequest(QUrl(m_serviceUrl + "/progress"));
    authorizeRequest(progressRequest);
    QNetworkReply *progressReply = m_networkManager->get(progressRequest);
    attachTimeout(progressReply, kProgressTimeoutMs, "Progress");

    connect(progressReply, &QNetworkReply::finished, this,
            [this, progressReply]() {
              if (progressReply->error() == QNetworkReply::NoError &&
                  isAuthenticatedProtocol(progressReply)) {
                QJsonObject obj;
                QString parseError;
                if (parseJsonObject(progressReply->readAll(), obj, parseError)) {
                  int progress = obj["progress"].toInt();
                  QString message = obj["message"].toString();

                  emit segmentationProgress(progress, message);
                }
              }
              progressReply->deleteLater();
            });
  });

  progressTimer->start();

  connect(reply, &QNetworkReply::finished, this,
          [this, reply, progressTimer]() {
            progressTimer->stop();
            progressTimer->deleteLater();
            handleAllObjectsResponse(reply);
          });
}

void SAM2Client::segmentHumans(const QImage &image) {
  // Use segment_humans endpoint to detect humans using YOLO + SAM2
  QString urlStr = m_serviceUrl + "/segment_humans";
  qDebug() << "SAM2: Sending image for human detection (YOLO + SAM2)...";
  qDebug() << "SAM2: URL:" << urlStr;
  qDebug() << "SAM2: Image size:" << image.size();

  QNetworkRequest request((QUrl(urlStr)));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  authorizeRequest(request);

  QJsonObject json;
  QString base64Image = QString::fromUtf8(imageToBase64(image));
  qDebug() << "SAM2: Base64 image length:" << base64Image.length();

  json["image"] = base64Image;
  json["confidence"] = 0.5; // Default confidence threshold

  QJsonDocument doc(json);
  QByteArray data = doc.toJson();

  QNetworkReply *reply = m_networkManager->post(request, data);
  attachTimeout(reply, kSegmentationTimeoutMs, "Human detection");

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    qDebug() << "SAM2: Human detection response received";
    qDebug() << "SAM2: Error:" << reply->error();
    qDebug()
        << "SAM2: Status code:"
        << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
      qDebug() << "SAM2: Error string:" << reply->errorString();
    }

    handleAllObjectsResponse(reply);
  });
}

void SAM2Client::segmentWithPoint(const QImage &image, const QPointF &point) {

  QJsonObject json;
  json["image"] = QString::fromUtf8(imageToBase64(image));
  json["point_x"] = point.x();
  json["point_y"] = point.y();

  QJsonDocument doc(json);
  QByteArray data = doc.toJson();

  QUrl url(m_serviceUrl + "/segment_point");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  authorizeRequest(request);

  QNetworkReply *reply = m_networkManager->post(request, data);
  attachTimeout(reply, kSegmentationTimeoutMs, "Point segmentation");

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { handleSegmentationResponse(reply); });
}

void SAM2Client::segmentWithBox(const QImage &image, const QRectF &box) {
  qDebug() << "SAM2: Sending image for box-based segmentation" << box;

  QJsonObject json;
  json["image"] = QString::fromUtf8(imageToBase64(image));
  json["x1"] = box.left();
  json["y1"] = box.top();
  json["x2"] = box.right();
  json["y2"] = box.bottom();

  QJsonDocument doc(json);
  QByteArray data = doc.toJson();

  QUrl url(m_serviceUrl + "/segment_box");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  authorizeRequest(request);

  QNetworkReply *reply = m_networkManager->post(request, data);
  attachTimeout(reply, kSegmentationTimeoutMs, "Box segmentation");

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { handleSegmentationResponse(reply); });
}

void SAM2Client::handleAllObjectsResponse(QNetworkReply *reply) {
  const QString timeoutMsg = timeoutErrorMessage(reply);
  if (!timeoutMsg.isEmpty()) {
    emit segmentationFailed(timeoutMsg);
    reply->deleteLater();
    return;
  }

  if (reply->error() == QNetworkReply::NoError &&
      !isAuthenticatedProtocol(reply)) {
    emit segmentationFailed(
        QStringLiteral("SAM2 service is not using the authenticated protocol"));
    reply->deleteLater();
    return;
  }

  if (reply->error() == QNetworkReply::NoError) {
    QByteArray response = reply->readAll();
    QJsonObject obj;
    QString parseError;
    if (!parseJsonObject(response, obj, parseError)) {
      emit segmentationFailed(QString("Invalid JSON response: %1").arg(parseError));
      reply->deleteLater();
      return;
    }

    if (obj["success"].toBool()) {
      // Check if this is fast endpoint (single mask), multi-mask endpoint, or
      // humans endpoint
      QJsonArray objects;
      int totalFound = 1;

      if (obj.contains("objects")) {
        // Multi-mask response (/segment_all)
        objects = obj["objects"].toArray();
        totalFound = obj["total_found"].toInt();
      } else if (obj.contains("humans")) {
        // Human detection response (/segment_humans)
        QJsonArray humans = obj["humans"].toArray();
        totalFound = obj["count"].toInt();

        // Convert humans array to objects format
        for (int i = 0; i < humans.size(); ++i) {
          QJsonObject human = humans[i].toObject();
          QJsonObject converted;
          converted["id"] =
              human.contains("index") ? human["index"].toInt() : i;
          converted["mask"] = human["mask"];
          converted["mask_shape"] = human["mask_shape"];
          converted["contour"] = human["contour"];
          converted["score"] = human["sam2_score"];
          converted["stability"] = human["sam2_score"];
          converted["predicted_iou"] = human["confidence"];
          converted["area_percent"] = human["area_percent"];
          objects.append(converted);
        }
      } else {
        // Fast endpoint - single mask, wrap it in array
        QJsonObject singleMask;
        singleMask["id"] = 0;
        singleMask["mask"] = obj["mask"];
        singleMask["mask_shape"] = obj["mask_shape"];
        singleMask["contour"] = obj["contour"];
        singleMask["score"] = obj["score"];
        singleMask["stability"] = obj["score"]; // Use score as stability
        singleMask["predicted_iou"] = obj["score"];
        singleMask["area_percent"] = obj["area_percent"];
        objects.append(singleMask);
        totalFound = 1;
      }

      qDebug() << "SAM2: Found" << totalFound << "objects, returning top"
               << objects.size();

      if (!objects.isEmpty()) {
        // Parse ALL candidates for user selection
        MultiSegmentationResult multiResult;
        multiResult.success = true;
        multiResult.total_found = totalFound;

        for (const QJsonValue &objVal : objects) {
          QJsonObject candidateObj = objVal.toObject();

          MaskCandidate candidate;
          candidate.id = candidateObj["id"].toInt();
          candidate.score = candidateObj["score"].toDouble();
          candidate.stability = candidateObj["stability"].toDouble();
          candidate.predicted_iou = candidateObj["predicted_iou"].toDouble();
          candidate.area_percent = candidateObj["area_percent"].toDouble();

          // Parse contour - keep original order
          QJsonArray contourArray = candidateObj["contour"].toArray();
          for (const QJsonValue &pointVal : contourArray) {
            QJsonArray point = pointVal.toArray();
            if (point.size() >= 2) {
              candidate.contour.push_back(
                  QPointF(point[0].toDouble(), point[1].toDouble()));
            }
          }

          // Decode per-pixel mask (detection resolution) for precise cutouts
          QString maskB64 = candidateObj["mask"].toString();
          QJsonArray shapeArray = candidateObj["mask_shape"].toArray();
          if (!maskB64.isEmpty() && shapeArray.size() >= 2) {
            int height = shapeArray[0].toInt();
            int width = shapeArray[1].toInt();
            candidate.mask = base64ToMask(maskB64, width, height);
          }

          multiResult.candidates.push_back(candidate);
        }

        qDebug() << "SAM2: Returning" << multiResult.candidates.size()
                 << "candidates for selection";

        // Emit multi-candidate result only — also emitting segmentationComplete
        // would overwrite ImagePrimitive's candidate list with a single mask.
        emit multiSegmentationComplete(multiResult);
      } else {
        emit segmentationFailed("No objects detected");
      }
    } else {
      QString error = obj["error"].toString();
      qDebug() << "SAM2: All objects detection failed:" << error;
      emit segmentationFailed(error);
    }
  } else {
    QString error = reply->errorString();
    qDebug() << "SAM2: Network error:" << error;
    emit segmentationFailed("Network error: " + error);
  }

  reply->deleteLater();
}

void SAM2Client::handleSegmentationResponse(QNetworkReply *reply) {
  const QString timeoutMsg = timeoutErrorMessage(reply);
  if (!timeoutMsg.isEmpty()) {
    emit segmentationFailed(timeoutMsg);
    reply->deleteLater();
    return;
  }

  if (reply->error() == QNetworkReply::NoError &&
      !isAuthenticatedProtocol(reply)) {
    emit segmentationFailed(
        QStringLiteral("SAM2 service is not using the authenticated protocol"));
    reply->deleteLater();
    return;
  }

  if (reply->error() == QNetworkReply::NoError) {
    QByteArray response = reply->readAll();
    QJsonObject obj;
    QString parseError;
    if (!parseJsonObject(response, obj, parseError)) {
      emit segmentationFailed(QString("Invalid JSON response: %1").arg(parseError));
      reply->deleteLater();
      return;
    }

    if (obj["success"].toBool()) {
      SegmentationResult result;
      result.success = true;

      // Parse mask
      QString maskB64 = obj["mask"].toString();
      QJsonArray shapeArray = obj["mask_shape"].toArray();
      if (shapeArray.size() < 2) {
        emit segmentationFailed("Invalid mask_shape in response");
        reply->deleteLater();
        return;
      }

      int height = shapeArray[0].toInt();
      int width = shapeArray[1].toInt();
      result.mask = base64ToMask(maskB64, width, height);
      if (result.mask.isNull()) {
        emit segmentationFailed("Invalid mask data in response");
        reply->deleteLater();
        return;
      }

      // Parse contour
      QJsonArray contourArray = obj["contour"].toArray();
      for (const QJsonValue &pointVal : contourArray) {
        QJsonArray point = pointVal.toArray();
        if (point.size() >= 2) {
          result.contour.push_back(
              QPointF(point[0].toDouble(), point[1].toDouble()));
        }
      }

      if (result.contour.empty()) {
        emit segmentationFailed("Empty contour in response");
        reply->deleteLater();
        return;
      }

      result.confidence = obj["score"].toDouble();

      qDebug() << "SAM2: Segmentation complete! Confidence:"
               << result.confidence
               << "Contour points:" << result.contour.size();

      emit segmentationComplete(result);
    } else {
      QString error = obj["error"].toString();
      qDebug() << "SAM2: Segmentation failed:" << error;
      emit segmentationFailed(error);
    }
  } else {
    QString error = reply->errorString();
    qDebug() << "SAM2: Network error:" << error;
    emit segmentationFailed("Network error: " + error);
  }

  reply->deleteLater();
}

QByteArray SAM2Client::imageToBase64(const QImage &image) {
  QByteArray ba;
  QBuffer buffer(&ba);
  buffer.open(QIODevice::WriteOnly);

  // Convert to RGB format for consistency
  QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
  rgbImage.save(&buffer, "PNG");

  return ba.toBase64();
}

QImage SAM2Client::base64ToMask(const QString &base64, int width, int height) {
  if (width <= 0 || height <= 0) {
    qWarning() << "SAM2: Invalid mask dimensions" << width << "x" << height;
    return {};
  }
  if (base64.isEmpty()) {
    qWarning() << "SAM2: Empty base64 mask data";
    return {};
  }

  QByteArray maskData = QByteArray::fromBase64(base64.toUtf8());
  const int expectedSize = width * height;

  if (maskData.size() != expectedSize) {
    qWarning() << "SAM2: Mask size mismatch. Expected:" << expectedSize
               << "Got:" << maskData.size();
    return {};
  }

  QImage mask(width, height, QImage::Format_Grayscale8);
  mask.fill(Qt::black);
  memcpy(mask.bits(), maskData.constData(), expectedSize);

  return mask;
}
