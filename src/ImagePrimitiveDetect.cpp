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

// ImagePrimitive SAM2 detection (refactor E22).

// --- autoDetectSubject ---
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


// --- startSubjectDetection ---
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


// --- startHumanDetection ---
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


// --- onSAM2SegmentationComplete ---
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


// --- onSAM2SegmentationFailed ---
void ImagePrimitive::onSAM2SegmentationFailed(const QString &error) {
  m_detectionInProgress = false;
  qDebug() << "ImagePrimitive: SAM2 segmentation failed:" << error;

  // Emit signal
  emit detectionFailed(error);
}


// --- onSAM2MultiSegmentationComplete ---
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


