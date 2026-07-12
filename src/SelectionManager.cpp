#include "SelectionManager.h"
#include "CanvasRenderer.h"
#include "DrawingPrimitive.h"
#include "Layer.h"
#include "LayerManager.h"
#include <QPainterPath>
#include <algorithm>
#include <cmath>

SelectionManager::SelectionManager(QObject *parent)
    : QObject(parent), m_isSelecting(false), m_isPipetteMode(false),
      m_pipetteTolerance(10) {}

SelectionManager::~SelectionManager() {}

void SelectionManager::setLayerManager(LayerManager *layerManager) {
  m_layerManager = layerManager;
}

std::vector<DrawingPrimitive *> SelectionManager::effectivePrimitives(
    const std::vector<std::unique_ptr<DrawingPrimitive>> &fallback) const {
  std::vector<DrawingPrimitive *> result;
  if (m_layerManager) {
    for (const auto &layer : m_layerManager->layers()) {
      if (!layer || !layer->isVisible() || layer->isLocked()) {
        continue;
      }
      for (const auto &prim : layer->primitives()) {
        if (prim) {
          result.push_back(prim.get());
        }
      }
    }
  } else {
    for (const auto &prim : fallback) {
      if (prim) {
        result.push_back(prim.get());
      }
    }
  }
  return result;
}

bool SelectionManager::isSelected(DrawingPrimitive *primitive) const {
  return std::find(m_selectedObjects.begin(), m_selectedObjects.end(),
                   primitive) != m_selectedObjects.end();
}

void SelectionManager::clearSelection() {
  if (!m_selectedObjects.empty()) {
    for (auto *obj : m_selectedObjects) {
      obj->setSelected(false);
    }
    m_selectedObjects.clear();
    emit selectionChanged();
  }
}

void SelectionManager::addToSelection(DrawingPrimitive *primitive) {
  if (primitive && !isSelected(primitive)) {
    primitive->setSelected(true);
    m_selectedObjects.push_back(primitive);
    emit selectionChanged();
  }
}

void SelectionManager::removeFromSelection(DrawingPrimitive *primitive) {
  auto it =
      std::find(m_selectedObjects.begin(), m_selectedObjects.end(), primitive);
  if (it != m_selectedObjects.end()) {
    (*it)->setSelected(false);
    m_selectedObjects.erase(it);
    emit selectionChanged();
  }
}

void SelectionManager::setSelection(
    const std::vector<DrawingPrimitive *> &primitives) {
  clearSelection();
  for (auto *prim : primitives) {
    addToSelection(prim);
  }
}

void SelectionManager::setColorForSelection(const QColor &color) {
  for (auto *prim : m_selectedObjects) {
    prim->setColor(color);

    // Handle filled primitives
    switch (prim->type()) {
    case PrimitiveType::Rectangle:
      if (auto *rect = dynamic_cast<RectanglePrimitive *>(prim)) {
        rect->setFilled(true);
        rect->setFillColor(color);
      }
      break;
    case PrimitiveType::Circle:
      if (auto *circle = dynamic_cast<CirclePrimitive *>(prim)) {
        circle->setFilled(true);
        circle->setFillColor(color);
      }
      break;
    case PrimitiveType::Ellipse:
      if (auto *ellipse = dynamic_cast<EllipsePrimitive *>(prim)) {
        ellipse->setFilled(true);
        ellipse->setFillColor(color);
      }
      break;
    case PrimitiveType::Polygon:
      if (auto *poly = dynamic_cast<PolygonPrimitive *>(prim)) {
        poly->setFilled(true);
        poly->setFillColor(color);
      }
      break;
    default:
      break;
    }
  }
  emit selectionChanged();
}

void SelectionManager::removeNullSelectedObjects() {
  auto it = std::remove_if(m_selectedObjects.begin(), m_selectedObjects.end(),
                           [](DrawingPrimitive *p) { return p == nullptr; });
  if (it != m_selectedObjects.end()) {
    m_selectedObjects.erase(it, m_selectedObjects.end());
    emit selectionChanged();
  }
}

void SelectionManager::setSelectionRect(const QRectF &rect) {
  m_selectionRect = rect;
}

void SelectionManager::setIsSelecting(bool selecting) {
  m_isSelecting = selecting;
}

void SelectionManager::setPipetteMode(bool enabled) {
  m_isPipetteMode = enabled;
}

void SelectionManager::setPipetteTolerance(int tolerance) {
  m_pipetteTolerance = tolerance;
}

void SelectionManager::enablePipetteMode(int tolerance) {
  setPipetteMode(true);
  setPipetteTolerance(tolerance);
}

void SelectionManager::selectObjectAt(
    const QVector2D &pos,
    const std::vector<std::unique_ptr<DrawingPrimitive>> &primitives,
    float tolerance) {
  DrawingPrimitive *hitObject = nullptr;

  const std::vector<DrawingPrimitive *> effective = effectivePrimitives(primitives);

  // Iterate in reverse to select top-most object
  for (auto it = effective.rbegin(); it != effective.rend(); ++it) {
    if (*it && (*it)->containsPoint(pos, tolerance)) {
      hitObject = *it;
      break;
    }
  }

  if (hitObject) {
    // If shift is not held (handled by caller usually, but here we assume
    // simple logic for now) For now, let's assume this method replaces
    // selection unless we want to support multi-select logic here. But
    // typically selectObjectAt is a low-level "find and select". Let's make it
    // clear selection and select new one. Wait, the original logic might have
    // been different. In DrawingCanvas, handleSelectTool handles the modifier
    // keys. So this method should probably just FIND the object? But the name
    // is "selectObjectAt". Let's make it "findObjectAt"? No, the task is to
    // move logic. Let's stick to "selectObjectAt" but maybe it should just add?
    // Or maybe it should take a "replace" flag?

    // Actually, let's look at how it was used.
    // DrawingCanvas::selectObjectAt cleared selection if no modifier.

    // For flexibility, let's make this method just set the selection to the hit
    // object. Complex logic (modifiers) should be handled by the caller using
    // primitives() and addToSelection/removeFromSelection.

    // But wait, if I move the loop here, I am encapsulating the hit test.
    // Let's keep it simple: clear and select.
    clearSelection();
    addToSelection(hitObject);
  } else {
    clearSelection();
  }
}

