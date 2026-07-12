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

ImagePrimitive::~ImagePrimitive() {
}

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

QRectF ImagePrimitive::boundingRect() const {
  return QRectF(m_position.x(), m_position.y(), m_size.x(), m_size.y());
}

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

void ImagePrimitive::translate(const QVector2D &offset) {
  qDebug() << "ImagePrimitive::translate: offset" << offset.x() << ","
           << offset.y() << "old pos:" << m_position.x() << ","
           << m_position.y();
  m_position += offset;
  qDebug() << "  new pos:" << m_position.x() << "," << m_position.y();
}

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

void ImagePrimitive::setControlPointPosition(int index,
                                             const QVector2D &position) {
  if (index >= 0 && index < 8) {
    resizeFromHandle(static_cast<ResizeHandle>(index), position);
  }
}

void ImagePrimitive::setImage(const QImage &image) {
  m_image = image;
}

void ImagePrimitive::setPosition(const QVector2D &position) {
  m_position = position;
}

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

void ImagePrimitive::setRotation(float rotation) { m_rotation = rotation; }

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

QVector2D ImagePrimitive::getHandlePosition(ResizeHandle handle) const {
  auto handles = getControlPoints();
  if (handle >= 0 && handle < 8) {
    return handles[handle];
  }
  return QVector2D();
}

QRectF ImagePrimitive::getResizeHandleRect(ResizeHandle handle) const {
  QVector2D pos = getHandlePosition(handle);
  return QRectF(pos.x() - HANDLE_SIZE / 2.0f, pos.y() - HANDLE_SIZE / 2.0f,
                HANDLE_SIZE, HANDLE_SIZE);
}

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

void ImagePrimitive::autoDetectSubject() {
  if (m_image.isNull() || m_detectionInProgress) {
    return;
  }

  // Defer SAM2 processing for faster initial loading
  // Subject detection will be triggered when user enters edit mode or after a
  // delay
  qDebug() << "ImagePrimitive: Image loaded, deferring subject detection for "
              "performance";

  // Start detection after a 2-second delay for better user experience
  QTimer::singleShot(2000, this, [this]() {
    if (!m_detectionInProgress && m_maskCandidates.empty()) {
      qDebug() << "ImagePrimitive: Auto-starting subject detection after delay";
      startSubjectDetection();
    }
  });
}

void ImagePrimitive::startSubjectDetection(int maxDimension) {
  if (m_image.isNull() || m_detectionInProgress) {
    return;
  }

  qDebug() << "ImagePrimitive: Starting subject detection...";
  emit detectionProgress(0, "Initializing mask detection...");

  // Check cache first
  MaskCache *cache = MaskCache::instance();
  if (cache && cache->isEnabled()) {
    auto cachedMasks = cache->getCachedMasks(m_image);
    if (!cachedMasks.empty()) {
      qDebug() << "ImagePrimitive: Loaded" << cachedMasks.size()
               << "masks from cache";
      emit detectionProgress(50, "Loading masks from cache...");

      // Convert cached masks to our format
      m_maskCandidates.clear();
      for (const auto &cached : cachedMasks) {
        MaskCandidate candidate;
        candidate.id = cached.id;
        candidate.contour = cached.contour;
        candidate.mask = cached.mask;
        candidate.score = cached.score;
        candidate.stability = cached.stability;
        candidate.predicted_iou = cached.predicted_iou;
        candidate.area_percent = cached.area_percent;
        m_maskCandidates.push_back(candidate);
      }

      // Select first candidate
      m_selectedMaskIndex = 0;
      selectMaskCandidate(0);

      emit detectionProgress(100, "Masks loaded from cache");
      emit detectionComplete();
      return;
    }
  }

  // Check if SAM2 client is available
  if (!m_sam2Client) {
    qWarning()
        << "ImagePrimitive: SAM2 client not available (extracted image?)";
    emit detectionFailed("SAM2 service not available");
    return;
  }

  m_detectionInProgress = true;
  emit detectionProgress(10, "Preparing image...");

  // Balanced image size for good accuracy and reasonable speed
  QImage imageToProcess = m_image;
  const int MAX_DIMENSION = maxDimension; // Configurable accuracy vs speed

  // If image has transparency (cutout), convert to RGB with white background
  // SAM2 works better with RGB images
  if (imageToProcess.hasAlphaChannel()) {
    qDebug()
        << "ImagePrimitive: Converting transparent image to RGB for detection";
    QImage rgbImage(imageToProcess.size(), QImage::Format_RGB888);
    rgbImage.fill(Qt::white); // White background

    QPainter painter(&rgbImage);
    painter.drawImage(0, 0, imageToProcess);
    painter.end();

    imageToProcess = rgbImage;
    qDebug() << "ImagePrimitive: Converted to RGB with white background";
  }

  if (m_image.width() > MAX_DIMENSION || m_image.height() > MAX_DIMENSION) {
    qDebug() << "ImagePrimitive: Resizing for balanced detection"
             << m_image.size() << "→";
    emit detectionProgress(20, "Resizing image...");
    // Smooth downscale preserves edges better for SAM2
    imageToProcess =
        imageToProcess.scaled(MAX_DIMENSION, MAX_DIMENSION, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation);
    qDebug() << "ImagePrimitive: Resized to" << imageToProcess.size()
             << "for balanced detection";

    // Store scale factors to convert detection coordinates back to original
    m_detectionScaleX =
        static_cast<float>(m_image.width()) / imageToProcess.width();
    m_detectionScaleY =
        static_cast<float>(m_image.height()) / imageToProcess.height();
    qDebug() << "ImagePrimitive: Detection scale factors:" << m_detectionScaleX
             << "x" << m_detectionScaleY;
  } else {
    // No resizing needed
    m_detectionScaleX = 1.0f;
    m_detectionScaleY = 1.0f;
  }

  // Use SAM2 for automatic detection
  emit detectionProgress(30, "Generating masks...");
  m_sam2Client->segmentImage(imageToProcess);
}

