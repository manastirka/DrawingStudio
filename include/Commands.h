#pragma once

#include "Command.h"
#include "CanvasSnapshot.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "Layer.h"
#include <memory>
#include <vector>
#include <QJsonObject>
#include <QPointer>
#include <QUuid>

class LayerManager;

/**
 * Command to add a primitive to the canvas
 */
class AddPrimitiveCommand : public Command {
public:
    AddPrimitiveCommand(DrawingCanvas* canvas, std::unique_ptr<DrawingPrimitive> primitive);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    DrawingCanvas* m_canvas;
    std::unique_ptr<DrawingPrimitive> m_primitive;
    QJsonObject m_primitiveData; // For undo
    bool m_executed;
};

/**
 * Command to delete primitives from canvas
 */
class DeletePrimitivesCommand : public Command {
public:
    DeletePrimitivesCommand(DrawingCanvas* canvas, const std::vector<DrawingPrimitive*>& primitives);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    struct DeletedPrimitiveInfo {
        QUuid primitiveId;
        QUuid layerId;
        int index = -1;
        std::unique_ptr<DrawingPrimitive> primitive;
    };

    DrawingCanvas* m_canvas;
    std::vector<DeletedPrimitiveInfo> m_deletedPrimitives;
    QString m_commandDescription;
    bool m_executed = false;
};

/**
 * Command to modify primitive properties
 */
class ModifyPrimitiveCommand : public Command {
public:
    ModifyPrimitiveCommand(DrawingPrimitive* primitive, const QString& propertyName);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    bool canMergeWith(const Command* other) const override;
    void mergeWith(const Command* other) override;
    
    // Store different types of property changes
    void storeColorChange(const QColor& oldColor, const QColor& newColor);
    void storeLineWidthChange(float oldWidth, float newWidth);
    void storePositionChange(const QVector2D& oldPos, const QVector2D& newPos);
    void storeControlPointChange(int index, const QVector2D& oldPos, const QVector2D& newPos);
    void storePropertyChange(const QJsonObject& oldState, const QJsonObject& newState);

    // Convenience helpers for operations that directly mutate a primitive first
    // (e.g., Fill tool) and then push an undo command.
    void captureNewState();
    void markAlreadyApplied() { m_alreadyApplied = true; }
    bool hasStateChange() const;
    
private:
    DrawingPrimitive* m_primitive;
    QString m_propertyName;
    QJsonObject m_oldState;
    QJsonObject m_newState;
    bool m_executed;
    bool m_alreadyApplied = false;
};

/**
 * Command for moving primitives
 */
class MovePrimitivesCommand : public Command {
public:
    MovePrimitivesCommand(const std::vector<DrawingPrimitive*>& primitives, const QVector2D& offset);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    bool canMergeWith(const Command* other) const override;
    void mergeWith(const Command* other) override;
    
private:
    std::vector<QPointer<DrawingPrimitive>> m_primitives;
    QVector2D m_offset;
    QVector2D m_totalOffset; // For merging
};

/**
 * Command for transforming primitives (scale, rotate)
 */
class TransformPrimitivesCommand : public Command {
public:
    enum TransformType {
        Scale,
        Rotate,
        Resize
    };
    
    TransformPrimitivesCommand(const std::vector<DrawingPrimitive*>& primitives, 
                              TransformType type, const QString& description);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
    void storeTransformation(const std::vector<QJsonObject>& oldStates, 
                           const std::vector<QJsonObject>& newStates);
    
private:
    std::vector<QPointer<DrawingPrimitive>> m_primitives;
    std::vector<QJsonObject> m_oldStates;
    std::vector<QJsonObject> m_newStates;
    TransformType m_transformType;
    QString m_commandDescription;
    bool m_executed;
};

/**
 * Command for modifying control points
 */
class ModifyControlPointCommand : public Command {
public:
    ModifyControlPointCommand(DrawingPrimitive* primitive, int controlPointIndex,
                             const QVector2D& oldPosition, const QVector2D& newPosition);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    bool canMergeWith(const Command* other) const override;
    void mergeWith(const Command* other) override;
    
private:
    DrawingPrimitive* m_primitive;
    int m_controlPointIndex;
    QVector2D m_oldPosition;
    QVector2D m_newPosition;
};

