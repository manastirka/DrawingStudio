#include "Commands.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "LayerManager.h"
#include "Layer.h"
#include "BrushStrokePrimitive.h"
#include "ImagePrimitive.h"
#include <QDebug>
#include <QJsonDocument>
#include <QColor>
#include <algorithm>
#include <map>
#include <set>

// Image / mask commands (refactor E20).

// ============================================================================
// ExtractSubjectCommand
// ============================================================================

ExtractSubjectCommand::ExtractSubjectCommand(DrawingCanvas* canvas, ImagePrimitive* sourceImage)
    : m_canvas(canvas)
    , m_sourceImage(sourceImage)
    , m_executed(false)
{
}

void ExtractSubjectCommand::execute()
{
    if (m_executed || !m_canvas) {
        return;
    }

    qDebug() << "Executing ExtractSubjectCommand";

    // First execution: extract; Redo: re-add the stored cutout.
    if (!m_extractedImage && m_sourceImage) {
        m_extractedImage = m_sourceImage->extractDetectedSubject();
    }

    if (!m_extractedImage) {
        qDebug() << "Failed to extract subject";
        return;
    }

    // Add to canvas
    ImagePrimitive* extractedPtr = m_extractedImage.get();
    m_extractedImageId = extractedPtr->id();
    m_canvas->addPrimitive(std::move(m_extractedImage));

    // Clear selection and select the extracted cutout
    m_canvas->clearSelection();
    m_canvas->addToSelection(extractedPtr);

    // Auto-activate Select tool — supports both move AND resize handles for the cutout
    m_canvas->setCurrentTool(DrawingTool::Select);

    m_executed = true;
    qDebug() << "Subject extracted successfully";
}

void ExtractSubjectCommand::undo()
{
    if (!m_executed || !m_canvas) {
        return;
    }

    qDebug() << "Undoing ExtractSubjectCommand";

    LayerManager* layerManager = m_canvas->layerManager();
    if (layerManager && !m_extractedImageId.isNull()) {
        for (const auto& layer : layerManager->layers()) {
            if (!layer) {
                continue;
            }
            auto& primitives = layer->primitives();
            for (auto it = primitives.begin(); it != primitives.end(); ++it) {
                if (!(*it) || (*it)->id() != m_extractedImageId) {
                    continue;
                }

                std::unique_ptr<DrawingPrimitive> removed = std::move(*it);
                primitives.erase(it);
                m_extractedImage.reset(dynamic_cast<ImagePrimitive*>(removed.release()));

                m_canvas->clearSelection();
                m_canvas->update();
                m_executed = false;
                qDebug() << "Extracted subject removed from canvas";
                return;
            }
        }
    } else {
        // Legacy mode fallback: search in flat primitives list
        auto& primitives = const_cast<std::vector<std::unique_ptr<DrawingPrimitive>>&>(m_canvas->primitives());
        for (auto it = primitives.begin(); it != primitives.end(); ++it) {
            if (!(*it) || (*it)->id() != m_extractedImageId) {
                continue;
            }

            std::unique_ptr<DrawingPrimitive> removed = std::move(*it);
            primitives.erase(it);
            m_extractedImage.reset(dynamic_cast<ImagePrimitive*>(removed.release()));
            m_canvas->clearSelection();
            m_canvas->update();
            m_executed = false;
            qDebug() << "Extracted subject removed from canvas";
            return;
        }
    }

    qDebug() << "Warning: Could not find extracted image to remove";
}

QString ExtractSubjectCommand::description() const
{
    return "Extract Subject";
}

// ============================================================================
// ImportImageCommand
// ============================================================================

ImportImageCommand::ImportImageCommand(DrawingCanvas* canvas, std::unique_ptr<ImagePrimitive> image)
    : m_canvas(canvas)
    , m_image(std::move(image))
    , m_executed(false)
{
}

void ImportImageCommand::execute()
{
    if (!m_executed && m_image && m_canvas) {
        qDebug() << "Executing ImportImageCommand";
        m_imageId = m_image->id();
        m_canvas->addPrimitive(std::move(m_image));
        m_executed = true;
    }
}

