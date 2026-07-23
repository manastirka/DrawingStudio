#pragma once

#include "DrawingPrimitive.h"
#include "SAM2Client.h"
#include <QImage>
#include <QRectF>
#include <QVector>
#include <QVector2D>

class ImagePrimitive : public DrawingPrimitive {
  Q_OBJECT

public:
  static constexpr qsizetype kMaxSerializedMaskCandidates = 4096;
  static constexpr qsizetype kMaxSerializedMaskPoints = 100000;
  static constexpr int kMaxContourSmoothness = 32;
  static constexpr int kMaxMaskFeather = 30;
  static constexpr int kMaxMaskBlur = 20;
  static constexpr int kMaxMaskExpand = 20;

  ImagePrimitive();
  ImagePrimitive(const QImage &image, const QVector2D &position,
                 const QVector2D &size);
  ~ImagePrimitive() override;

  // DrawingPrimitive interface
  void render(QPainter* painter) const override;
  QRectF boundingRect() const override;
  bool containsPoint(const QVector2D &point,
                     float tolerance = 0.0f) const override;
  std::unique_ptr<DrawingPrimitive> clone() const override;
  void translate(const QVector2D &offset) override;
  std::vector<QVector2D> getControlPoints() const override;
  void setControlPointPosition(int index, const QVector2D &position) override;

  // Serialization
  QJsonObject toJson() const override;
  void fromJson(const QJsonObject &json) override;

  // Image-specific methods
  void setImage(const QImage &image);
  QImage image() const { return m_image; }

  void setPosition(const QVector2D &position);
  QVector2D position() const { return m_position; }

  void setSize(const QVector2D &size);
  QVector2D size() const { return m_size; }

  void setRotation(float rotation);
  float rotation() const { return m_rotation; }

  // Resize handles
  enum ResizeHandle : int {
    TopLeft = 0,
    TopRight = 1,
    BottomLeft = 2,
    BottomRight = 3,
    Top = 4,
    Bottom = 5,
    Left = 6,
    Right = 7,
    ResizeHandleCount = 8,
    None = -1
  };

  ResizeHandle getResizeHandleAt(const QVector2D &point,
                                 float tolerance = 8.0f) const;
  QVector2D getHandlePosition(ResizeHandle handle) const;
  QRectF getResizeHandleRect(ResizeHandle handle) const;
  void resizeFromHandle(ResizeHandle handle, const QVector2D &newPosition);

  // Aspect ratio
  void setMaintainAspectRatio(bool maintain) {
    m_maintainAspectRatio = maintain;
  }
  bool maintainAspectRatio() const { return m_maintainAspectRatio; }

  // Simplified SAM2 auto-detection API
  struct DetectedSubject {
    QImage extractedImage;
    std::vector<QPointF> contour;
    QRectF boundingBox;
    QImage mask;
  };

  // Manual subject detection trigger for performance optimization.
  // maxDimension controls the resize used before segmentation (larger = more
  // accurate but slower).
  void startSubjectDetection(int maxDimension = 1536);
  void startHumanDetection();        // Human detection using YOLO + SAM2

  // Mask candidate for selection
  struct MaskCandidate {
    int id;
    std::vector<QPointF> contour;
    QImage mask; // Full-resolution binary mask when available
    float score;
    float stability;
    float predicted_iou;
    float area_percent;
  };

  void autoDetectSubject();
  std::unique_ptr<ImagePrimitive> extractDetectedSubject();
  std::vector<std::unique_ptr<ImagePrimitive>> extractAllMaskCandidates();
  bool hasDetectedSubject() const {
    return !m_detectedSubject.extractedImage.isNull();
  }

  // Mask selection API
  int getMaskCandidateCount() const { return m_maskCandidates.size(); }
  int getSelectedMaskIndex() const { return m_selectedMaskIndex; }
  const std::vector<int> &selectedMaskIndices() const {
    return m_selectedMaskIndices;
  }
  bool isMaskCandidateSelected(int index) const;
  /** Replace multi-selection with a single mask (Next/Prev mask). */
  void selectMaskCandidate(int index);
  /** Add a mask to the green multi-selection (click another subject). */
  void addMaskCandidateToSelection(int index);
  void clearMaskMultiSelection();
  const MaskCandidate *getSelectedCandidate() const;
  const MaskCandidate *getCandidateAt(int index) const;
  void invertMask(); // Invert the current mask selection
  bool isMaskInverted() const { return m_maskInverted; }
  void fillMaskedArea(const QColor &color); // Fill the masked area with a color