/**
 * Command for layer operations
 */
class LayerCommand : public Command {
public:
    enum LayerOperation {
        CreateLayer,
        DeleteLayer,
        MoveToLayer,
        ReorderLayer
    };
    
    LayerCommand(LayerManager* layerManager, LayerOperation operation, const QString& description);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
    // For different layer operations
    void setCreateLayerData(const QString& layerName);
    void setDeleteLayerData(const QUuid& layerId, const QJsonObject& layerData);
    void setMoveToLayerData(const std::vector<DrawingPrimitive*>& primitives, 
                           const QUuid& fromLayerId, const QUuid& toLayerId);
    void setReorderLayerData(const QUuid& layerId, int oldIndex, int newIndex);
    
private:
    LayerManager* m_layerManager;
    LayerOperation m_operation;
    QString m_commandDescription;
    
    // Operation-specific data
    QString m_layerName;
    QUuid m_layerId;
    QUuid m_fromLayerId;
    QUuid m_toLayerId;
    QJsonObject m_layerData;
    std::vector<QPointer<DrawingPrimitive>> m_primitives;
    int m_oldIndex;
    int m_newIndex;
    bool m_executed;
};

/**
 * Command to create a new layer
 */
class CreateLayerCommand : public Command {
public:
    CreateLayerCommand(LayerManager* layerManager, const QString& name = QString());

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    LayerManager* m_layerManager;
    QString m_layerName;
    QUuid m_layerId;
    size_t m_insertIndex = 0;
    QUuid m_previousActiveLayerId;
    std::unique_ptr<class Layer> m_layer;
    bool m_executed = false;
};

/**
 * Command to delete a layer (preserves the layer object for undo/redo)
 */
class DeleteLayerCommand : public Command {
public:
    DeleteLayerCommand(LayerManager* layerManager, const QUuid& layerId);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    LayerManager* m_layerManager;
    QUuid m_layerId;
    size_t m_removedIndex = 0;
    QUuid m_previousActiveLayerId;
    bool m_deletedWasActive = false;
    std::unique_ptr<class Layer> m_removedLayer;
    bool m_executed = false;
};


/**
 * Command to reorder a layer within the stack
 */
class ReorderLayerCommand : public Command {
public:
    ReorderLayerCommand(LayerManager* layerManager, const QUuid& layerId,
                        int oldIndex, int newIndex, const QString& description);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    LayerManager* m_layerManager;
    QUuid m_layerId;
    int m_oldIndex = -1;
    int m_newIndex = -1;
    QString m_description;
    bool m_executed = false;
};

/**
 * Command to set a layer's visibility
 */
class SetLayerVisibilityCommand : public Command {
public:
    SetLayerVisibilityCommand(LayerManager* layerManager, const QUuid& layerId,
                              bool oldVisible, bool newVisible);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    LayerManager* m_layerManager;
    QUuid m_layerId;
    bool m_oldVisible = true;
    bool m_newVisible = true;
    bool m_executed = false;
};

/**
 * Command to set a layer's locked state
 */
class SetLayerLockCommand : public Command {
public:
    SetLayerLockCommand(LayerManager* layerManager, const QUuid& layerId,
                        bool oldLocked, bool newLocked);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    LayerManager* m_layerManager;
    QUuid m_layerId;
    bool m_oldLocked = false;
    bool m_newLocked = false;
    bool m_executed = false;
};

/**
 * Command to set a layer's opacity
 */
class SetLayerOpacityCommand : public Command {
public:
    SetLayerOpacityCommand(LayerManager* layerManager, const QUuid& layerId,
                           float oldOpacity, float newOpacity);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    LayerManager* m_layerManager;
    QUuid m_layerId;
    float m_oldOpacity = 1.0f;
    float m_newOpacity = 1.0f;
    bool m_executed = false;
};

/**
 * Command to rename a layer
 */