void ImagePrimitive::startHumanDetection() {
  if (m_image.isNull() || m_detectionInProgress) {
    return;
  }

  qDebug() << "ImagePrimitive: Starting HUMAN detection with YOLO + SAM2...";
  emit detectionProgress(0, "Initializing human detection...");

  // Check if SAM2 client is available
  if (!m_sam2Client) {
    qWarning()
        << "ImagePrimitive: SAM2 client not available (extracted image?)";
    emit detectionFailed("SAM2 service not available");
    return;
  }

  m_detectionInProgress = true;
  emit detectionProgress(10, "Preparing image...");

  // Higher resolution for more precise human boundaries
  QImage imageToProcess = m_image;
  const int MAX_DIMENSION = 1536;

  // If image has transparency (cutout), convert to RGB with white background
  // SAM2 works better with RGB images
  if (imageToProcess.hasAlphaChannel()) {
    qDebug()
        << "ImagePrimitive: Converting transparent image to RGB for detection";
    QImage rgbImage(imageToProcess.size(), QImage::Format_RGB888);
    rgbImage.fill(Qt::white); // White background

    QPainter painter(&rgbImage);
    painter.drawImage(0, 0, imageToProcess);
    painter.end();

    imageToProcess = rgbImage;
    qDebug() << "ImagePrimitive: Converted to RGB with white background";
  }

  if (m_image.width() > MAX_DIMENSION || m_image.height() > MAX_DIMENSION) {
    qDebug() << "ImagePrimitive: Resizing for human detection" << m_image.size()
             << "→";
    emit detectionProgress(20, "Resizing image...");
    imageToProcess =
        imageToProcess.scaled(MAX_DIMENSION, MAX_DIMENSION, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation);
    qDebug() << "ImagePrimitive: Resized to" << imageToProcess.size()
             << "for human detection";

    // Store scale factors to convert detection coordinates back to original
    m_detectionScaleX =
        static_cast<float>(m_image.width()) / imageToProcess.width();
    m_detectionScaleY =
        static_cast<float>(m_image.height()) / imageToProcess.height();
    qDebug() << "ImagePrimitive: Detection scale factors:" << m_detectionScaleX
             << "x" << m_detectionScaleY;
  } else {
    // No resizing needed
    m_detectionScaleX = 1.0f;
    m_detectionScaleY = 1.0f;
  }

  // Use YOLO + SAM2 for human detection
  emit detectionProgress(30, "Detecting humans with YOLO...");
  m_sam2Client->segmentHumans(imageToProcess);
}

void ImagePrimitive::setEditMode(bool enabled) {
  m_editMode = enabled;

  // Trigger subject detection when edit mode is first enabled
  if (enabled && !m_detectionInProgress && m_maskCandidates.empty()) {
    startSubjectDetection();
  }
}