void ImportImageCommand::undo()
{
    if (m_executed && m_canvas) {
        qDebug() << "Undoing ImportImageCommand";

        LayerManager* layerManager = m_canvas->layerManager();
        if (layerManager && !m_imageId.isNull()) {
            for (const auto& layer : layerManager->layers()) {
                if (!layer) {
                    continue;
                }
                auto& primitives = layer->primitives();
                for (auto it = primitives.begin(); it != primitives.end(); ++it) {
                    if (!(*it) || (*it)->id() != m_imageId) {
                        continue;
                    }

                    std::unique_ptr<DrawingPrimitive> removed = std::move(*it);
                    primitives.erase(it);
                    m_image.reset(dynamic_cast<ImagePrimitive*>(removed.release()));

                    m_canvas->clearSelection();
                    m_canvas->update();
                    m_executed = false;
                    qDebug() << "Imported image removed from canvas";
                    return;
                }
            }
        } else {
            // Legacy mode fallback
            auto& primitives = const_cast<std::vector<std::unique_ptr<DrawingPrimitive>>&>(m_canvas->primitives());
            for (auto it = primitives.begin(); it != primitives.end(); ++it) {
                if (!(*it) || (*it)->id() != m_imageId) {
                    continue;
                }

                std::unique_ptr<DrawingPrimitive> removed = std::move(*it);
                primitives.erase(it);
                m_image.reset(dynamic_cast<ImagePrimitive*>(removed.release()));
                m_canvas->clearSelection();
                m_canvas->update();
                m_executed = false;
                qDebug() << "Imported image removed from canvas";
                return;
            }
        }

        qDebug() << "Warning: Could not find imported image to remove";
    }
}

QString ImportImageCommand::description() const
{
    return "Import Image";
}

// ============================================================================
// ExtractFloorPlanLinesCommand
// ============================================================================

ExtractFloorPlanLinesCommand::ExtractFloorPlanLinesCommand(
    DrawingCanvas *canvas, LayerManager *layerManager, QString layerName,
    std::vector<Segment> segments, QColor color, float lineWidth)
    : m_canvas(canvas)
    , m_layerManager(layerManager)
    , m_layerName(std::move(layerName))
    , m_segments(std::move(segments))
    , m_color(color)
    , m_lineWidth(lineWidth)
{
}

void ExtractFloorPlanLinesCommand::execute()
{
    if (m_executed || !m_layerManager || m_segments.empty())
        return;

    if (Layer *active = m_layerManager->activeLayer())
        m_previousActiveLayerId = active->id();

    Layer *created = nullptr;
    if (m_layer) {
        created = m_layerManager->insertLayer(std::move(m_layer), m_insertIndex);
    } else {
        created = m_layerManager->createLayer(m_layerName);
        if (created) {
            m_layerId = created->id();
            m_insertIndex = m_layerManager->getLayerIndex(created);
            for (const Segment &seg : m_segments) {
                auto linePrim = std::make_unique<LinePrimitive>(seg.start, seg.end);
                linePrim->setColor(m_color);
                linePrim->setLineWidth(m_lineWidth);
                linePrim->setLayerId(created->id());
                created->addPrimitive(std::move(linePrim));
            }
        }
    }

    if (created) {
        m_layerId = created->id();
        m_layerManager->setActiveLayer(created);
    }

    if (m_canvas)
        m_canvas->update();
    m_executed = true;
}

void ExtractFloorPlanLinesCommand::undo()
{
    if (!m_executed || !m_layerManager || m_layerId.isNull())
        return;

    size_t removedIndex = 0;
    m_layer = m_layerManager->takeLayer(m_layerId, &removedIndex);
    m_insertIndex = removedIndex;

    if (!m_previousActiveLayerId.isNull() &&
        m_layerManager->getLayer(m_previousActiveLayerId)) {
        m_layerManager->setActiveLayer(m_previousActiveLayerId);
    }

    if (m_canvas) {
        m_canvas->clearSelection();
        m_canvas->update();
    }
    m_executed = false;
}

QString ExtractFloorPlanLinesCommand::description() const
{
    return QStringLiteral("Extract Floor Plan Lines");
}

// ============================================================================
// SetContourSmoothnessCommand
// ============================================================================

