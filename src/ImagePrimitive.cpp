#include "ImagePrimitive.h"
#include "MaskCache.h"
#include "SAM2Client.h"
#include <QBuffer>
#include <QDebug>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <limits>

// ImagePrimitive core + JSON (refactor E22).

namespace {
constexpr float kMinImageDimension = 1.0f;
constexpr qsizetype kMaxSerializedImageBytes = 64 * 1024 * 1024;
constexpr int kMaxSerializedImageDimension = 16384;
constexpr qint64 kMaxSerializedImagePixels = 64LL * 1024LL * 1024LL;

bool isValidResizeHandle(ImagePrimitive::ResizeHandle handle)
{
  const int index = static_cast<int>(handle);
  return index >= static_cast<int>(ImagePrimitive::TopLeft)
      && index < static_cast<int>(ImagePrimitive::ResizeHandleCount);
}

QImage decodeSerializedImage(const QJsonValue &value)
{
  if (!value.isString())
    return {};

  const QString encodedString = value.toString();
  constexpr qsizetype kMaxBase64Characters =
      ((kMaxSerializedImageBytes + 2) / 3) * 4;
  if (encodedString.isEmpty()
      || encodedString.size() > kMaxBase64Characters) {
    return {};
  }

  const auto decodedResult = QByteArray::fromBase64Encoding(
      encodedString.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
  if (!decodedResult || decodedResult.decoded.isEmpty()
      || decodedResult.decoded.size() > kMaxSerializedImageBytes) {
    return {};
  }

  QBuffer buffer;
  buffer.setData(decodedResult.decoded);
  if (!buffer.open(QIODevice::ReadOnly))
    return {};

  QImageReader reader(&buffer, "PNG");
  reader.setAutoTransform(false);
  reader.setDecideFormatFromContent(false);
  const QSize dimensions = reader.size();
  if (!dimensions.isValid() || dimensions.width() <= 0
      || dimensions.height() <= 0
      || dimensions.width() > kMaxSerializedImageDimension
      || dimensions.height() > kMaxSerializedImageDimension
      || static_cast<qint64>(dimensions.width()) * dimensions.height()
             > kMaxSerializedImagePixels) {
    return {};
  }

  QImage image = reader.read();
  if (image.isNull() || image.size() != dimensions)
    return {};
  return image;
}
}

// --- ImagePrimitive ---
ImagePrimitive::ImagePrimitive()
    : DrawingPrimitive(PrimitiveType::Image), m_position(0.0f, 0.0f),
      m_size(100.0f, 100.0f), m_rotation(0.0f), m_maintainAspectRatio(false),
      m_sam2Client(new SAM2Client(this)),
      m_detectionInProgress(false),
      m_detectionScaleX(1.0f), m_detectionScaleY(1.0f), m_selectedMaskIndex(-1),
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
  setSize(size);

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
  if (index >= 0 && index < static_cast<int>(ResizeHandleCount)) {
    resizeFromHandle(static_cast<ResizeHandle>(index), position);
  }
}


// --- setImage ---
void ImagePrimitive::setImage(const QImage &image) {
  m_image = image;
}


// --- setContourSmoothness ---
void ImagePrimitive::setContourSmoothness(int level) {
  m_contourSmoothness = std::clamp(level, 0, kMaxContourSmoothness);
}


// --- setMaskFeather ---
void ImagePrimitive::setMaskFeather(int amount) {
  m_maskFeather = std::clamp(amount, 0, kMaxMaskFeather);
}


// --- setMaskBlur ---
void ImagePrimitive::setMaskBlur(int amount) {
  m_maskBlur = std::clamp(amount, 0, kMaxMaskBlur);
}


// --- setMaskExpand ---
void ImagePrimitive::setMaskExpand(int amount) {
  m_maskExpand = std::clamp(amount, -kMaxMaskExpand, kMaxMaskExpand);
}


// --- setPosition ---
void ImagePrimitive::setPosition(const QVector2D &position) {
  if (isSupportedPoint(position))
    m_position = position;
}