std::unique_ptr<ImagePrimitive> ImagePrimitive::extractDetectedSubject() {
  // Prefer full-resolution pixel mask when available (precise cutout).
  // Fall back to contour polygon when the user has edited control points
  // (pixel mask is cleared on edit) or when the server did not send a mask.
  std::vector<QPointF> contourToUse;

  if (!m_detectedSubject.contour.empty()) {
    contourToUse = m_detectedSubject.contour;
    qDebug() << "ImagePrimitive: Using edited contour with"
             << contourToUse.size() << "points";
  } else {
    const MaskCandidate *candidate = getSelectedCandidate();
    if (!candidate || candidate->contour.empty()) {
      qDebug() << "ImagePrimitive: No mask selected for extraction";
      return nullptr;
    }
    contourToUse = candidate->contour;
    qDebug() << "ImagePrimitive: Using candidate contour with"
             << contourToUse.size() << "points";
  }

  qDebug() << "ImagePrimitive: Extracting subject with" << contourToUse.size()
           << "contour points";
  qDebug() << "ImagePrimitive: Mask inverted:" << m_maskInverted;

  // Apply spline interpolation if smoothness is enabled (contour path only)
  if (m_contourSmoothness > 0 && m_detectedSubject.mask.isNull()) {
    std::vector<QPointF> smoothContour;
    int n = contourToUse.size();
    for (int i = 0; i < n; ++i) {
      QPointF p0 = contourToUse[(i - 1 + n) % n];
      QPointF p1 = contourToUse[i];
      QPointF p2 = contourToUse[(i + 1) % n];
      QPointF p3 = contourToUse[(i + 2) % n];

      smoothContour.push_back(p1);

      int subdivisions = m_contourSmoothness;
      for (int j = 1; j <= subdivisions; ++j) {
        float t = static_cast<float>(j) / (subdivisions + 1);
        float t2 = t * t;
        float t3 = t2 * t;

        float x =
            0.5f *
            ((2.0f * p1.x()) + (-p0.x() + p2.x()) * t +
             (2.0f * p0.x() - 5.0f * p1.x() + 4.0f * p2.x() - p3.x()) * t2 +
             (-p0.x() + 3.0f * p1.x() - 3.0f * p2.x() + p3.x()) * t3);

        float y =
            0.5f *
            ((2.0f * p1.y()) + (-p0.y() + p2.y()) * t +
             (2.0f * p0.y() - 5.0f * p1.y() + 4.0f * p2.y() - p3.y()) * t2 +
             (-p0.y() + 3.0f * p1.y() - 3.0f * p2.y() + p3.y()) * t3);

        smoothContour.push_back(QPointF(x, y));
      }
    }
    contourToUse = smoothContour;
    qDebug() << "ImagePrimitive: Applied spline smoothing, now"
             << contourToUse.size() << "points";
  }

  // Create RGBA image with transparency
  QImage extractedImage = m_image.convertToFormat(QImage::Format_ARGB32);

  // Build alpha mask: prefer SAM2 pixel mask (precise), else polygon fill
  QImage maskImage(m_image.size(), QImage::Format_Grayscale8);

  if (!m_detectedSubject.mask.isNull() &&
      m_detectedSubject.mask.size() == m_image.size()) {
    qDebug() << "ImagePrimitive: Using full-resolution pixel mask for cutout";
    maskImage = m_detectedSubject.mask.convertToFormat(QImage::Format_Grayscale8);
    if (m_maskInverted) {
      for (int y = 0; y < maskImage.height(); ++y) {
        uchar *line = maskImage.scanLine(y);
        for (int x = 0; x < maskImage.width(); ++x) {
          line[x] = 255 - line[x];
        }
      }
    }
  } else {
    qDebug() << "ImagePrimitive: Falling back to contour polygon mask";
    maskImage.fill(m_maskInverted ? Qt::white : Qt::black);

    QPainter maskPainter(&maskImage);
    maskPainter.setRenderHint(QPainter::Antialiasing);
    maskPainter.setBrush(m_maskInverted ? Qt::black : Qt::white);
    maskPainter.setPen(Qt::NoPen);

    QPolygonF polygon;
    for (const auto &point : contourToUse) {
      polygon << point;
    }
    maskPainter.drawPolygon(polygon);
    maskPainter.end();
  }

  // Apply mask to create transparency
  for (int y = 0; y < extractedImage.height(); ++y) {
    for (int x = 0; x < extractedImage.width(); ++x) {
      QRgb pixel = extractedImage.pixel(x, y);
      int maskValue = qGray(maskImage.pixel(x, y));

      // Set alpha channel based on mask
      extractedImage.setPixel(
          x, y, qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), maskValue));
    }
  }

  // Apply mask refinements (feather, blur, expand) during extraction
  // These are set via contour smoothness which affects the visual quality
  if (m_maskExpand != 0 || m_maskBlur > 0 || m_maskFeather > 0) {
    qDebug() << "ImagePrimitive: Applying mask refinements - expand:"
             << m_maskExpand << "blur:" << m_maskBlur
             << "feather:" << m_maskFeather;

    // Expand/Contract
    if (m_maskExpand != 0) {
      for (int iter = 0; iter < abs(m_maskExpand); ++iter) {
        QImage temp = extractedImage.copy();
        for (int y = 0; y < extractedImage.height(); ++y) {
          for (int x = 0; x < extractedImage.width(); ++x) {
            int alpha = extractedImage.pixelColor(x, y).alpha();
            if (alpha == 0 && m_maskExpand < 0)
              continue;
            if (alpha == 255 && m_maskExpand > 0)
              continue;

            if (m_maskExpand > 0) {
              int maxAlpha = alpha;
              for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                  int nx = x + dx, ny = y + dy;
                  if (nx >= 0 && nx < extractedImage.width() && ny >= 0 &&
                      ny < extractedImage.height()) {
                    maxAlpha = qMax(maxAlpha,
                                    extractedImage.pixelColor(nx, ny).alpha());
                  }
                }
              }
              QColor c = temp.pixelColor(x, y);
              c.setAlpha(maxAlpha);
              temp.setPixelColor(x, y, c);
            } else {
              int minAlpha = alpha;
              for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                  int nx = x + dx, ny = y + dy;
                  if (nx >= 0 && nx < extractedImage.width() && ny >= 0 &&
                      ny < extractedImage.height()) {
                    minAlpha = qMin(minAlpha,
                                    extractedImage.pixelColor(nx, ny).alpha());
                  }
                }
              }
              QColor c = temp.pixelColor(x, y);
              c.setAlpha(minAlpha);
              temp.setPixelColor(x, y, c);
            }
          }
        }
        extractedImage = temp;
      }
    }

    // Blur
    if (m_maskBlur > 0) {
      for (int pass = 0; pass < 2; ++pass) {
        QImage temp = extractedImage.copy();
        int radius = qMin(m_maskBlur, 10);
        for (int y = 0; y < extractedImage.height(); ++y) {
          for (int x = 0; x < extractedImage.width(); ++x) {
            int alpha = extractedImage.pixelColor(x, y).alpha();
            if (alpha == 0 || alpha == 255)
              continue;

            int sum = 0, count = 0;
            for (int dy = -radius; dy <= radius; ++dy) {
              for (int dx = -radius; dx <= radius; ++dx) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && nx < extractedImage.width() && ny >= 0 &&
                    ny < extractedImage.height()) {
                  sum += extractedImage.pixelColor(nx, ny).alpha();
                  count++;
                }
              }
            }
            if (count > 0) {
              QColor c = temp.pixelColor(x, y);
              c.setAlpha(sum / count);
              temp.setPixelColor(x, y, c);
            }
          }
        }
        extractedImage = temp;
      }
    }

    // Feather (progressive blur)
    if (m_maskFeather > 0) {
      int passes = qMin(m_maskFeather / 3, 10);
      for (int p = 0; p < passes; ++p) {
        QImage temp = extractedImage.copy();
        for (int y = 0; y < extractedImage.height(); ++y) {
          for (int x = 0; x < extractedImage.width(); ++x) {
            int alpha = extractedImage.pixelColor(x, y).alpha();
            if (alpha == 0 || alpha == 255)
              continue;

            int sum = 0, count = 0;
            for (int dy = -3; dy <= 3; ++dy) {
              for (int dx = -3; dx <= 3; ++dx) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && nx < extractedImage.width() && ny >= 0 &&
                    ny < extractedImage.height()) {
                  sum += extractedImage.pixelColor(nx, ny).alpha();
                  count++;
                }
              }
            }
            if (count > 0) {
              QColor c = temp.pixelColor(x, y);
              c.setAlpha(sum / count);
              temp.setPixelColor(x, y, c);
            }
          }
        }
        extractedImage = temp;
      }
    }
  }

  // Find bounding box of non-transparent pixels
  int minX = extractedImage.width(), minY = extractedImage.height();
  int maxX = 0, maxY = 0;
  bool foundPixels = false;

  for (int y = 0; y < extractedImage.height(); ++y) {
    for (int x = 0; x < extractedImage.width(); ++x) {
      if (qAlpha(extractedImage.pixel(x, y)) > 0) {
        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x);
        maxY = qMax(maxY, y);
        foundPixels = true;
      }
    }
  }

  // Check if we found any valid pixels
  if (!foundPixels || minX > maxX || minY > maxY) {
    qWarning()
        << "ImagePrimitive: No valid pixels found in mask, cannot extract";
    return nullptr;
  }

  // Crop to bounding box
  QRect boundingBox(minX, minY, maxX - minX + 1, maxY - minY + 1);

  // Validate bounding box
  if (!boundingBox.isValid() || boundingBox.isEmpty()) {
    qWarning() << "ImagePrimitive: Invalid bounding box" << boundingBox;
    return nullptr;
  }

  QImage croppedImage = extractedImage.copy(boundingBox);

  // Validate cropped image
  if (croppedImage.isNull() || croppedImage.width() == 0 ||
      croppedImage.height() == 0) {
    qWarning() << "ImagePrimitive: Failed to crop image";
    return nullptr;
  }

  // Calculate position and size in world coordinates
  float scaleX = m_size.x() / m_image.width();
  float scaleY = m_size.y() / m_image.height();

  // Calculate extracted size
  QVector2D extractedSize(boundingBox.width() * scaleX,
                          boundingBox.height() * scaleY);

  // Simple approach: just place at the bounding box location
  // No rotation inheritance to avoid complexity
  QVector2D extractedPos = m_position + QVector2D(boundingBox.x() * scaleX,
                                                  boundingBox.y() * scaleY);

  qDebug() << "ImagePrimitive: Extraction:"
           << "\n  Parent pos:" << m_position.x() << "," << m_position.y()
           << "\n  Parent size:" << m_size.x() << "x" << m_size.y()
           << "\n  BBox:" << boundingBox << "\n  Scale:" << scaleX << "x"
           << scaleY << "\n  Extracted pos:" << extractedPos.x() << ","
           << extractedPos.y() << "\n  Extracted size:" << extractedSize.x()
           << "x" << extractedSize.y();

  // Create extracted image without rotation
  auto extracted = std::make_unique<ImagePrimitive>(croppedImage, extractedPos,
                                                    extractedSize);

  // Clear any mask/detection data from extracted object (it's already cut out)
  // But keep SAM2 client so user can detect masks on the cutout later
  extracted->m_detectedSubject.contour.clear();
  extracted->m_detectedSubject.extractedImage = QImage();
  extracted->m_detectedSubject.mask = QImage();
  extracted->m_detectedSubject.boundingBox = QRectF();
  extracted->m_maskCandidates.clear();
  extracted->m_selectedMaskIndex = -1;
  extracted->m_detectionInProgress = false;
  extracted->m_editMode = false;

  // SAM2 client is kept alive for manual detection on cutouts

  qDebug() << "ImagePrimitive: Extracted subject at" << extractedPos.x() << ","
           << extractedPos.y() << "size" << extractedSize.x() << "x"
           << extractedSize.y() << "with transparency";

  return extracted;
}