SetContourSmoothnessCommand::SetContourSmoothnessCommand(ImagePrimitive* imagePrimitive, int oldSmoothness, int newSmoothness)
    : m_imagePrimitive(imagePrimitive)
    , m_oldSmoothness(oldSmoothness)
    , m_newSmoothness(newSmoothness)
{
}

void SetContourSmoothnessCommand::execute()
{
    if (m_imagePrimitive) {
        qDebug() << "Executing SetContourSmoothnessCommand:" << m_newSmoothness;
        m_imagePrimitive->setContourSmoothness(m_newSmoothness);
    }
}

void SetContourSmoothnessCommand::undo()
{
    if (m_imagePrimitive) {
        qDebug() << "Undoing SetContourSmoothnessCommand:" << m_oldSmoothness;
        m_imagePrimitive->setContourSmoothness(m_oldSmoothness);
    }
}

QString SetContourSmoothnessCommand::description() const
{
    return "Set Contour Smoothness";
}

ModifyMaskControlPointCommand::ModifyMaskControlPointCommand(ImagePrimitive* imagePrimitive, int controlPointIndex,
                                                           const QPointF& oldPosition, const QPointF& newPosition)
    : m_imagePrimitive(imagePrimitive)
    , m_controlPointIndex(controlPointIndex)
    , m_oldPosition(oldPosition)
    , m_newPosition(newPosition)
{
}

void ModifyMaskControlPointCommand::execute()
{
    if (m_imagePrimitive) {
        qDebug() << "Executing ModifyMaskControlPointCommand";
        
        // Convert from image coordinates to world coordinates
        // The m_newPosition is in image coordinates, but moveControlPoint expects world coordinates
        float scaleX = m_imagePrimitive->size().x() / m_imagePrimitive->image().width();
        float scaleY = m_imagePrimitive->size().y() / m_imagePrimitive->image().height();
        
        // Convert to world coordinates: world = imagePos + (imageCoord * scale)
        // For Y: Apply Y-flip (image Y=0 is top, world Y increases upward)
        float worldX = m_imagePrimitive->position().x() + (m_newPosition.x() * scaleX);
        float worldY = m_imagePrimitive->position().y() + (m_imagePrimitive->size().y() - (m_newPosition.y() * scaleY));
        
        QVector2D worldPos(worldX, worldY);
        qDebug() << "Converting image coords" << m_newPosition.x() << "," << m_newPosition.y() 
                 << "to world coords" << worldX << "," << worldY;
        
        m_imagePrimitive->moveControlPoint(m_controlPointIndex, worldPos);
    }
}

void ModifyMaskControlPointCommand::undo()
{
    if (m_imagePrimitive) {
        qDebug() << "Undoing ModifyMaskControlPointCommand";
        
        // Convert from image coordinates to world coordinates
        // The m_oldPosition is in image coordinates, but moveControlPoint expects world coordinates
        float scaleX = m_imagePrimitive->size().x() / m_imagePrimitive->image().width();
        float scaleY = m_imagePrimitive->size().y() / m_imagePrimitive->image().height();
        
        // Convert to world coordinates: world = imagePos + (imageCoord * scale)
        // For Y: Apply Y-flip (image Y=0 is top, world Y increases upward)
        float worldX = m_imagePrimitive->position().x() + (m_oldPosition.x() * scaleX);
        float worldY = m_imagePrimitive->position().y() + (m_imagePrimitive->size().y() - (m_oldPosition.y() * scaleY));
        
        QVector2D worldPos(worldX, worldY);
        qDebug() << "Converting image coords" << m_oldPosition.x() << "," << m_oldPosition.y() 
                 << "to world coords" << worldX << "," << worldY;
        
        m_imagePrimitive->moveControlPoint(m_controlPointIndex, worldPos);
    }
}

QString ModifyMaskControlPointCommand::description() const
{
    return QString("Modify Mask Point %1").arg(m_controlPointIndex);
}

bool ModifyMaskControlPointCommand::canMergeWith(const Command* other) const
{
    const ModifyMaskControlPointCommand* otherModify = dynamic_cast<const ModifyMaskControlPointCommand*>(other);
    return otherModify && 
           otherModify->m_imagePrimitive == m_imagePrimitive && 
           otherModify->m_controlPointIndex == m_controlPointIndex;
}