// --- setSize ---
void ImagePrimitive::setSize(const QVector2D &size) {
  if (!std::isfinite(size.x()) || !std::isfinite(size.y()))
    return;

  QVector2D adjusted(std::max(kMinImageDimension, size.x()),
                     std::max(kMinImageDimension, size.y()));
  if (m_maintainAspectRatio && !m_image.isNull()
      && m_image.width() > 0 && m_image.height() > 0) {
    const float aspectRatio = static_cast<float>(m_image.width()) /
                              static_cast<float>(m_image.height());
    const float widthFromHeight = adjusted.y() * aspectRatio;
    const float tolerance =
        0.001f * std::max({1.0f, adjusted.x(), widthFromHeight});
    if (std::abs(adjusted.x() - widthFromHeight) > tolerance) {
      const float currentWidth = std::max(kMinImageDimension, m_size.x());
      const float currentHeight = std::max(kMinImageDimension, m_size.y());
      const float relativeWidthChange =
          std::abs(adjusted.x() - currentWidth) / currentWidth;
      const float relativeHeightChange =
          std::abs(adjusted.y() - currentHeight) / currentHeight;
      if (relativeHeightChange > relativeWidthChange)
        adjusted.setX(widthFromHeight);
      else
        adjusted.setY(adjusted.x() / aspectRatio);
    }

    // Preserve the ratio even for extremely wide or tall source images while
    // keeping both drawable dimensions above the minimum.
    const float minimumHeight =
        std::max(kMinImageDimension, kMinImageDimension / aspectRatio);
    if (adjusted.y() < minimumHeight) {
      adjusted.setY(minimumHeight);
      adjusted.setX(minimumHeight * aspectRatio);
    }
  }
  m_size = adjusted;
}


// --- setRotation ---
void ImagePrimitive::setRotation(float rotation) {
  if (std::isfinite(rotation))
    m_rotation = std::clamp(rotation, -3600.0f, 3600.0f);
}


// --- getResizeHandleAt ---
ImagePrimitive::ResizeHandle
ImagePrimitive::getResizeHandleAt(const QVector2D &point,
                                  float tolerance) const {
  auto handles = getControlPoints();
  for (int i = 0; i < static_cast<int>(ResizeHandleCount); ++i) {
    if ((point - handles[i]).length() <= tolerance) {
      return static_cast<ResizeHandle>(i);
    }
  }
  return None;
}


// --- getHandlePosition ---
QVector2D ImagePrimitive::getHandlePosition(ResizeHandle handle) const {
  if (!isValidResizeHandle(handle))
    return QVector2D();
  const auto handles = getControlPoints();
  return handles[static_cast<size_t>(handle)];
}


// --- getResizeHandleRect ---
QRectF ImagePrimitive::getResizeHandleRect(ResizeHandle handle) const {
  if (!isValidResizeHandle(handle))
    return QRectF();
  QVector2D pos = getHandlePosition(handle);
  return QRectF(pos.x() - HANDLE_SIZE / 2.0f, pos.y() - HANDLE_SIZE / 2.0f,
                HANDLE_SIZE, HANDLE_SIZE);
}