std::vector<std::unique_ptr<ImagePrimitive>>
ImagePrimitive::extractAllMaskCandidates() {
  std::vector<std::unique_ptr<ImagePrimitive>> extractedImages;

  if (m_maskCandidates.empty()) {
    qDebug() << "ImagePrimitive: No mask candidates to extract";
    return extractedImages;
  }

  qDebug() << "ImagePrimitive: Extracting all" << m_maskCandidates.size()
           << "mask candidates";

  // Save current selection
  int originalSelection = m_selectedMaskIndex;
  std::vector<QPointF> originalContour = m_detectedSubject.contour;
  QImage originalMask = m_detectedSubject.mask;

  // Extract each candidate
  for (int i = 0; i < static_cast<int>(m_maskCandidates.size()); ++i) {
    // Temporarily set this candidate as the current one
    m_detectedSubject.contour = m_maskCandidates[i].contour;
    m_detectedSubject.mask = m_maskCandidates[i].mask;

    // Extract it
    auto extracted = extractDetectedSubject();
    if (extracted) {
      extractedImages.push_back(std::move(extracted));
      qDebug() << "  Extracted candidate" << i << "with"
               << m_maskCandidates[i].contour.size() << "points"
               << (m_maskCandidates[i].mask.isNull() ? "(polygon)" : "(pixel mask)");
    }
  }

  // Restore original selection
  m_selectedMaskIndex = originalSelection;
  m_detectedSubject.contour = originalContour;
  m_detectedSubject.mask = originalMask;

  qDebug() << "ImagePrimitive: Successfully extracted" << extractedImages.size()
           << "subjects";
  return extractedImages;
}