void ModifyMaskControlPointCommand::mergeWith(const Command* other)
{
    const ModifyMaskControlPointCommand* otherModify = dynamic_cast<const ModifyMaskControlPointCommand*>(other);
    if (otherModify) {
        m_newPosition = otherModify->m_newPosition;
    }
}

// InsertMaskControlPointCommand Implementation
InsertMaskControlPointCommand::InsertMaskControlPointCommand(ImagePrimitive* imagePrimitive, int afterIndex,
                                                           const QPointF& position)
    : m_imagePrimitive(imagePrimitive)
    , m_afterIndex(afterIndex)
    , m_position(position)
    , m_executed(false)
{
}

void InsertMaskControlPointCommand::execute()
{
    if (!m_executed && m_imagePrimitive) {
        qDebug() << "Executing InsertMaskControlPointCommand";
        m_imagePrimitive->insertControlPoint(m_afterIndex, QVector2D(m_position.x(), m_position.y()));
        m_executed = true;
    }
}

void InsertMaskControlPointCommand::undo()
{
    if (m_executed && m_imagePrimitive) {
        qDebug() << "Undoing InsertMaskControlPointCommand";
        // The inserted point will be at index (m_afterIndex + 1)
        m_imagePrimitive->deleteControlPoint(m_afterIndex + 1);
        m_executed = false;
    }
}

QString InsertMaskControlPointCommand::description() const
{
    return "Insert Mask Point";
}

// DeleteMaskControlPointCommand Implementation
DeleteMaskControlPointCommand::DeleteMaskControlPointCommand(ImagePrimitive* imagePrimitive, int controlPointIndex)
    : m_imagePrimitive(imagePrimitive)
    , m_controlPointIndex(controlPointIndex)
    , m_executed(false)
{
    // Store the position of the point we're about to delete
    if (m_imagePrimitive) {
        auto contour = m_imagePrimitive->getEditableContour();
        if (controlPointIndex >= 0 && controlPointIndex < static_cast<int>(contour.size())) {
            m_deletedPosition = contour[controlPointIndex];
        }
    }
}

void DeleteMaskControlPointCommand::execute()
{
    if (!m_executed && m_imagePrimitive) {
        qDebug() << "Executing DeleteMaskControlPointCommand";
        m_imagePrimitive->deleteControlPoint(m_controlPointIndex);
        m_executed = true;
    }
}

void DeleteMaskControlPointCommand::undo()
{
    if (m_executed && m_imagePrimitive) {
        qDebug() << "Undoing DeleteMaskControlPointCommand";
        // m_deletedPosition is in image space; insertControlPoint expects world
        // coordinates, so convert using the primitive's image->world transform.
        // Insert after (index-1) so the point lands back at its original index.
        int afterIndex = m_controlPointIndex - 1;
        m_imagePrimitive->insertControlPoint(
            afterIndex, m_imagePrimitive->imageToWorld(m_deletedPosition));
        m_executed = false;
    }
}

QString DeleteMaskControlPointCommand::description() const
{
    return QString("Delete Mask Point %1").arg(m_controlPointIndex);
}

// SelectMaskCandidateCommand Implementation
SelectMaskCandidateCommand::SelectMaskCandidateCommand(ImagePrimitive* imagePrimitive, int oldIndex, int newIndex)
    : m_imagePrimitive(imagePrimitive)
    , m_oldIndex(oldIndex)
    , m_newIndex(newIndex)
{
}

void SelectMaskCandidateCommand::execute()
{
    if (m_imagePrimitive) {
        qDebug() << "Executing SelectMaskCandidateCommand";
        m_imagePrimitive->selectMaskCandidate(m_newIndex);
    }
}

void SelectMaskCandidateCommand::undo()
{
    if (m_imagePrimitive) {
        qDebug() << "Undoing SelectMaskCandidateCommand";
        m_imagePrimitive->selectMaskCandidate(m_oldIndex);
    }
}

QString SelectMaskCandidateCommand::description() const
{
    return "Select Mask Candidate";
}