class RenameLayerCommand : public Command {
public:
    RenameLayerCommand(LayerManager* layerManager, const QUuid& layerId,
                       const QString& oldName, const QString& newName);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    LayerManager* m_layerManager;
    QUuid m_layerId;
    QString m_oldName;
    QString m_newName;
    bool m_executed = false;
};

/**
 * Command to set blend mode
 */
class SetLayerBlendModeCommand : public Command {
public:
    SetLayerBlendModeCommand(LayerManager* layerManager, const QUuid& layerId,
                             int oldBlendMode, int newBlendMode);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    LayerManager* m_layerManager;
    QUuid m_layerId;
    int m_oldBlendMode = 0;
    int m_newBlendMode = 0;
    bool m_executed = false;
};

/**
 * Command to show/hide all layers
 */
class SetAllLayersVisibilityCommand : public Command {
public:
    SetAllLayersVisibilityCommand(LayerManager* layerManager, bool newVisible);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    LayerManager* m_layerManager;
    bool m_newVisible = true;
    std::vector<QUuid> m_layerIds;
    std::vector<bool> m_oldVisibilities;
    bool m_executed = false;
};

/**
 * Command to set a primitive's visibility
 */
class SetPrimitiveVisibilityCommand : public Command {
public:
    SetPrimitiveVisibilityCommand(DrawingPrimitive* primitive, bool oldVisible, bool newVisible);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    DrawingPrimitive* m_primitive;
    bool m_oldVisible = true;
    bool m_newVisible = true;
    bool m_executed = false;
};

/**
 * Command for brush strokes (special handling for continuous drawing)
 */
class BrushStrokeCommand : public Command {
public:
    BrushStrokeCommand(DrawingCanvas* canvas, std::unique_ptr<DrawingPrimitive> brushStroke);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    bool canMergeWith(const Command* other) const override;
    void mergeWith(const Command* other) override;
    
    void addPoint(const QVector2D& point);
    
private:
    DrawingCanvas* m_canvas;
    std::unique_ptr<DrawingPrimitive> m_brushStroke;
    QJsonObject m_strokeData;
    bool m_executed;
};

/**
 * Command for modifying image mask control points
 */
class ModifyMaskControlPointCommand : public Command {
public:
    ModifyMaskControlPointCommand(class ImagePrimitive* imagePrimitive, int controlPointIndex,
                                 const QPointF& oldPosition, const QPointF& newPosition);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    bool canMergeWith(const Command* other) const override;
    void mergeWith(const Command* other) override;
    
private:
    ImagePrimitive* m_imagePrimitive;
    int m_controlPointIndex;
    QPointF m_oldPosition;
    QPointF m_newPosition;
};

/**
 * Command for inserting mask control points
 */
class InsertMaskControlPointCommand : public Command {
public:
    InsertMaskControlPointCommand(class ImagePrimitive* imagePrimitive, int afterIndex,
                                 const QPointF& position);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    ImagePrimitive* m_imagePrimitive;
    int m_afterIndex;
    QPointF m_position;
    bool m_executed;
};

/**
 * Command for deleting mask control points
 */
class DeleteMaskControlPointCommand : public Command {
public:
    DeleteMaskControlPointCommand(class ImagePrimitive* imagePrimitive, int controlPointIndex);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    ImagePrimitive* m_imagePrimitive;
    int m_controlPointIndex;
    QPointF m_deletedPosition;
    bool m_executed;
};

/**
 * Command for mask candidate selection changes
 */
class SelectMaskCandidateCommand : public Command {
public:
    SelectMaskCandidateCommand(class ImagePrimitive* imagePrimitive, int oldIndex, int newIndex);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    ImagePrimitive* m_imagePrimitive;
    int m_oldIndex;
    int m_newIndex;
};

/**
 * Command for extracting subject from image (cutout operation)
 */
class ExtractSubjectCommand : public Command {
public:
    ExtractSubjectCommand(DrawingCanvas* canvas, class ImagePrimitive* sourceImage);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    DrawingCanvas* m_canvas;
    ImagePrimitive* m_sourceImage;
    std::unique_ptr<ImagePrimitive> m_extractedImage;
    QUuid m_extractedImageId;
    bool m_executed;
};