void SelectionManager::selectObjectsInRect(
    const QRectF &rect,
    const std::vector<std::unique_ptr<DrawingPrimitive>> &primitives,
    SelectionOperation operation) {
  if (operation == SelectionOperation::Replace) {
    clearSelection();
  }

  // Normalize rect
  float left = std::min(rect.left(), rect.right());
  float right = std::max(rect.left(), rect.right());
  float top = std::min(rect.top(), rect.bottom());
  float bottom = std::max(rect.top(), rect.bottom());
  QRectF normalizedRect(left, top, right - left, bottom - top);

  for (DrawingPrimitive *prim : effectivePrimitives(primitives)) {
    // Use bounding rect intersection as a first approximation
    // Ideally, we should have a more precise intersects(QRectF) method on
    // primitives
    if (prim && prim->boundingRect().intersects(normalizedRect)) {
      if (operation == SelectionOperation::Subtract) {
        removeFromSelection(prim);
      } else {
        addToSelection(prim);
      }
    }
  }
}

void SelectionManager::selectObjectsInLasso(
    const QPolygonF &polygon,
    const std::vector<std::unique_ptr<DrawingPrimitive>> &primitives,
    SelectionOperation operation) {
  if (polygon.size() < 3) {
    return;
  }

  if (operation == SelectionOperation::Replace) {
    clearSelection();
  }

  QPainterPath path;
  path.addPolygon(polygon);

  for (DrawingPrimitive *prim : effectivePrimitives(primitives)) {
    if (!prim) {
      continue;
    }
    QRectF bounds = prim->boundingRect();
    if (path.intersects(bounds) || path.contains(bounds.center())) {
      if (operation == SelectionOperation::Subtract) {
        removeFromSelection(prim);
      } else {
        addToSelection(prim);
      }
    }
  }
}

void SelectionManager::selectByColor(
    const QColor &color,
    const std::vector<std::unique_ptr<DrawingPrimitive>> &primitives,
    int tolerance) {
  clearSelection();
  for (DrawingPrimitive *prim : effectivePrimitives(primitives)) {
    if (!prim) {
      continue;
    }
    const QColor primColor = prim->color();
    int dr = primColor.red() - color.red();
    int dg = primColor.green() - color.green();
    int db = primColor.blue() - color.blue();
    float distance = std::sqrt(static_cast<float>(dr * dr + dg * dg + db * db));
    float maxDistance = (static_cast<float>(tolerance) / 100.0f) * 441.672f;
    if (distance <= maxDistance) {
      addToSelection(prim);
    }
  }
}

void SelectionManager::render(CanvasRenderer *renderer) {
  if (!renderer)
    return;

  // Render selection rectangle if selecting
  if (m_isSelecting && m_selectionRect.width() > 0.5f &&
      m_selectionRect.height() > 0.5f) {
    QVector4D selectionColor(0.0f, 0.5f, 1.0f, 0.3f); // Semi-transparent blue
    auto batch = renderer->begin(CanvasRenderer::Mode::Triangles, 1.0f);
    // Add selection rect (filled)
    float x = m_selectionRect.x();
    float y = m_selectionRect.y();
    float w = m_selectionRect.width();
    float h = m_selectionRect.height();

    // Triangle 1
    renderer->addVertex(batch, QVector2D(x, y), selectionColor);
    renderer->addVertex(batch, QVector2D(x + w, y), selectionColor);
    renderer->addVertex(batch, QVector2D(x, y + h), selectionColor);

    // Triangle 2
    renderer->addVertex(batch, QVector2D(x + w, y), selectionColor);
    renderer->addVertex(batch, QVector2D(x + w, y + h), selectionColor);
    renderer->addVertex(batch, QVector2D(x, y + h), selectionColor);

    renderer->submit(batch);

    // Outline
    QVector4D outlineColor(0.0f, 0.5f, 1.0f, 1.0f);
    auto lineBatch = renderer->begin(CanvasRenderer::Mode::Lines, 1.0f);

    // Top
    renderer->addVertex(lineBatch, QVector2D(x, y), outlineColor);
    renderer->addVertex(lineBatch, QVector2D(x + w, y), outlineColor);
    // Right
    renderer->addVertex(lineBatch, QVector2D(x + w, y), outlineColor);
    renderer->addVertex(lineBatch, QVector2D(x + w, y + h), outlineColor);
    // Bottom
    renderer->addVertex(lineBatch, QVector2D(x + w, y + h), outlineColor);
    renderer->addVertex(lineBatch, QVector2D(x, y + h), outlineColor);
    // Left
    renderer->addVertex(lineBatch, QVector2D(x, y + h), outlineColor);
    renderer->addVertex(lineBatch, QVector2D(x, y), outlineColor);

    renderer->submit(lineBatch);
  }

}