void ImagePrimitive::onSAM2SegmentationComplete(
    const SAM2Client::SegmentationResult &result) {
  m_detectionInProgress = false;

  // If we already have mask candidates from multi-segmentation, skip this
  // (multi-segmentation is called first and handles everything)
  if (!m_maskCandidates.empty()) {
    qDebug() << "ImagePrimitive: Skipping single result (already have"
             << m_maskCandidates.size() << "candidates)";
    return;
  }

  if (result.mask.isNull() || result.contour.empty()) {
    qDebug() << "ImagePrimitive: SAM2 segmentation returned empty result";
    return;
  }

  qDebug() << "ImagePrimitive: SAM2 segmentation complete (single result), "
              "contour points:"
           << result.contour.size();
  qDebug() << "ImagePrimitive: Mask size:" << result.mask.size()
           << "Original image size:" << m_image.size();

  // Scale contour from mask coordinates to original image coordinates
  float scaleX = static_cast<float>(m_image.width()) / result.mask.width();
  float scaleY = static_cast<float>(m_image.height()) / result.mask.height();

  qDebug() << "ImagePrimitive: Scaling contour by X=" << scaleX
           << "Y=" << scaleY;

  std::vector<QPointF> scaledContour;
  scaledContour.reserve(result.contour.size());
  for (const auto &point : result.contour) {
    scaledContour.push_back(QPointF(point.x() * scaleX, point.y() * scaleY));
  }

  // Calculate bounding box from scaled contour
  QRectF boundingBox;
  if (!scaledContour.empty()) {
    float minX = scaledContour[0].x();
    float maxX = scaledContour[0].x();
    float minY = scaledContour[0].y();
    float maxY = scaledContour[0].y();

    for (const auto &point : scaledContour) {
      minX = std::min(minX, static_cast<float>(point.x()));
      maxX = std::max(maxX, static_cast<float>(point.x()));
      minY = std::min(minY, static_cast<float>(point.y()));
      maxY = std::max(maxY, static_cast<float>(point.y()));
    }

    boundingBox = QRectF(minX, minY, maxX - minX, maxY - minY);
  }

  // Store the detected subject with scaled contour + full-res mask
  if (!result.mask.isNull() && result.mask.size() != m_image.size()) {
    m_detectedSubject.mask = result.mask.scaled(
        m_image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    for (int y = 0; y < m_detectedSubject.mask.height(); ++y) {
      uchar *line = m_detectedSubject.mask.scanLine(y);
      for (int x = 0; x < m_detectedSubject.mask.width(); ++x) {
        line[x] = line[x] >= 128 ? 255 : 0;
      }
    }
  } else {
    m_detectedSubject.mask = result.mask;
  }
  m_detectedSubject.contour = scaledContour;
  m_detectedSubject.boundingBox = boundingBox;

  // Extract the subject image using the mask
  QImage extractedImage = m_image.copy();
  if (extractedImage.format() != QImage::Format_ARGB32) {
    extractedImage = extractedImage.convertToFormat(QImage::Format_ARGB32);
  }

  // Apply the full-resolution mask to create transparency
  const QImage &fullMask = m_detectedSubject.mask;
  for (int y = 0; y < extractedImage.height(); ++y) {
    for (int x = 0; x < extractedImage.width(); ++x) {
      if (!fullMask.isNull() && x < fullMask.width() && y < fullMask.height()) {
        int maskValue = qGray(fullMask.pixel(x, y));
        QColor color = extractedImage.pixelColor(x, y);
        color.setAlpha(maskValue);
        extractedImage.setPixelColor(x, y, color);
      } else {
        extractedImage.setPixelColor(x, y, Qt::transparent);
      }
    }
  }

  m_detectedSubject.extractedImage = extractedImage;

  // Create a mask candidate for the mask editing UI
  MaskCandidate candidate;
  candidate.id = 0;
  candidate.score = result.confidence;
  candidate.stability = result.confidence; // Use confidence as stability
  candidate.predicted_iou = result.confidence;
  // Calculate approximate area percentage
  QPolygonF polygon;
  for (const auto &point : scaledContour) {
    polygon << point;
  }
  QRectF boundingRect = polygon.boundingRect();
  float totalImageArea = m_image.width() * m_image.height();
  float boundingRectArea = boundingRect.width() * boundingRect.height();
  candidate.area_percent = (boundingRectArea / totalImageArea) * 100.0f;
  candidate.contour = scaledContour;

  // Add to mask candidates list so editing UI appears
  m_maskCandidates.clear();
  m_maskCandidates.push_back(candidate);
  m_selectedMaskIndex = 0;

  qDebug()
      << "ImagePrimitive: Subject detection complete and ready for extraction";
  qDebug() << "ImagePrimitive: Green outline should now be visible on canvas";
  qDebug() << "ImagePrimitive: Created mask candidate for editing UI";

  // Emit signal to trigger canvas update
  emit detectionComplete();
}

void ImagePrimitive::onSAM2SegmentationFailed(const QString &error) {
  m_detectionInProgress = false;
  qDebug() << "ImagePrimitive: SAM2 segmentation failed:" << error;

  // Emit signal
  emit detectionFailed(error);
}

void ImagePrimitive::onSAM2MultiSegmentationComplete(
    const SAM2Client::MultiSegmentationResult &result) {
  m_detectionInProgress = false;

  if (!result.success || result.candidates.empty()) {
    qDebug() << "ImagePrimitive: No mask candidates returned";
    emit detectionFailed("No mask candidates found");
    return;
  }

  qDebug() << "ImagePrimitive: Received" << result.candidates.size()
           << "mask candidates";
  emit detectionProgress(
      60, QString("Processing %1 masks...").arg(result.candidates.size()));

  // Store all candidates
  m_maskCandidates.clear();
  int processed = 0;
  for (const auto &candidate : result.candidates) {
    MaskCandidate mc;
    mc.id = candidate.id;
    mc.score = candidate.score;
    mc.stability = candidate.stability;
    mc.predicted_iou = candidate.predicted_iou;
    mc.area_percent = candidate.area_percent;

    // Scale contours from detection image coordinates to original image
    // coordinates
    for (const auto &point : candidate.contour) {
      QPointF scaledPoint(point.x() * m_detectionScaleX,
                          point.y() * m_detectionScaleY);
      mc.contour.push_back(scaledPoint);
    }

    // Upscale pixel mask to full image resolution for precise cutouts
    if (!candidate.mask.isNull()) {
      if (candidate.mask.size() != m_image.size()) {
        mc.mask = candidate.mask.scaled(m_image.size(), Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
        // Re-binarize after smooth upscale so soft edges stay usable as alpha
        for (int y = 0; y < mc.mask.height(); ++y) {
          uchar *line = mc.mask.scanLine(y);
          for (int x = 0; x < mc.mask.width(); ++x) {
            line[x] = line[x] >= 128 ? 255 : 0;
          }
        }
      } else {
        mc.mask = candidate.mask;
      }
    }

    // Debug: Check coordinate ranges
    if (!candidate.contour.empty()) {
      float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
      for (const auto &p : candidate.contour) {
        if (p.x() < minX)
          minX = p.x();
        if (p.x() > maxX)
          maxX = p.x();
        if (p.y() < minY)
          minY = p.y();
        if (p.y() > maxY)
          maxY = p.y();
      }
      qDebug() << "Candidate" << candidate.id << "contour bounds:"
               << "X:" << minX << "-" << maxX << "Y:" << minY << "-" << maxY
               << "Image size:" << m_image.width() << "x" << m_image.height()
               << "mask:" << (!mc.mask.isNull() ? "yes" : "no");
    }

    m_maskCandidates.push_back(mc);
    Q_UNUSED(processed);
  }

  // Cache the results
  MaskCache *cache = MaskCache::instance();
  if (cache && cache->isEnabled()) {
    std::vector<MaskCache::CachedMask> cachedMasks;
    for (const auto &mc : m_maskCandidates) {
      MaskCache::CachedMask cached;
      cached.id = mc.id;
      cached.contour = mc.contour;
      cached.score = mc.score;
      cached.stability = mc.stability;
      cached.predicted_iou = mc.predicted_iou;
      cached.area_percent = mc.area_percent;
      cached.imageWidth = m_image.width();
      cached.imageHeight = m_image.height();
      cached.mask = mc.mask;
      cachedMasks.push_back(cached);
    }
    cache->cacheMasks(m_image, cachedMasks);
    qDebug() << "ImagePrimitive: Cached" << cachedMasks.size()
             << "masks for future use";
  }

  emit detectionProgress(90, "Finalizing masks...");

  // Select the first (best) candidate by default
  m_selectedMaskIndex = 0;
  selectMaskCandidate(0);

  emit detectionProgress(100, "Mask detection complete!");
  emit detectionComplete();
}

void ImagePrimitive::applyMaskCandidateAsPrimary(int index)
{
  if (index < 0 || index >= static_cast<int>(m_maskCandidates.size()))
    return;

  m_selectedMaskIndex = index;
  m_maskInverted = false;
  const MaskCandidate &selected = m_maskCandidates[index];
  m_detectedSubject.contour = selected.contour;
  m_detectedSubject.mask = selected.mask;

  if (!selected.contour.empty()) {
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto &point : selected.contour) {
      minX = std::min(minX, static_cast<float>(point.x()));
      maxX = std::max(maxX, static_cast<float>(point.x()));
      minY = std::min(minY, static_cast<float>(point.y()));
      maxY = std::max(maxY, static_cast<float>(point.y()));
    }
    m_detectedSubject.boundingBox =
        QRectF(minX, minY, maxX - minX, maxY - minY);
  }
}

bool ImagePrimitive::isMaskCandidateSelected(int index) const
{
  return std::find(m_selectedMaskIndices.begin(), m_selectedMaskIndices.end(),
                   index) != m_selectedMaskIndices.end();
}

void ImagePrimitive::clearMaskMultiSelection()
{
  m_selectedMaskIndices.clear();
}

void ImagePrimitive::selectMaskCandidate(int index)
{
  if (index < 0 || index >= static_cast<int>(m_maskCandidates.size())) {
    qDebug() << "ImagePrimitive: Invalid mask index:" << index;
    return;
  }
  // Replace multi-selection with this single mask (Next/Prev)
  m_selectedMaskIndices.clear();
  m_selectedMaskIndices.push_back(index);
  applyMaskCandidateAsPrimary(index);
  emit detectionComplete();
}

void ImagePrimitive::addMaskCandidateToSelection(int index)
{
  if (index < 0 || index >= static_cast<int>(m_maskCandidates.size()))
    return;
  if (!isMaskCandidateSelected(index))
    m_selectedMaskIndices.push_back(index);
  applyMaskCandidateAsPrimary(index);
  emit detectionComplete();
}

std::unique_ptr<ImagePrimitive>
ImagePrimitive::extractMaskCandidateAt(int index)
{
  if (index < 0 || index >= static_cast<int>(m_maskCandidates.size()))
    return nullptr;

  DetectedSubject savedSubject = m_detectedSubject;
  const int savedIdx = m_selectedMaskIndex;
  const auto savedMulti = m_selectedMaskIndices;

  applyMaskCandidateAsPrimary(index);
  auto extracted = extractDetectedSubject();

  m_detectedSubject = savedSubject;
  m_selectedMaskIndex = savedIdx;
  m_selectedMaskIndices = savedMulti;
  return extracted;
}

std::unique_ptr<ImagePrimitive>
ImagePrimitive::extractCombinedSelectedMasks()
{
  std::vector<int> idxs = m_selectedMaskIndices;
  if (idxs.empty() && m_selectedMaskIndex >= 0)
    idxs.push_back(m_selectedMaskIndex);
  if (idxs.empty() || m_image.isNull())
    return nullptr;
  if (idxs.size() == 1)
    return extractMaskCandidateAt(idxs.front());

  // Paint all selected masks onto a full-size transparent plate so relative
  // positions match the original photo.
  QImage combined(m_image.size(), QImage::Format_ARGB32);
  combined.fill(qRgba(0, 0, 0, 0));

  const QImage src = m_image.convertToFormat(QImage::Format_ARGB32);
  for (int idx : idxs) {
    if (idx < 0 || idx >= static_cast<int>(m_maskCandidates.size()))
      continue;
    const MaskCandidate &cand = m_maskCandidates[idx];

    QImage maskImg;
    if (!cand.mask.isNull() && cand.mask.size() == m_image.size()) {
      maskImg = cand.mask.convertToFormat(QImage::Format_Grayscale8);
    } else if (!cand.contour.empty()) {
      maskImg = QImage(m_image.size(), QImage::Format_Grayscale8);
      maskImg.fill(0);
      QPainter mp(&maskImg);
      mp.setRenderHint(QPainter::Antialiasing, true);
      mp.setPen(Qt::NoPen);
      mp.setBrush(Qt::white);
      QPolygonF poly;
      for (const auto &pt : cand.contour)
        poly << pt;
      mp.drawPolygon(poly);
      mp.end();
    } else {
      continue;
    }

    for (int y = 0; y < combined.height(); ++y) {
      QRgb *dst = reinterpret_cast<QRgb *>(combined.scanLine(y));
      const QRgb *s = reinterpret_cast<const QRgb *>(src.constScanLine(y));
      const uchar *m = maskImg.constScanLine(y);
      for (int x = 0; x < combined.width(); ++x) {
        const int a = m[x];
        if (a < 8)
          continue;
        // Keep strongest alpha if masks overlap
        if (a > qAlpha(dst[x]))
          dst[x] = qRgba(qRed(s[x]), qGreen(s[x]), qBlue(s[x]), a);
      }
    }
  }

  // Crop to union of opaque pixels
  int minX = combined.width(), minY = combined.height(), maxX = -1, maxY = -1;
  for (int y = 0; y < combined.height(); ++y) {
    const QRgb *line = reinterpret_cast<const QRgb *>(combined.constScanLine(y));
    for (int x = 0; x < combined.width(); ++x) {
      if (qAlpha(line[x]) > 0) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
      }
    }
  }
  if (maxX < minX || maxY < minY)
    return nullptr;

  const QRect bbox(minX, minY, maxX - minX + 1, maxY - minY + 1);
  QImage cropped = combined.copy(bbox);
  if (cropped.isNull())
    return nullptr;

  const float scaleX = m_size.x() / m_image.width();
  const float scaleY = m_size.y() / m_image.height();
  QVector2D extractedSize(bbox.width() * scaleX, bbox.height() * scaleY);
  QVector2D extractedPos =
      m_position + QVector2D(bbox.x() * scaleX, bbox.y() * scaleY);

  return std::make_unique<ImagePrimitive>(cropped, extractedPos, extractedSize);
}