// --- resizeFromHandle ---
void ImagePrimitive::resizeFromHandle(ResizeHandle handle,
                                      const QVector2D &newPosition) {
  if (!isValidResizeHandle(handle) || !std::isfinite(newPosition.x())
      || !std::isfinite(newPosition.y()) || !std::isfinite(m_position.x())
      || !std::isfinite(m_position.y()) || !std::isfinite(m_size.x())
      || !std::isfinite(m_size.y()))
    return;

  const float originalWidth = std::max(kMinImageDimension, m_size.x());
  const float originalHeight = std::max(kMinImageDimension, m_size.y());
  float left = m_position.x();
  float top = m_position.y();
  float right = left + originalWidth;
  float bottom = top + originalHeight;

  switch (handle) {
  case TopLeft:
    left = std::min(newPosition.x(), right - kMinImageDimension);
    top = std::min(newPosition.y(), bottom - kMinImageDimension);
    break;
  case TopRight:
    right = std::max(newPosition.x(), left + kMinImageDimension);
    top = std::min(newPosition.y(), bottom - kMinImageDimension);
    break;
  case BottomLeft:
    left = std::min(newPosition.x(), right - kMinImageDimension);
    bottom = std::max(newPosition.y(), top + kMinImageDimension);
    break;
  case BottomRight:
    right = std::max(newPosition.x(), left + kMinImageDimension);
    bottom = std::max(newPosition.y(), top + kMinImageDimension);
    break;
  case Top:
    top = std::min(newPosition.y(), bottom - kMinImageDimension);
    break;
  case Bottom:
    bottom = std::max(newPosition.y(), top + kMinImageDimension);
    break;
  case Left:
    left = std::min(newPosition.x(), right - kMinImageDimension);
    break;
  case Right:
    right = std::max(newPosition.x(), left + kMinImageDimension);
    break;
  default:
    return;
  }

  const float aspectRatio =
      m_maintainAspectRatio && !m_image.isNull() && m_image.height() > 0
          ? static_cast<float>(m_image.width()) /
                static_cast<float>(m_image.height())
          : 0.0f;
  if (aspectRatio > 0.0f) {
    float width = right - left;
    float height = bottom - top;
    if (handle == Top || handle == Bottom) {
      right = left + height * aspectRatio;
    } else if (handle == Left || handle == Right) {
      bottom = top + width / aspectRatio;
    } else {
      const float relativeWidthChange =
          std::abs(width - originalWidth) / originalWidth;
      const float relativeHeightChange =
          std::abs(height - originalHeight) / originalHeight;
      if (relativeWidthChange >= relativeHeightChange) {
        height = width / aspectRatio;
        if (handle == TopLeft || handle == TopRight)
          top = bottom - height;
        else
          bottom = top + height;
      } else {
        width = height * aspectRatio;
        if (handle == TopLeft || handle == BottomLeft)
          left = right - width;
        else
          right = left + width;
      }
    }

    // If the image has an extreme aspect ratio, satisfying one dimension's
    // minimum can otherwise make the other dimension sub-pixel. Grow both
    // dimensions together and retain the edge opposite the dragged handle.
    height = right > left ? (right - left) / aspectRatio : 0.0f;
    const float minimumHeight =
        std::max(kMinImageDimension, kMinImageDimension / aspectRatio);
    if (height < minimumHeight) {
      height = minimumHeight;
      const float width = height * aspectRatio;
      if (handle == TopLeft || handle == BottomLeft || handle == Left)
        left = right - width;
      else
        right = left + width;

      if (handle == TopLeft || handle == TopRight || handle == Top)
        top = bottom - height;
      else
        bottom = top + height;
    }
  }

  m_position = QVector2D(left, top);
  m_size = QVector2D(std::max(kMinImageDimension, right - left),
                     std::max(kMinImageDimension, bottom - top));
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
  json["maintainAspectRatio"] = m_maintainAspectRatio;
  json["contourSmoothness"] = m_contourSmoothness;
  json["maskFeather"] = m_maskFeather;
  json["maskBlur"] = m_maskBlur;
  json["maskExpand"] = m_maskExpand;
  json["maskOverlayVisible"] = m_maskOverlayVisible;
  json["maskInverted"] = m_maskInverted;

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
  }

  json["selectedMaskIndex"] = m_selectedMaskIndex;
  if (!m_selectedMaskIndices.empty()) {
    QJsonArray selectedArray;
    for (int index : m_selectedMaskIndices)
      selectedArray.append(index);
    json["selectedMaskIndices"] = selectedArray;
  }

  return json;
}


