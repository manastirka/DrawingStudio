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

// ImagePrimitive mask extract / contour edit (refactor E22).

// --- setEditMode ---
void ImagePrimitive::setEditMode(bool enabled) {
  m_editMode = enabled;

  // Trigger subject detection when edit mode is first enabled
  if (enabled && !m_detectionInProgress && m_maskCandidates.empty()) {
    startSubjectDetection();
  }
}


// --- extractDetectedSubject ---
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


// --- extractAllMaskCandidates ---
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


// --- applyMaskCandidateAsPrimary ---
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


// --- isMaskCandidateSelected ---
bool ImagePrimitive::isMaskCandidateSelected(int index) const
{
  return std::find(m_selectedMaskIndices.begin(), m_selectedMaskIndices.end(),
                   index) != m_selectedMaskIndices.end();
}


// --- clearMaskMultiSelection ---
void ImagePrimitive::clearMaskMultiSelection()
{
  m_selectedMaskIndices.clear();
}


// --- selectMaskCandidate ---
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


// --- addMaskCandidateToSelection ---
void ImagePrimitive::addMaskCandidateToSelection(int index)
{
  if (index < 0 || index >= static_cast<int>(m_maskCandidates.size()))
    return;
  if (!isMaskCandidateSelected(index))
    m_selectedMaskIndices.push_back(index);
  applyMaskCandidateAsPrimary(index);
  emit detectionComplete();
}


// --- extractMaskCandidateAt ---
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


// --- extractCombinedSelectedMasks ---
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


// --- maskCandidatePixelBounds ---
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


// --- extractSelectedMasksSeparately ---
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


// --- getSelectedCandidate ---
const ImagePrimitive::MaskCandidate *
ImagePrimitive::getSelectedCandidate() const {
  if (m_selectedMaskIndex >= 0 &&
      m_selectedMaskIndex < static_cast<int>(m_maskCandidates.size())) {
    return &m_maskCandidates[m_selectedMaskIndex];
  }
  return nullptr;
}


// --- getCandidateAt ---
const ImagePrimitive::MaskCandidate *
ImagePrimitive::getCandidateAt(int index) const {
  if (index >= 0 && index < static_cast<int>(m_maskCandidates.size())) {
    return &m_maskCandidates[index];
  }
  return nullptr;
}


// --- getMaskIndexAt ---
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


// --- imageToWorld ---
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


// --- worldToImage ---
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


// --- invertMask ---
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


// --- fillMaskedArea ---
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


// --- getControlPointAt ---
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


// --- moveControlPoint ---
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


// --- insertControlPoint ---
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


// --- getNearestContourSegment ---
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


// --- deleteControlPoint ---
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