QRect ImagePrimitive::maskCandidatePixelBounds(int index) const
{
  if (index < 0 || index >= static_cast<int>(m_maskCandidates.size()) ||
      m_image.isNull())
    return {};

  const MaskCandidate &cand = m_maskCandidates[index];
  QImage maskImg;
  if (!cand.mask.isNull() && cand.mask.size() == m_image.size()) {
    maskImg = cand.mask.convertToFormat(QImage::Format_Grayscale8);
  } else if (!cand.contour.empty()) {
    maskImg = QImage(m_image.size(), QImage::Format_Grayscale8);
    maskImg.fill(0);
    QPainter mp(&maskImg);
    mp.setRenderHint(QPainter::Antialiasing, true);
    mp.setPen(Qt::NoPen);
    mp.setBrush(Qt::white);
    QPolygonF poly;
    for (const auto &pt : cand.contour)
      poly << pt;
    mp.drawPolygon(poly);
    mp.end();
  } else {
    return {};
  }

  int minX = maskImg.width(), minY = maskImg.height(), maxX = -1, maxY = -1;
  for (int y = 0; y < maskImg.height(); ++y) {
    const uchar *line = maskImg.constScanLine(y);
    for (int x = 0; x < maskImg.width(); ++x) {
      if (line[x] > 8) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
      }
    }
  }
  if (maxX < minX || maxY < minY)
    return {};
  return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

QVector<ImagePrimitive::ExtractedMaskCutout>
ImagePrimitive::extractSelectedMasksSeparately()
{
  QVector<ExtractedMaskCutout> out;
  std::vector<int> idxs = m_selectedMaskIndices;
  if (idxs.empty() && m_selectedMaskIndex >= 0)
    idxs.push_back(m_selectedMaskIndex);
  if (idxs.empty())
    idxs.push_back(0);

  const float invW = m_image.width() > 0 ? 1.0f / m_image.width() : 0.0f;
  const float invH = m_image.height() > 0 ? 1.0f / m_image.height() : 0.0f;

  for (int idx : idxs) {
    auto extracted = extractMaskCandidateAt(idx);
    if (!extracted || extracted->image().isNull())
      continue;
    ExtractedMaskCutout piece;
    piece.image = extracted->image();
    const QRect bb = maskCandidatePixelBounds(idx);
    if (bb.isValid()) {
      piece.sourceNormRect = QRectF(bb.x() * invW, bb.y() * invH,
                                    bb.width() * invW, bb.height() * invH);
    }
    out.append(piece);
  }
  return out;
}

const ImagePrimitive::MaskCandidate *
ImagePrimitive::getSelectedCandidate() const {
  if (m_selectedMaskIndex >= 0 &&
      m_selectedMaskIndex < static_cast<int>(m_maskCandidates.size())) {
    return &m_maskCandidates[m_selectedMaskIndex];
  }
  return nullptr;
}

