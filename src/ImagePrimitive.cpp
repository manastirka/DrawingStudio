#include "ImagePrimitive.h"
#include "MaskCache.h"
#include "SAM2Client.h"
#include <QBuffer>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <limits>

// ImagePrimitive core + JSON (refactor E22).

// --- ImagePrimitive ---
ImagePrimitive::ImagePrimitive()
    : DrawingPrimitive(PrimitiveType::Image), m_position(0.0f, 0.0f),
      m_size(100.0f, 100.0f), m_rotation(0.0f), m_maintainAspectRatio(false),
      m_sam2Client(new SAM2Client(this)),
      m_detectionInProgress(false),
      m_detectionScaleX(1.0f), m_detectionScaleY(1.0f), m_selectedMaskIndex(0),
      m_maskInverted(false), m_editMode(false), m_draggingControlPoint(-1),
      m_contourSmoothness(0), m_maskFeather(0), m_maskBlur(0), m_maskExpand(0),
      m_maskOverlayVisible(true) {
  setColor(Qt::black);

  connect(m_sam2Client, &SAM2Client::segmentationComplete, this,
          &ImagePrimitive::onSAM2SegmentationComplete);
  connect(m_sam2Client, &SAM2Client::multiSegmentationComplete, this,
          &ImagePrimitive::onSAM2MultiSegmentationComplete);
  connect(m_sam2Client, &SAM2Client::segmentationFailed, this,
          &ImagePrimitive::onSAM2SegmentationFailed);
  connect(m_sam2Client, &SAM2Client::segmentationProgress, this,
          &ImagePrimitive::detectionProgress);
}


// --- ImagePrimitive ---
ImagePrimitive::ImagePrimitive(const QImage &image, const QVector2D &position,
                               const QVector2D &size)
    : DrawingPrimitive(PrimitiveType::Image), m_image(image),
      m_position(position), m_size(size), m_rotation(0.0f),
      m_maintainAspectRatio(false), m_sam2Client(new SAM2Client(this)),
      m_detectionInProgress(false), m_detectionScaleX(1.0f),
      m_detectionScaleY(1.0f), m_selectedMaskIndex(-1), m_maskInverted(false),
      m_editMode(false), m_draggingControlPoint(-1), m_contourSmoothness(0),
      m_maskFeather(0), m_maskBlur(0), m_maskExpand(0),
      m_maskOverlayVisible(true) {
  setColor(Qt::black);

  // Connect SAM2 signals
  connect(m_sam2Client, &SAM2Client::segmentationComplete, this,
          &ImagePrimitive::onSAM2SegmentationComplete);
  connect(m_sam2Client, &SAM2Client::multiSegmentationComplete, this,
          &ImagePrimitive::onSAM2MultiSegmentationComplete);
  connect(m_sam2Client, &SAM2Client::segmentationFailed, this,
          &ImagePrimitive::onSAM2SegmentationFailed);
  connect(m_sam2Client, &SAM2Client::segmentationProgress, this,
          &ImagePrimitive::detectionProgress);

  // Don't auto-detect - user will trigger manually
  // autoDetectSubject();
}


// --- ~ImagePrimitive ---
ImagePrimitive::~ImagePrimitive() {
}


