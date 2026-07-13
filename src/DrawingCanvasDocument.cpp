#include "DrawingCanvas.h"

#include "AirbrushEngine.h"
#include "BrushStrokeOps.h"
#include "CanvasPrimitiveOps.h"
#include "ClassicTextTool.h"
#include "Commands.h"
#include "DrawingPrimitive.h"
#include "GroupOps.h"
#include "ImagePrimitive.h"
#include "LayerManager.h"
#include "PathToolFinalize.h"
#include "PrimitiveJsonOps.h"
#include "SelectionManager.h"
#include "ToolCursor.h"
#include "ToolSwitchOps.h"

#include <QDebug>
#include <QTimer>
#include <set>

// Document mutations (refactor E13) — tool switch, add/delete, group.

void DrawingCanvas::setCurrentTool(DrawingTool tool)
{
    if (ToolSwitchOps::isSwitch(m_currentTool, tool)) {
        qDebug() << "Tool changed from" << static_cast<int>(m_currentTool)
                 << "to" << static_cast<int>(tool)
                 << "- resetting drawing state";

        if (m_currentPrimitive && m_isDrawing) {
            qDebug() << "Finalizing incomplete primitive before tool switch";
            if (PathToolFinalize::shouldCommitOnToolSwitch(
                    m_currentPrimitive.get())) {
                m_currentPrimitive->setColor(m_defaultDrawingColor);
                addPrimitiveWithCommand(std::move(m_currentPrimitive));
                qDebug() << "Primitive finalized and added to canvas";
            } else {
                qDebug() << "Discarding incomplete primitive (not enough points)";
                m_currentPrimitive.reset();
            }
        } else {
            m_currentPrimitive.reset();
        }

        m_isDrawing = false;
        m_selectionManager->setIsSelecting(false);
        ToolSwitchOps::resetCreationStages(m_bezierCreationStage,
                                           m_angleLineStage, m_arcStage);

        if (m_isBrushing || m_isBlurring) {
            qDebug() << "setCurrentTool: cleaning up brush/blur state";
            if (m_airbrushTimer) {
                m_airbrushTimer->stop();
            }
            if (auto stroke = BrushStrokeOps::create(
                    m_brushStroke, m_brushParticleScale, m_brushParticleAlpha,
                    m_brushSize, m_brushHardness, m_defaultDrawingColor,
                    m_isBlurring, /*useParticles=*/m_isBrushing)) {
                addPrimitiveWithCommand(std::move(stroke));
            }
            BrushStrokeOps::clearBuffers(&m_brushStroke, &m_brushParticleScale,
                                         &m_brushParticleAlpha);
            AirbrushEngine::clearDrips(m_airbrushState);
            m_isBrushing = false;
            m_isBlurring = false;
        }

        if (ToolSwitchOps::isLeavingTextTool(m_currentTool, tool)) {
            if (m_classicTextTool) {
                m_classicTextTool->deactivate();
            }
        }
    }
    m_currentTool = tool;
    setCursor(ToolCursor::forTool(tool));
    m_showCursorPreview = ToolCursor::showsSizePreview(tool);
}

bool DrawingCanvas::selectPrimitiveById(const QUuid &id)
{
    DrawingPrimitive *prim =
        CanvasPrimitiveOps::findById(m_layerManager, m_primitives, id);
    if (!prim) {
        return false;
    }
    clearSelection();
    prim->setSelected(true);
    addToSelection(prim);
    emit selectionChanged();
    update();
    return true;
}

void DrawingCanvas::addPrimitive(std::unique_ptr<DrawingPrimitive> primitive)
{
    if (!primitive) {
        return;
    }

    if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(primitive.get())) {
        connect(imgPrim, &ImagePrimitive::detectionComplete, this,
                [this, imgPrim]() {
                    qDebug()
                        << "DrawingCanvas: Detection complete signal received";
                    update();
                    emit maskDetectionComplete(imgPrim);
                });
        connect(imgPrim, &ImagePrimitive::detectionFailed, this,
                [this](const QString &error) {
                    qDebug() << "DrawingCanvas: Detection failed:" << error;
                    emit maskDetectionFailed(error);
                });
        connect(imgPrim, &ImagePrimitive::detectionProgress, this,
                [this](int percentage, const QString &message) {
                    emit maskDetectionProgress(percentage, message);
                });
    }

    DrawingPrimitive *primitivePtr = CanvasPrimitiveOps::insert(
        m_layerManager, m_primitives, std::move(primitive));

    if (CanvasPrimitiveOps::shouldAutoSelect(primitivePtr)) {
        qDebug() << "*** AUTO-SELECTING NEWLY CREATED PRIMITIVE ***";
        clearSelection();
        addToSelection(primitivePtr);
        emit selectionChanged();
    }

    update();
}