const ImagePrimitive::MaskCandidate *
ImagePrimitive::getCandidateAt(int index) const {
  if (index >= 0 && index < static_cast<int>(m_maskCandidates.size())) {
    return &m_maskCandidates[index];
  }
  return nullptr;
}

int ImagePrimitive::getMaskIndexAt(const QVector2D &point,
                                   bool preferUnselected) const {
  if (m_maskCandidates.empty() || m_image.isNull())
    return -1;

  QVector2D local = point - m_position;
  if (std::abs(m_rotation) > 0.0001f) {
    QVector2D center = m_position + (m_size * 0.5f);
    QVector2D p = point - center;
    float rad = -m_rotation * (static_cast<float>(M_PI) / 180.0f);
    float c = std::cos(rad);
    float s = std::sin(rad);
    QVector2D pr(p.x() * c - p.y() * s, p.x() * s + p.y() * c);
    local = (pr + center) - m_position;
  }

  const float scaleX =
      (m_image.width() > 0) ? (m_size.x() / m_image.width()) : 1.0f;
  const float scaleY =
      (m_image.height() > 0) ? (m_size.y() / m_image.height()) : 1.0f;
  if (scaleX == 0.0f || scaleY == 0.0f)
    return -1;

  const QPointF imgPt(local.x() / scaleX, (m_size.y() - local.y()) / scaleY);
  if (imgPt.x() < 0 || imgPt.y() < 0 || imgPt.x() > m_image.width() ||
      imgPt.y() > m_image.height()) {
    return -1;
  }

  int bestAny = -1;
  int bestUnselected = -1;
  float bestAnyArea = std::numeric_limits<float>::max();
  float bestUnselectedArea = std::numeric_limits<float>::max();

  for (int i = 0; i < static_cast<int>(m_maskCandidates.size()); ++i) {
    const auto &cand = m_maskCandidates[i];
    bool hit = false;
    if (!cand.mask.isNull() && cand.mask.size() == m_image.size()) {
      const int ix = qBound(0, static_cast<int>(imgPt.x()), m_image.width() - 1);
      const int iy = qBound(0, static_cast<int>(imgPt.y()), m_image.height() - 1);
      hit = qGray(cand.mask.pixel(ix, iy)) > 128;
    } else if (!cand.contour.empty()) {
      QPolygonF poly;
      for (const auto &pt : cand.contour)
        poly << pt;
      QPainterPath path;
      path.addPolygon(poly);
      hit = path.contains(imgPt);
    }
    if (!hit)
      continue;

    const float area =
        cand.area_percent > 0.0f ? cand.area_percent : 100.0f;
    if (area < bestAnyArea) {
      bestAnyArea = area;
      bestAny = i;
    }
    if (!isMaskCandidateSelected(i) && area < bestUnselectedArea) {
      bestUnselectedArea = area;
      bestUnselected = i;
    }
  }

  if (preferUnselected && bestUnselected >= 0)
    return bestUnselected;
  return bestAny;
}

QVector2D ImagePrimitive::imageToWorld(const QPointF &imagePoint) const {
  const float scaleX =
      (m_image.width() > 0) ? (m_size.x() / m_image.width()) : 1.0f;
  const float scaleY =
      (m_image.height() > 0) ? (m_size.y() / m_image.height()) : 1.0f;
  // Inverse of worldToImage: image space is Y-down, world contour space is
  // Y-flipped relative to the primitive origin.
  return QVector2D(m_position.x() + static_cast<float>(imagePoint.x()) * scaleX,
                   m_position.y() + m_size.y() -
                       static_cast<float>(imagePoint.y()) * scaleY);
}

QPointF ImagePrimitive::worldToImage(const QVector2D &worldPoint) const {
  const float scaleX =
      (m_image.width() > 0) ? (m_size.x() / m_image.width()) : 1.0f;
  const float scaleY =
      (m_image.height() > 0) ? (m_size.y() / m_image.height()) : 1.0f;
  const QVector2D local = worldPoint - m_position;
  const float imageX = (scaleX != 0.0f) ? (local.x() / scaleX) : 0.0f;
  const float imageY =
      (scaleY != 0.0f) ? ((m_size.y() - local.y()) / scaleY) : 0.0f;
  return QPointF(imageX, imageY);
}

void ImagePrimitive::invertMask() {
  if (m_detectedSubject.contour.empty()) {
    qWarning() << "ImagePrimitive: Cannot invert mask - no contour available";
    return;
  }

  // Toggle inversion flag
  m_maskInverted = !m_maskInverted;

  qDebug() << "ImagePrimitive: Mask inversion toggled -"
           << (m_maskInverted ? "selecting background" : "selecting subject");

  // Clear cached data so extraction will use new inversion state
  m_detectedSubject.extractedImage = QImage();
  m_detectedSubject.mask = QImage();
}

void ImagePrimitive::fillMaskedArea(const QColor &color) {
  if (m_detectedSubject.contour.empty() && m_detectedSubject.mask.isNull()) {
    qWarning() << "ImagePrimitive: Cannot fill - no mask available";
    return;
  }

  qDebug() << "ImagePrimitive: Filling masked area with color" << color.name();
  qDebug() << "ImagePrimitive: Mask inverted:" << m_maskInverted;

  QImage maskImage(m_image.size(), QImage::Format_Grayscale8);

  if (!m_detectedSubject.mask.isNull() &&
      m_detectedSubject.mask.size() == m_image.size()) {
    maskImage = m_detectedSubject.mask.convertToFormat(QImage::Format_Grayscale8);
    if (m_maskInverted) {
      for (int y = 0; y < maskImage.height(); ++y) {
        uchar *line = maskImage.scanLine(y);
        for (int x = 0; x < maskImage.width(); ++x) {
          line[x] = 255 - line[x];
        }
      }
    }
  } else {
    maskImage.fill(m_maskInverted ? Qt::white : Qt::black);

    QPainter maskPainter(&maskImage);
    maskPainter.setRenderHint(QPainter::Antialiasing);
    maskPainter.setBrush(m_maskInverted ? Qt::black : Qt::white);
    maskPainter.setPen(Qt::NoPen);

    QPolygonF polygon;
    for (const auto &point : m_detectedSubject.contour) {
      polygon << point;
    }
    maskPainter.drawPolygon(polygon);
    maskPainter.end();
  }

  // Apply the fill color to the masked area
  QImage newImage = m_image.copy();
  for (int y = 0; y < newImage.height(); ++y) {
    for (int x = 0; x < newImage.width(); ++x) {
      int maskValue = qGray(maskImage.pixel(x, y));
      if (maskValue > 128) { // Pixel is in the masked area
        newImage.setPixelColor(x, y, color);
      }
    }
  }

  // Update the image
  m_image = newImage;

  qDebug() << "ImagePrimitive: Filled"
           << (m_maskInverted ? "background" : "subject") << "area with color";
}