/**
 * Command for editing text properties
 */
class EditTextCommand : public Command {
public:
    EditTextCommand(class TextPrimitive* textPrimitive, const QString& description);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
    void storeOldState(const QJsonObject& oldState);
    void storeNewState(const QJsonObject& newState);
    void markAlreadyApplied() { m_executed = true; }
    
private:
    TextPrimitive* m_textPrimitive;
    QJsonObject m_oldState;
    QJsonObject m_newState;
    QString m_description;
    bool m_executed;
};

/**
 * Command for importing images
 */
class ImportImageCommand : public Command {
public:
    ImportImageCommand(DrawingCanvas* canvas, std::unique_ptr<class ImagePrimitive> image);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    DrawingCanvas* m_canvas;
    std::unique_ptr<ImagePrimitive> m_image;
    QUuid m_imageId;
    bool m_executed;
};

/**
 * Create a layer and populate it with floor-plan line segments (undo removes the layer).
 */
class ExtractFloorPlanLinesCommand : public Command {
public:
    struct Segment {
        QVector2D start;
        QVector2D end;
    };

    ExtractFloorPlanLinesCommand(DrawingCanvas *canvas,
                                 LayerManager *layerManager,
                                 QString layerName,
                                 std::vector<Segment> segments,
                                 QColor color = Qt::black,
                                 float lineWidth = 1.0f);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    DrawingCanvas *m_canvas = nullptr;
    LayerManager *m_layerManager = nullptr;
    QString m_layerName;
    std::vector<Segment> m_segments;
    QColor m_color;
    float m_lineWidth = 1.0f;
    QUuid m_layerId;
    size_t m_insertIndex = 0;
    QUuid m_previousActiveLayerId;
    std::unique_ptr<Layer> m_layer;
    bool m_executed = false;
};

/**
 * Command for changing mask contour smoothness
 */
class SetContourSmoothnessCommand : public Command {
public:
    SetContourSmoothnessCommand(class ImagePrimitive* imagePrimitive, int oldSmoothness, int newSmoothness);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    ImagePrimitive* m_imagePrimitive;
    int m_oldSmoothness;
    int m_newSmoothness;
};

/**
 * Command for resizing text primitives
 */
class ResizeTextCommand : public Command {
public:
    ResizeTextCommand(class TextPrimitive* textPrimitive);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
    void storeOldState(const QVector2D& position, float width, float height, float fontSize);
    void storeNewState(const QVector2D& position, float width, float height, float fontSize);
    
private:
    TextPrimitive* m_textPrimitive;
    QVector2D m_oldPosition;
    float m_oldWidth;
    float m_oldHeight;
    float m_oldFontSize;
    QVector2D m_newPosition;
    float m_newWidth;
    float m_newHeight;
    float m_newFontSize;
    bool m_executed;
};


/**
 * Snapshot-based command for bulk operations (image render effects, auto-trace)
 * where tracking individual primitives would be impractical.
 *
 * Usage: construct BEFORE mutating the document (captures the "before" state),
 * mutate, call captureAfterState(), then push via
 * CommandManager::addCommandWithoutExecuting() since the mutation is already
 * applied. execute() re-applies the "after" state on redo.
 */
class SnapshotCommand : public Command {
public:
    SnapshotCommand(LayerManager* layerManager, const QString& description);

    void captureAfterState();

    void execute() override;
    void undo() override;
    QString description() const override { return m_description; }

private:
    LayerManager* m_layerManager;
    QString m_description;
    CanvasSnapshot m_before;
    CanvasSnapshot m_after;
    bool m_afterCaptured = false;
};

/**
 * Compound command for grouping multiple operations
 */
class CompoundCommand : public Command {
public:
    CompoundCommand(const QString& description);
    ~CompoundCommand();
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
    void addCommand(std::unique_ptr<Command> command);
    bool isEmpty() const { return m_commands.empty(); }
    void markAlreadyApplied() { m_executed = true; }
    
private:
    std::vector<std::unique_ptr<Command>> m_commands;
    QString m_description;
    bool m_executed;
};
