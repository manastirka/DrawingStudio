#pragma once

#include <QColor>
#include <QObject>
#include <QPolygonF>
#include <QRectF>
#include <QVector2D>
#include <memory>
#include <vector>

class DrawingPrimitive;
class CanvasRenderer;
class LayerManager;

class SelectionManager : public QObject {
  Q_OBJECT

public:
  enum class SelectionOperation { Replace, Add, Subtract };

  explicit SelectionManager(QObject *parent = nullptr);
  ~SelectionManager();

  // When a LayerManager is set, selection operations act on the primitives of
  // all visible, unlocked layers instead of the legacy fallback list passed to
  // the operation methods.
  void setLayerManager(LayerManager *layerManager);
  LayerManager *layerManager() const { return m_layerManager; }

  // Selection state
  const std::vector<DrawingPrimitive *> &selectedObjects() const {
    return m_selectedObjects;
  }
  bool hasSelection() const { return !m_selectedObjects.empty(); }
  bool isSelected(DrawingPrimitive *primitive) const;

  // Selection modification
  void clearSelection();
  void addToSelection(DrawingPrimitive *primitive);
  void removeFromSelection(DrawingPrimitive *primitive);
  void setSelection(const std::vector<DrawingPrimitive *> &primitives);
  void setColorForSelection(const QColor &color);
  void removeNullSelectedObjects();

  // Drag selection
  void setSelectionRect(const QRectF &rect);
  const QRectF &selectionRect() const { return m_selectionRect; }
  void setIsSelecting(bool selecting);
  bool isSelecting() const { return m_isSelecting; }

  // Pipette mode
  void setPipetteMode(bool enabled);
  bool isPipetteMode() const { return m_isPipetteMode; }
  void setPipetteTolerance(int tolerance);
  int pipetteTolerance() const { return m_pipetteTolerance; }
  void enablePipetteMode(int tolerance);

  // Operations
  void selectObjectAt(
      const QVector2D &pos,
      const std::vector<std::unique_ptr<DrawingPrimitive>> &primitives,
      float tolerance = 5.0f);
  void selectObjectsInRect(
      const QRectF &rect,
      const std::vector<std::unique_ptr<DrawingPrimitive>> &primitives,
      SelectionOperation operation = SelectionOperation::Replace);
  void selectObjectsInLasso(
      const QPolygonF &polygon,
      const std::vector<std::unique_ptr<DrawingPrimitive>> &primitives,
      SelectionOperation operation = SelectionOperation::Replace);
  void selectByColor(
      const QColor &color,
      const std::vector<std::unique_ptr<DrawingPrimitive>> &primitives,
      int tolerance = 10);

  // Rendering
  void render(CanvasRenderer *renderer);

signals:
  void selectionChanged();

private:
  // Returns the primitives selection operations should act on: the visible,
  // unlocked layers of the LayerManager when present, otherwise the supplied
  // legacy fallback list.
  std::vector<DrawingPrimitive *> effectivePrimitives(
      const std::vector<std::unique_ptr<DrawingPrimitive>> &fallback) const;

  std::vector<DrawingPrimitive *> m_selectedObjects;
  QRectF m_selectionRect;
  bool m_isSelecting;
  bool m_isPipetteMode;
  int m_pipetteTolerance;
  LayerManager *m_layerManager = nullptr;
};