  /**
   * Index of mask under world point, or -1.
   * If preferUnselected, returns an unselected mask under the point when possible.
   */
  int getMaskIndexAt(const QVector2D &point,
                     bool preferUnselected = false) const;

  /** Extract one mask candidate without clearing multi-selection state. */
  std::unique_ptr<ImagePrimitive> extractMaskCandidateAt(int index);

  /**
   * Extract all currently multi-selected masks into ONE cutout image that
   * preserves their original relative positions (as in the source photo).
   */
  std::unique_ptr<ImagePrimitive> extractCombinedSelectedMasks();

  /** One cutout + where it sat in the source photo (normalized 0..1). */
  struct ExtractedMaskCutout {
    QImage image;
    QRectF sourceNormRect;
  };

  /**
   * Extract each selected green mask as its own cutout, with layout rects
   * so compose can place them separately (not glued as one original crop).
   */
  QVector<ExtractedMaskCutout> extractSelectedMasksSeparately();

  /** Pixel bounding box of a mask candidate in source-image coordinates. */
  QRect maskCandidatePixelBounds(int index) const;

  // Coordinate conversion between original image space (Y-down, origin
  // top-left) and world space (accounts for position, size scaling and the
  // Y-flip used when rendering/editing contours).
  QVector2D imageToWorld(const QPointF &imagePoint) const;
  QPointF worldToImage(const QVector2D &worldPoint) const;

  // Mask editing API
  void setEditMode(bool enabled);
  bool isEditMode() const { return m_editMode; }
  int getControlPointAt(const QVector2D &point, float tolerance = 10.0f) const;
  void moveControlPoint(int index, const QVector2D &position);
  void insertControlPoint(int afterIndex, const QVector2D &position);
  void deleteControlPoint(int index);
  std::vector<QPointF> getEditableContour() const {
    return m_detectedSubject.contour;
  }
  int getNearestContourSegment(const QVector2D &point,
                               float tolerance = 10.0f) const;

  // Contour rendering smoothness
  void setContourSmoothness(int level);
  int getContourSmoothness() const { return m_contourSmoothness; }

  // Mask refinement parameters (applied during extraction)
  void setMaskFeather(int amount);
  int getMaskFeather() const { return m_maskFeather; }
  void setMaskBlur(int amount);
  int getMaskBlur() const { return m_maskBlur; }
  void setMaskExpand(int amount);
  int getMaskExpand() const { return m_maskExpand; }
  void setMaskOverlayVisible(bool visible) { m_maskOverlayVisible = visible; }
  bool isMaskOverlayVisible() const { return m_maskOverlayVisible; }

signals:
  void detectionComplete();
  void detectionFailed(const QString &error);
  void detectionProgress(int percentage, const QString &message);
  void maskModified(); // Emitted when mask is edited

private slots:
  void onSAM2SegmentationComplete(const SAM2Client::SegmentationResult &result);
  void onSAM2MultiSegmentationComplete(
      const SAM2Client::MultiSegmentationResult &result);
  void onSAM2SegmentationFailed(const QString &error);

private:
  void applyMaskCandidateAsPrimary(int index);

  QImage m_image;
  QVector2D m_position;
  QVector2D m_size;
  float m_rotation;
  bool m_maintainAspectRatio;

  SAM2Client *m_sam2Client;
  DetectedSubject m_detectedSubject;
  bool m_detectionInProgress;
  float m_detectionScaleX; // Scale factor from detection image to original
  float m_detectionScaleY;

  // Mask selection
  std::vector<MaskCandidate> m_maskCandidates;
  int m_selectedMaskIndex;
  std::vector<int> m_selectedMaskIndices; // multi green selection
  bool m_maskInverted; // True if mask is inverted (selecting background)

  // Mask editing
  bool m_editMode;
  int m_draggingControlPoint;
  int m_contourSmoothness; // 0 = angular, higher = smoother interpolation

  // Mask refinement parameters
  int m_maskFeather;
  int m_maskBlur;
  int m_maskExpand;
  bool m_maskOverlayVisible;

  static constexpr float HANDLE_SIZE = 8.0f;
  static constexpr float CONTROL_POINT_SIZE = 2.5f;
};