// --- render ---
void ImagePrimitive::render(QPainter* painter) const {
  if (m_image.isNull()) {
    return;
  }

  // Draw the image using QPainter
  painter->save();
  painter->translate(m_position.x() + m_size.x()/2.0f, m_position.y() + m_size.y()/2.0f);
  painter->rotate(m_rotation);
  // Y-flip: in world space Y is flipped, so images need scale(1,-1) to appear right-side-up
  painter->scale(1, -1);
  painter->setOpacity(m_opacityMultiplier);
  painter->drawImage(QRectF(-m_size.x()/2.0f, -m_size.y()/2.0f, m_size.x(), m_size.y()), m_image);
  painter->setOpacity(1.0);
  painter->restore();

  // Draw all multi-selected green mask overlays when image is selected or editing
  if (m_maskOverlayVisible && (isSelected() || m_editMode) &&
      !m_selectedMaskIndices.empty()) {
    painter->save();
    painter->translate(m_position.x(), m_position.y());

    const float scaleX = m_size.x() / m_image.width();
    const float scaleY = m_size.y() / m_image.height();

    auto drawCandidate = [&](int index) {
      if (index < 0 || index >= static_cast<int>(m_maskCandidates.size()))
        return;
      const auto &cand = m_maskCandidates[index];
      if (cand.contour.empty())
        return;

      std::vector<QPointF> renderContour = cand.contour;
      QPainterPath contourPath;
      float y_flipped = m_size.y() - (renderContour[0].y() * scaleY);
      contourPath.moveTo(renderContour[0].x() * scaleX, y_flipped);
      for (size_t i = 1; i < renderContour.size(); ++i) {
        y_flipped = m_size.y() - (renderContour[i].y() * scaleY);
        contourPath.lineTo(renderContour[i].x() * scaleX, y_flipped);
      }
      contourPath.closeSubpath();

      QPen outlinePen(m_maskInverted ? QColor(255, 128, 0) : QColor(0, 255, 0), 1.5f);
      outlinePen.setCosmetic(true);
      painter->setPen(outlinePen);
      painter->setBrush(Qt::NoBrush);
      painter->drawPath(contourPath);

      QColor fillColor =
          m_maskInverted ? QColor(255, 128, 0, 38) : QColor(0, 255, 0, 38);
      painter->setPen(Qt::NoPen);
      if (m_maskInverted) {
        QPainterPath rectPath;
        rectPath.addRect(0, 0, m_size.x(), m_size.y());
        painter->fillPath(rectPath.subtracted(contourPath), fillColor);
      } else {
        painter->fillPath(contourPath, fillColor);
      }
    };

    for (int idx : m_selectedMaskIndices)
      drawCandidate(idx);

    painter->restore();
  } else if (m_maskOverlayVisible && !m_detectedSubject.contour.empty() &&
             (isSelected() || m_editMode)) {
    // Fallback: single detected contour (legacy path)
    painter->save();
    painter->translate(m_position.x(), m_position.y());

    float scaleX = m_size.x() / m_image.width();
    float scaleY = m_size.y() / m_image.height();
    std::vector<QPointF> renderContour = m_detectedSubject.contour;
    QPainterPath contourPath;
    if (!renderContour.empty()) {
      float y_flipped = m_size.y() - (renderContour[0].y() * scaleY);
      contourPath.moveTo(renderContour[0].x() * scaleX, y_flipped);
      for (size_t i = 1; i < renderContour.size(); ++i) {
        y_flipped = m_size.y() - (renderContour[i].y() * scaleY);
        contourPath.lineTo(renderContour[i].x() * scaleX, y_flipped);
      }
      contourPath.closeSubpath();
    }
    QPen outlinePen(m_maskInverted ? QColor(255, 128, 0) : QColor(0, 255, 0), 1.5f);
    outlinePen.setCosmetic(true);
    painter->setPen(outlinePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(contourPath);
    QColor fillColor =
        m_maskInverted ? QColor(255, 128, 0, 38) : QColor(0, 255, 0, 38);
    painter->setPen(Qt::NoPen);
    painter->fillPath(contourPath, fillColor);
    painter->restore();
  }

  // Draw control points AFTER everything else (in edit mode)
  if (m_editMode && !m_detectedSubject.contour.empty()) {
    // Scale contour to match current display size
    float scaleX = m_size.x() / m_image.width();
    float scaleY = m_size.y() / m_image.height();

    // Size handles relative to the image so they're visible at any zoom.
    // Clamped to avoid huge dots on tiny thumbnails or invisible dots on huge images.
    float baseDim = std::min(m_size.x(), m_size.y());
    float cpRadius = std::clamp(baseDim * 0.005f, 0.8f, 2.5f);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    for (const auto &point : m_detectedSubject.contour) {
      // Transform to world coordinates
      float wx = m_position.x() + point.x() * scaleX;
      float wy = m_position.y() + (m_size.y() - point.y() * scaleY);

      // Draw green filled circle
      painter->setPen(Qt::NoPen);
      painter->setBrush(QColor(0, 255, 0, 230));
      painter->drawEllipse(QPointF(wx, wy), cpRadius, cpRadius);

      // Draw white center circle
      float innerRadius = cpRadius * 0.5f;
      painter->setBrush(QColor(255, 255, 255, 255));
      painter->drawEllipse(QPointF(wx, wy), innerRadius, innerRadius);
    }

    painter->restore();
  }
}


// --- boundingRect ---
QRectF ImagePrimitive::boundingRect() const {
  return QRectF(m_position.x(), m_position.y(), m_size.x(), m_size.y());
}


// --- containsPoint ---
bool ImagePrimitive::containsPoint(const QVector2D &point,
                                   float tolerance) const {
  // If image is rotated, we need to transform the point into image space
  QVector2D localPoint = point;

  if (m_rotation != 0.0f) {
    // Transform point to image's local coordinate system
    // 1. Translate to image center
    QVector2D center =
        m_position + QVector2D(m_size.x() / 2.0f, m_size.y() / 2.0f);
    QVector2D relativePoint = point - center;

    // 2. Rotate by negative angle (inverse rotation)
    float radians = -m_rotation * M_PI / 180.0f;
    float cosR = std::cos(radians);
    float sinR = std::sin(radians);

    float rotatedX = relativePoint.x() * cosR - relativePoint.y() * sinR;
    float rotatedY = relativePoint.x() * sinR + relativePoint.y() * cosR;

    // 3. Translate back
    localPoint = QVector2D(rotatedX, rotatedY) + center;
  }

  // Hit the full bounds — including transparent padding on cutouts — so
  // Shift-click multi-select works without needing to click the silhouette.
  QRectF rect = boundingRect();
  rect.adjust(-tolerance, -tolerance, tolerance, tolerance);
  return rect.contains(localPoint.toPointF());
}


// --- clone ---
std::unique_ptr<DrawingPrimitive> ImagePrimitive::clone() const {
  auto clone = std::make_unique<ImagePrimitive>(m_image, m_position, m_size);
  clone->setColor(color());
  clone->setFillColor(fillColor());
  clone->setLineWidth(lineWidth());
  clone->setRotation(m_rotation);
  clone->setMaintainAspectRatio(m_maintainAspectRatio);

  // Copy mask detection state so duplicated images keep their masks
  clone->m_maskCandidates = m_maskCandidates;
  clone->m_selectedMaskIndex = m_selectedMaskIndex;
  clone->m_selectedMaskIndices = m_selectedMaskIndices;
  clone->m_maskInverted = m_maskInverted;
  clone->m_detectedSubject = m_detectedSubject;
  clone->m_detectionScaleX = m_detectionScaleX;
  clone->m_detectionScaleY = m_detectionScaleY;
  clone->m_contourSmoothness = m_contourSmoothness;
  clone->m_maskFeather = m_maskFeather;
  clone->m_maskBlur = m_maskBlur;
  clone->m_maskExpand = m_maskExpand;
  clone->m_maskOverlayVisible = m_maskOverlayVisible;

  return clone;
}


// --- translate ---
void ImagePrimitive::translate(const QVector2D &offset) {
  qDebug() << "ImagePrimitive::translate: offset" << offset.x() << ","
           << offset.y() << "old pos:" << m_position.x() << ","
           << m_position.y();
  m_position += offset;
  qDebug() << "  new pos:" << m_position.x() << "," << m_position.y();
}


// --- getControlPoints ---
std::vector<QVector2D> ImagePrimitive::getControlPoints() const {
  std::vector<QVector2D> points;

  // Add 8 resize handles
  points.push_back(QVector2D(m_position.x(), m_position.y())); // Top-left
  points.push_back(
      QVector2D(m_position.x() + m_size.x(), m_position.y())); // Top-right
  points.push_back(
      QVector2D(m_position.x(), m_position.y() + m_size.y())); // Bottom-left
  points.push_back(QVector2D(m_position.x() + m_size.x(),
                             m_position.y() + m_size.y())); // Bottom-right
  points.push_back(
      QVector2D(m_position.x() + m_size.x() / 2.0f, m_position.y())); // Top
  points.push_back(QVector2D(m_position.x() + m_size.x() / 2.0f,
                             m_position.y() + m_size.y())); // Bottom
  points.push_back(
      QVector2D(m_position.x(), m_position.y() + m_size.y() / 2.0f)); // Left
  points.push_back(QVector2D(m_position.x() + m_size.x(),
                             m_position.y() + m_size.y() / 2.0f)); // Right

  return points;
}


// --- setControlPointPosition ---
void ImagePrimitive::setControlPointPosition(int index,
                                             const QVector2D &position) {
  if (index >= 0 && index < 8) {
    resizeFromHandle(static_cast<ResizeHandle>(index), position);
  }
}


// --- setImage ---
void ImagePrimitive::setImage(const QImage &image) {
  m_image = image;
}


// --- setPosition ---
void ImagePrimitive::setPosition(const QVector2D &position) {
  m_position = position;
}


// --- setSize ---
void ImagePrimitive::setSize(const QVector2D &size) {
  if (m_maintainAspectRatio && !m_image.isNull()) {
    float aspectRatio = static_cast<float>(m_image.width()) /
                        static_cast<float>(m_image.height());
    if (size.x() / aspectRatio != size.y()) {
      m_size = QVector2D(size.x(), size.x() / aspectRatio);
      return;
    }
  }
  m_size = size;
}


// --- setRotation ---
void ImagePrimitive::setRotation(float rotation) { m_rotation = rotation; }


// --- getResizeHandleAt ---
ImagePrimitive::ResizeHandle
ImagePrimitive::getResizeHandleAt(const QVector2D &point,
                                  float tolerance) const {
  auto handles = getControlPoints();
  for (int i = 0; i < 8; ++i) {
    if ((point - handles[i]).length() <= tolerance) {
      return static_cast<ResizeHandle>(i);
    }
  }
  return None;
}


// --- getHandlePosition ---
QVector2D ImagePrimitive::getHandlePosition(ResizeHandle handle) const {
  auto handles = getControlPoints();
  if (handle >= 0 && handle < 8) {
    return handles[handle];
  }
  return QVector2D();
}


// --- getResizeHandleRect ---
QRectF ImagePrimitive::getResizeHandleRect(ResizeHandle handle) const {
  QVector2D pos = getHandlePosition(handle);
  return QRectF(pos.x() - HANDLE_SIZE / 2.0f, pos.y() - HANDLE_SIZE / 2.0f,
                HANDLE_SIZE, HANDLE_SIZE);
}


// --- resizeFromHandle ---
void ImagePrimitive::resizeFromHandle(ResizeHandle handle,
                                      const QVector2D &newPosition) {
  float aspectRatio = m_maintainAspectRatio && !m_image.isNull()
                          ? static_cast<float>(m_image.width()) /
                                static_cast<float>(m_image.height())
                          : 0.0f;

  QVector2D newPos = m_position;
  QVector2D newSize = m_size;

  switch (handle) {
  case TopLeft:
    newSize = QVector2D(m_position.x() + m_size.x() - newPosition.x(),
                        m_position.y() + m_size.y() - newPosition.y());
    newPos = newPosition;
    break;
  case TopRight:
    newSize = QVector2D(newPosition.x() - m_position.x(),
                        m_position.y() + m_size.y() - newPosition.y());
    newPos = QVector2D(m_position.x(), newPosition.y());
    break;
  case BottomLeft:
    newSize = QVector2D(m_position.x() + m_size.x() - newPosition.x(),
                        newPosition.y() - m_position.y());
    newPos = QVector2D(newPosition.x(), m_position.y());
    break;
  case BottomRight:
    newSize = QVector2D(newPosition.x() - m_position.x(),
                        newPosition.y() - m_position.y());
    break;
  case Top:
    newSize =
        QVector2D(m_size.x(), m_position.y() + m_size.y() - newPosition.y());
    newPos = QVector2D(m_position.x(), newPosition.y());
    break;
  case Bottom:
    newSize = QVector2D(m_size.x(), newPosition.y() - m_position.y());
    break;
  case Left:
    newSize =
        QVector2D(m_position.x() + m_size.x() - newPosition.x(), m_size.y());
    newPos = QVector2D(newPosition.x(), m_position.y());
    break;
  case Right:
    newSize = QVector2D(newPosition.x() - m_position.x(), m_size.y());
    break;
  default:
    return;
  }

  // Maintain aspect ratio if needed
  if (m_maintainAspectRatio && aspectRatio > 0.0f) {
    if (handle == Top || handle == Bottom) {
      newSize.setX(newSize.y() * aspectRatio);
    } else if (handle == Left || handle == Right) {
      newSize.setY(newSize.x() / aspectRatio);
    } else {
      // Corner handles - maintain aspect based on which dimension changed more
      if (std::abs(newSize.x() - m_size.x()) >
          std::abs(newSize.y() - m_size.y())) {
        newSize.setY(newSize.x() / aspectRatio);
      } else {
        newSize.setX(newSize.y() * aspectRatio);
      }
    }
  }

  m_position = newPos;
  m_size = newSize;
}


// --- toJson ---
QJsonObject ImagePrimitive::toJson() const {
  QJsonObject json = DrawingPrimitive::toJson();

  // Save position and size
  json["posX"] = m_position.x();
  json["posY"] = m_position.y();
  json["sizeX"] = m_size.x();
  json["sizeY"] = m_size.y();
  json["rotation"] = m_rotation;

  // Save image as base64 PNG
  QByteArray imageData;
  QBuffer buffer(&imageData);
  buffer.open(QIODevice::WriteOnly);
  m_image.save(&buffer, "PNG");
  json["imageData"] = QString(imageData.toBase64());

  // Save mask contour if exists
  if (!m_detectedSubject.contour.empty()) {
    QJsonArray contourArray;
    for (const auto &point : m_detectedSubject.contour) {
      QJsonObject pointObj;
      pointObj["x"] = point.x();
      pointObj["y"] = point.y();
      contourArray.append(pointObj);
    }
    json["maskContour"] = contourArray;
  }

  // Save mask candidates so cycling/selection survives save/load
  if (!m_maskCandidates.empty()) {
    QJsonArray candidatesArray;
    for (const auto &candidate : m_maskCandidates) {
      QJsonObject candObj;
      candObj["id"] = candidate.id;
      candObj["score"] = candidate.score;
      candObj["stability"] = candidate.stability;
      candObj["predicted_iou"] = candidate.predicted_iou;
      candObj["area_percent"] = candidate.area_percent;

      QJsonArray candContour;
      for (const auto &point : candidate.contour) {
        QJsonObject pointObj;
        pointObj["x"] = point.x();
        pointObj["y"] = point.y();
        candContour.append(pointObj);
      }
      candObj["contour"] = candContour;
      candidatesArray.append(candObj);
    }
    json["maskCandidates"] = candidatesArray;
    json["selectedMaskIndex"] = m_selectedMaskIndex;
    json["maskInverted"] = m_maskInverted;
  }

  return json;
}


// --- fromJson ---
void ImagePrimitive::fromJson(const QJsonObject &json) {
  DrawingPrimitive::fromJson(json);

  // Restore position and size
  m_position = QVector2D(json["posX"].toDouble(), json["posY"].toDouble());
  m_size = QVector2D(json["sizeX"].toDouble(), json["sizeY"].toDouble());
  m_rotation = json["rotation"].toDouble();

  // Restore image from base64
  QByteArray imageData =
      QByteArray::fromBase64(json["imageData"].toString().toUtf8());
  m_image.loadFromData(imageData, "PNG");

  // Restore mask contour if exists
  if (json.contains("maskContour")) {
    QJsonArray contourArray = json["maskContour"].toArray();
    m_detectedSubject.contour.clear();
    for (const QJsonValue &pointValue : contourArray) {
      QJsonObject pointObj = pointValue.toObject();
      m_detectedSubject.contour.push_back(
          QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble()));
    }
  }

  // Restore mask candidates and selection so cycling still works after reload
  if (json.contains("maskCandidates")) {
    QJsonArray candidatesArray = json["maskCandidates"].toArray();
    m_maskCandidates.clear();
    for (const QJsonValue &candValue : candidatesArray) {
      QJsonObject candObj = candValue.toObject();
      MaskCandidate candidate;
      candidate.id = candObj["id"].toInt();
      candidate.score = candObj["score"].toDouble();
      candidate.stability = candObj["stability"].toDouble();
      candidate.predicted_iou = candObj["predicted_iou"].toDouble();
      candidate.area_percent = candObj["area_percent"].toDouble();
      QJsonArray candContour = candObj["contour"].toArray();
      for (const QJsonValue &pointValue : candContour) {
        QJsonObject pointObj = pointValue.toObject();
        candidate.contour.push_back(
            QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble()));
      }
      m_maskCandidates.push_back(candidate);
    }

    m_selectedMaskIndex = json.contains("selectedMaskIndex")
                              ? json["selectedMaskIndex"].toInt()
                              : (m_maskCandidates.empty() ? -1 : 0);
    m_maskInverted = json["maskInverted"].toBool(false);
  }
}

