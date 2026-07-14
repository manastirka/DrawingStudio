// Link stubs for ProjectFileService tests when canvas is not constructed.
// Only symbols referenced by ProjectFileService.cpp are provided.

#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"

#include <memory>
#include <vector>

void DrawingCanvas::setPaperColor(const QColor &) {}
void DrawingCanvas::setGridVisible(bool) {}
void DrawingCanvas::setSnapEnabled(bool) {}
void DrawingCanvas::clearPrimitives() {}
void DrawingCanvas::setBackgroundColor(const QColor &) {}
bool DrawingCanvas::isGridVisible() const { return false; }
bool DrawingCanvas::isSnapEnabled() const { return false; }