void DrawingCanvas::addPrimitiveWithCommand(
    std::unique_ptr<DrawingPrimitive> primitive)
{
    if (primitive) {
        qDebug() << "Creating AddPrimitiveCommand for type:"
                 << static_cast<int>(primitive->type());
        auto command =
            std::make_unique<AddPrimitiveCommand>(this, std::move(primitive));
        emit commandRequested(command.release());
    }
}

void DrawingCanvas::addTextPrimitive(TextPrimitive *primitive)
{
    if (primitive) {
        auto uniquePrimitive = std::unique_ptr<DrawingPrimitive>(primitive);
        addPrimitiveWithCommand(std::move(uniquePrimitive));
        update();
    }
}

void DrawingCanvas::deleteSelectedPrimitivesWithCommand()
{
    const auto &selected = m_selectionManager->selectedObjects();
    if (!selected.empty()) {
        qDebug() << "Deleting" << selected.size() << "primitives";
        auto command = std::make_unique<DeletePrimitivesCommand>(this, selected);
        emit commandRequested(command.release());
    }
}

void DrawingCanvas::deleteSelectedPrimitives()
{
    const auto &selected = m_selectionManager->selectedObjects();
    if (selected.empty()) {
        return;
    }

    const std::set<QUuid> idsToDelete =
        CanvasPrimitiveOps::collectIds(selected);
    const bool changed = CanvasPrimitiveOps::eraseByIds(
        m_layerManager, m_primitives, idsToDelete);
    m_selectionManager->clearSelection();
    if (changed) {
        update();
    }
}

void DrawingCanvas::clearPrimitives()
{
    m_selectionManager->clearSelection();
    m_primitives.clear();
    update();
}

void DrawingCanvas::setUnits(Units units)
{
    m_unitsConverter.setUnits(units);
    CanvasPrimitiveOps::syncDimensionUnits(
        m_layerManager, m_primitives, getUnitsString(),
        static_cast<float>(pixelsPerUnit()));
    updateStatusBar();
}

void DrawingCanvas::groupSelected()
{
    // E14: PrimitiveJsonOps snapshots; GroupOps mutates groupIds
    auto selected = selectedObjects();
    if (!GroupOps::canGroup(selected)) {
        return;
    }

    const auto oldStates = PrimitiveJsonOps::snapshotStates(selected);
    if (GroupOps::assignNewGroup(selected).isNull()) {
        return;
    }
    const auto newStates = PrimitiveJsonOps::snapshotStates(selected);

    auto command = std::make_unique<TransformPrimitivesCommand>(
        selected, TransformPrimitivesCommand::Rotate, QStringLiteral("Group"));
    command->storeTransformation(oldStates, newStates);
    emit commandRequested(command.release());
    update();
}

void DrawingCanvas::ungroupSelected()
{
    auto selected = selectedObjects();
    if (!GroupOps::anyGrouped(selected)) {
        return;
    }

    const auto oldStates = PrimitiveJsonOps::snapshotStates(selected);
    GroupOps::clearGroups(selected);
    const auto newStates = PrimitiveJsonOps::snapshotStates(selected);

    auto command = std::make_unique<TransformPrimitivesCommand>(
        selected, TransformPrimitivesCommand::Rotate,
        QStringLiteral("Ungroup"));
    command->storeTransformation(oldStates, newStates);
    emit commandRequested(command.release());
    update();
}

void DrawingCanvas::selectGroupMembers(DrawingPrimitive *seed)
{
    if (!seed || seed->groupId().isNull()) {
        return;
    }
    for (DrawingPrimitive *p : GroupOps::collectMembers(
             m_layerManager, m_primitives, seed->groupId())) {
        p->setSelected(true);
        m_selectionManager->addToSelection(p);
    }
}
