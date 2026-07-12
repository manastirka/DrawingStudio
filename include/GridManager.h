#pragma once

#include <QObject>
#include <QColor>
#include <QVector2D>
#include <QSizeF>
#include <memory>

class CanvasRenderer;

class GridManager : public QObject
{
    Q_OBJECT

public:
    explicit GridManager(QObject *parent = nullptr);
    ~GridManager();

    // Grid settings
    void setVisible(bool visible);
    bool isVisible() const { return m_visible; }

    void setSnapEnabled(bool enabled);
    bool isSnapEnabled() const { return m_snapEnabled; }

    void setGridSize(float size);
    float gridSize() const { return m_gridSize; }

    void setGridColor(const QColor &color);
    const QColor& gridColor() const { return m_gridColor; }

    // Paper settings (needed for grid rendering boundaries)
    void setPaperSize(const QSizeF &size);
    const QSizeF& paperSize() const { return m_paperSize; }

    void setPixelsPerMM(float pixelsPerMM);
    float pixelsPerMM() const { return m_pixelsPerMM; }

    // Core functionality
    QVector2D snapToGrid(const QVector2D &pos) const;
    void render(CanvasRenderer* renderer);

    // Constants
    static constexpr float DEFAULT_GRID_SIZE = 7.5591f; // 2mm grid
    static constexpr float FINE_GRID_SIZE = 3.77953f;   // 1mm grid
    static constexpr float COARSE_GRID_SIZE = 37.7953f; // 10mm grid

signals:
    void gridChanged();

private:
    bool m_visible;
    bool m_snapEnabled;
    float m_gridSize;
    QColor m_gridColor;
    QSizeF m_paperSize;
    float m_pixelsPerMM;
};