// --- fromJson ---
void ImagePrimitive::fromJson(const QJsonObject &json) {
  DrawingPrimitive::fromJson(json);

  // Restore geometry. Apply the saved size only after the image and aspect
  // lock are available so invalid dimensions are clamped consistently.
  m_position = QVector2D(json["posX"].toDouble(), json["posY"].toDouble());
  const QVector2D savedSize(json["sizeX"].toDouble(),
                            json["sizeY"].toDouble());
  m_rotation = json["rotation"].toDouble();

  // Restore image from base64
  m_image = decodeSerializedImage(json["imageData"]);
  m_maintainAspectRatio = json["maintainAspectRatio"].toBool(false);
  setSize(savedSize);

  // Optional mask data must be reset before it is restored. fromJson() is
  // also used for in-place undo/redo, where retaining an omitted old value
  // would leak mask state from a later snapshot.
  m_detectedSubject = DetectedSubject{};
  m_maskCandidates.clear();
  m_selectedMaskIndex = -1;
  m_selectedMaskIndices.clear();
  m_maskInverted = json["maskInverted"].toBool(false);
  setContourSmoothness(json["contourSmoothness"].toInt(0));
  setMaskFeather(json["maskFeather"].toInt(0));
  setMaskBlur(json["maskBlur"].toInt(0));
  setMaskExpand(json["maskExpand"].toInt(0));
  m_maskOverlayVisible = json["maskOverlayVisible"].toBool(true);

  // Restore mask contour if exists
  if (json["maskContour"].isArray()) {
    const QJsonArray contourArray = json["maskContour"].toArray();
    const qsizetype pointCount =
        std::min(contourArray.size(), kMaxSerializedMaskPoints);
    m_detectedSubject.contour.reserve(static_cast<size_t>(pointCount));
    for (qsizetype i = 0; i < pointCount; ++i) {
      const QJsonValue pointValue = contourArray.at(i);
      if (!pointValue.isObject())
        continue;
      const QJsonObject pointObj = pointValue.toObject();
      if (!pointObj["x"].isDouble() || !pointObj["y"].isDouble())
        continue;
      m_detectedSubject.contour.push_back(
          QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble()));
    }
  }

  // Restore mask candidates and selection so cycling still works after reload
  if (json["maskCandidates"].isArray()) {
    const QJsonArray candidatesArray = json["maskCandidates"].toArray();
    const qsizetype candidateCount =
        std::min(candidatesArray.size(), kMaxSerializedMaskCandidates);
    m_maskCandidates.reserve(static_cast<size_t>(candidateCount));
    for (qsizetype i = 0; i < candidateCount; ++i) {
      const QJsonValue candValue = candidatesArray.at(i);
      if (!candValue.isObject())
        continue;
      const QJsonObject candObj = candValue.toObject();
      MaskCandidate candidate{};
      candidate.id = candObj["id"].toInt();
      candidate.score = candObj["score"].toDouble();
      candidate.stability = candObj["stability"].toDouble();
      candidate.predicted_iou = candObj["predicted_iou"].toDouble();
      candidate.area_percent = candObj["area_percent"].toDouble();
      const QJsonArray candContour = candObj["contour"].toArray();
      const qsizetype contourPointCount =
          std::min(candContour.size(), kMaxSerializedMaskPoints);
      candidate.contour.reserve(static_cast<size_t>(contourPointCount));
      for (qsizetype pointIndex = 0; pointIndex < contourPointCount;
           ++pointIndex) {
        const QJsonValue pointValue = candContour.at(pointIndex);
        if (!pointValue.isObject())
          continue;
        const QJsonObject pointObj = pointValue.toObject();
        if (!pointObj["x"].isDouble() || !pointObj["y"].isDouble())
          continue;
        candidate.contour.push_back(
            QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble()));
      }
      m_maskCandidates.push_back(candidate);
    }
  }

  const int requestedSelection = json.contains("selectedMaskIndex")
                                     ? json["selectedMaskIndex"].toInt(-1)
                                     : (m_maskCandidates.empty() ? -1 : 0);
  if (requestedSelection == -1) {
    m_selectedMaskIndex = -1;
  } else if (requestedSelection >= 0
             && requestedSelection
                    < static_cast<int>(m_maskCandidates.size())) {
    m_selectedMaskIndex = requestedSelection;
  } else {
    m_selectedMaskIndex = m_maskCandidates.empty() ? -1 : 0;
  }

  if (json["selectedMaskIndices"].isArray()) {
    const QJsonArray selectedArray = json["selectedMaskIndices"].toArray();
    const qsizetype selectionCount =
        std::min(selectedArray.size(), kMaxSerializedMaskCandidates);
    for (qsizetype i = 0; i < selectionCount; ++i) {
      const QJsonValue selectedValue = selectedArray.at(i);
      const int index = selectedValue.toInt(-1);
      if (index < 0 || index >= static_cast<int>(m_maskCandidates.size())
          || std::find(m_selectedMaskIndices.begin(),
                       m_selectedMaskIndices.end(), index)
                 != m_selectedMaskIndices.end()) {
        continue;
      }
      m_selectedMaskIndices.push_back(index);
    }
  }

  // Legacy files may contain candidates but no separately saved primary
  // contour. Reconstruct it so extraction remains immediately available.
  if (m_detectedSubject.contour.empty() && m_selectedMaskIndex >= 0)
    m_detectedSubject.contour = m_maskCandidates[m_selectedMaskIndex].contour;
}