// Mask editing implementation
int ImagePrimitive::getControlPointAt(const QVector2D &point,
                                      float tolerance) const {
  if (!m_editMode || m_detectedSubject.contour.empty()) {
    return -1;
  }

  // Scale contour to match current display size
  float scaleX = m_size.x() / m_image.width();
  float scaleY = m_size.y() / m_image.height();

  // Convert point to local coordinates
  QVector2D localPoint = point - m_position;

  // Find the CLOSEST control point within tolerance
  float effectiveTolerance = 15.0f; // Smaller, more precise tolerance

  int closestIndex = -1;
  float closestDist = effectiveTolerance;

  for (int i = 0; i < static_cast<int>(m_detectedSubject.contour.size()); ++i) {
    const QPointF &cp = m_detectedSubject.contour[i];

    // Scale control point to display coordinates AND flip Y
    float cpX = cp.x() * scaleX;
    float cpY = m_size.y() - (cp.y() * scaleY);

    float dx = cpX - localPoint.x();
    float dy = cpY - localPoint.y();
    float dist = std::sqrt(dx * dx + dy * dy);

    // Keep track of the closest point
    if (dist < closestDist) {
      closestDist = dist;
      closestIndex = i;
    }
  }

  return closestIndex;
}

void ImagePrimitive::moveControlPoint(int index, const QVector2D &position) {
  if (index < 0 ||
      index >= static_cast<int>(m_detectedSubject.contour.size())) {
    return;
  }

  // Convert from world coordinates to image coordinates
  // Account for position offset and Y-flip (rendering flips Y)
  float scaleX = m_size.x() / m_image.width();
  float scaleY = m_size.y() / m_image.height();

  QVector2D localPoint = position - m_position;

  // Flip Y back (rendering uses: y_flipped = m_size.y() - (point.y() * scaleY))
  // So to reverse: point.y() = (m_size.y() - y_flipped) / scaleY
  float imageX = localPoint.x() / scaleX;
  float imageY = (m_size.y() - localPoint.y()) / scaleY;

  // Clamp coordinates to stay within image bounds
  imageX = qMax(0.0f, qMin(static_cast<float>(m_image.width()), imageX));
  imageY = qMax(0.0f, qMin(static_cast<float>(m_image.height()), imageY));

  m_detectedSubject.contour[index] = QPointF(imageX, imageY);

  // Manual contour edit invalidates the pixel mask — cutout will use polygon
  m_detectedSubject.mask = QImage();

  emit maskModified();
}

void ImagePrimitive::insertControlPoint(int afterIndex,
                                        const QVector2D &position) {
  if (afterIndex < -1 ||
      afterIndex >= static_cast<int>(m_detectedSubject.contour.size())) {
    return;
  }

  // Convert world coordinates to image coordinates
  float scaleX = m_size.x() / m_image.width();
  float scaleY = m_size.y() / m_image.height();

  // World to image space
  QVector2D relativePos = position - m_position;
  float imageX = relativePos.x() / scaleX;
  float imageY = (m_size.y() - relativePos.y()) / scaleY; // Y-flip

  // Clamp coordinates to stay within image bounds
  imageX = qMax(0.0f, qMin(static_cast<float>(m_image.width()), imageX));
  imageY = qMax(0.0f, qMin(static_cast<float>(m_image.height()), imageY));

  // Insert after the specified index
  auto it = m_detectedSubject.contour.begin() + afterIndex + 1;
  m_detectedSubject.contour.insert(it, QPointF(imageX, imageY));

  m_detectedSubject.mask = QImage();

  emit maskModified();

  qDebug() << "ImagePrimitive: Inserted control point after" << afterIndex
           << "at" << imageX << "," << imageY;
}

int ImagePrimitive::getNearestContourSegment(const QVector2D &point,
                                             float tolerance) const {
  qDebug() << "getNearestContourSegment called with tolerance:" << tolerance;
  qDebug() << "Contour size:" << m_detectedSubject.contour.size();

  if (m_detectedSubject.contour.size() < 2) {
    qDebug() << "Contour too small, returning -1";
    return -1;
  }

  float scaleX = m_size.x() / m_image.width();
  float scaleY = m_size.y() / m_image.height();

  int nearestSegment = -1;
  float minDistance = tolerance;

  qDebug() << "Checking" << m_detectedSubject.contour.size() << "segments";

  for (size_t i = 0; i < m_detectedSubject.contour.size(); ++i) {
    size_t nextI = (i + 1) % m_detectedSubject.contour.size();

    // Transform contour points to world coordinates
    QVector2D p1(m_position.x() + m_detectedSubject.contour[i].x() * scaleX,
                 m_position.y() +
                     (m_size.y() - m_detectedSubject.contour[i].y() * scaleY));
    QVector2D p2(
        m_position.x() + m_detectedSubject.contour[nextI].x() * scaleX,
        m_position.y() +
            (m_size.y() - m_detectedSubject.contour[nextI].y() * scaleY));

    // Calculate distance from point to line segment
    QVector2D v = p2 - p1;
    QVector2D w = point - p1;

    float c1 = QVector2D::dotProduct(w, v);
    if (c1 <= 0) {
      float dist = (point - p1).length();
      if (dist < minDistance) {
        minDistance = dist;
        nearestSegment = i;
      }
      continue;
    }

    float c2 = QVector2D::dotProduct(v, v);
    if (c1 >= c2) {
      float dist = (point - p2).length();
      if (dist < minDistance) {
        minDistance = dist;
        nearestSegment = i;
      }
      continue;
    }

    float b = c1 / c2;
    QVector2D pb = p1 + b * v;
    float dist = (point - pb).length();

    if (dist < minDistance) {
      minDistance = dist;
      nearestSegment = i;
    }
  }

  qDebug() << "Nearest segment:" << nearestSegment
           << "with distance:" << minDistance;
  return nearestSegment;
}

void ImagePrimitive::deleteControlPoint(int index) {
  if (index < 0 ||
      index >= static_cast<int>(m_detectedSubject.contour.size())) {
    return;
  }

  // Don't allow deleting if we have too few points
  if (m_detectedSubject.contour.size() <= 3) {
    qDebug()
        << "ImagePrimitive: Cannot delete - need at least 3 control points";
    return;
  }

  m_detectedSubject.contour.erase(m_detectedSubject.contour.begin() + index);

  m_detectedSubject.mask = QImage();

  emit maskModified();

  qDebug() << "ImagePrimitive: Deleted control point" << index;
}

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
