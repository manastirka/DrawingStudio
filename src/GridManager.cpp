#include "GridManager.h"
#include "CanvasRenderer.h"
#include <cmath>

GridManager::GridManager(QObject *parent)
    : QObject(parent), m_visible(true), m_snapEnabled(true),
      m_gridSize(DEFAULT_GRID_SIZE), m_gridColor(QColor(120, 120, 120, 255)),
      m_paperSize(210.0f, 297.0f) // Default A4
      ,
      m_pixelsPerMM(2.0f) // Default
{}

GridManager::~GridManager() {}

void GridManager::setVisible(bool visible) {
  if (m_visible != visible) {
    m_visible = visible;
    emit gridChanged();
  }
}

void GridManager::setSnapEnabled(bool enabled) { m_snapEnabled = enabled; }

void GridManager::setGridSize(float size) {
  if (m_gridSize != size) {
    m_gridSize = size;
    if (m_visible) {
      emit gridChanged();
    }
  }
}

void GridManager::setGridColor(const QColor &color) {
  if (m_gridColor != color) {
    m_gridColor = color;
    if (m_visible) {
      emit gridChanged();
    }
  }
}

void GridManager::setPaperSize(const QSizeF &size) {
  if (m_paperSize != size) {
    m_paperSize = size;
    if (m_visible) {
      emit gridChanged();
    }
  }
}

void GridManager::setPixelsPerMM(float pixelsPerMM) {
  if (m_pixelsPerMM != pixelsPerMM) {
    m_pixelsPerMM = pixelsPerMM;
    if (m_visible) {
      emit gridChanged();
    }
  }
}

QVector2D GridManager::snapToGrid(const QVector2D &pos) const {
  if (!m_snapEnabled) {
    return pos;
  }

  float snappedX = std::round(pos.x() / m_gridSize) * m_gridSize;
  float snappedY = std::round(pos.y() / m_gridSize) * m_gridSize;

  return QVector2D(snappedX, snappedY);
}

void GridManager::render(CanvasRenderer *renderer) {
  if (!m_visible || !renderer) {
    return;
  }

  // Calculate paper boundaries in world coordinates
  float paperWidth = m_paperSize.width() * m_pixelsPerMM;
  float paperHeight = m_paperSize.height() * m_pixelsPerMM;

  // Paper is centered at origin
  float paperLeft = -paperWidth / 2.0f;
  float paperRight = paperWidth / 2.0f;
  float paperBottom = -paperHeight / 2.0f;
  float paperTop = paperHeight / 2.0f;

  // For grid, we'll draw within paper bounds, aligned to grid
  float gridLeft = ceil(paperLeft / m_gridSize) * m_gridSize;
  float gridRight = floor(paperRight / m_gridSize) * m_gridSize;
  float gridBottom = ceil(paperBottom / m_gridSize) * m_gridSize;
  float gridTop = floor(paperTop / m_gridSize) * m_gridSize;

  // Make grid always visible with consistent opacity
  float opacity = 0.6f;

  QVector4D gridColorVec(m_gridColor.redF(), m_gridColor.greenF(),
                         m_gridColor.blueF(), opacity);

  auto batch = renderer->begin(CanvasRenderer::Mode::Lines, 1.0f);

  // Vertical lines
  for (float x = gridLeft; x <= gridRight + 0.001f; x += m_gridSize) {
    renderer->addVertex(batch, QVector2D(x, gridBottom), gridColorVec);
    renderer->addVertex(batch, QVector2D(x, gridTop), gridColorVec);
  }

  // Horizontal lines
  for (float y = gridBottom; y <= gridTop + 0.001f; y += m_gridSize) {
    renderer->addVertex(batch, QVector2D(gridLeft, y), gridColorVec);
    renderer->addVertex(batch, QVector2D(gridRight, y), gridColorVec);
  }

  renderer->submit(batch);
}
