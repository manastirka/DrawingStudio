cmake_minimum_required(VERSION 3.16)
project(DrawingStudioTests)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Test Gui Network OpenGL OpenGLWidgets Widgets)

# DrawingPrimitive split (E21) — use for any test that links primitives
set(DRAWING_PRIMITIVE_SOURCES
    ../src/DrawingPrimitive.cpp
    ../src/DrawingPrimitiveShapes.cpp
    ../src/DrawingPrimitiveCurves.cpp
    ../src/DrawingPrimitiveAnnot.cpp
)

# ImagePrimitive split (E22)
set(IMAGE_PRIMITIVE_SOURCES
    ../src/ImagePrimitive.cpp
    ../src/ImagePrimitiveDetect.cpp
    ../src/ImagePrimitiveMask.cpp
)

# LineExtractor split (E25)
set(LINE_EXTRACTOR_SOURCES
    ../src/LineExtractor.cpp
    ../src/LineExtractorImage.cpp
    ../src/LineExtractorGeometry.cpp
)

# GridManager Test
add_executable(tst_GridManager 
    tst_GridManager.cpp 
    ../src/GridManager.cpp 
    ../src/CanvasRenderer.cpp
    ../include/GridManager.h
)
target_include_directories(tst_GridManager PRIVATE ../include)
target_link_libraries(tst_GridManager PRIVATE Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_GridManager COMMAND tst_GridManager)

# SelectionManager Test
set(TEST_SELECTION_SOURCES
    tst_SelectionManager.cpp
    ../src/SelectionManager.cpp
    ../src/CanvasRenderer.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../include/SelectionManager.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)

add_executable(tst_SelectionManager ${TEST_SELECTION_SOURCES})
target_include_directories(tst_SelectionManager PRIVATE ../include)
target_link_libraries(tst_SelectionManager PRIVATE Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test Qt6::Network Qt6::Widgets)
add_test(NAME tst_SelectionManager COMMAND tst_SelectionManager)

# YOLO Integration Test
add_executable(tst_YOLOIntegration
    tst_YOLOIntegration.cpp
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
target_include_directories(tst_YOLOIntegration PRIVATE ../include)
target_link_libraries(tst_YOLOIntegration PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Test)
add_test(NAME tst_YOLOIntegration COMMAND tst_YOLOIntegration)

# Atomic and validated SAM mask cache persistence
add_executable(tst_MaskCache
    tst_MaskCache.cpp
    ../src/MaskCache.cpp
    ../include/MaskCache.h
)
target_include_directories(tst_MaskCache PRIVATE ../include)
target_link_libraries(tst_MaskCache PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME tst_MaskCache COMMAND tst_MaskCache)

# Line Extraction & DXF Export Test
add_executable(tst_LineExtraction
    test_line_extraction.cpp
    ${LINE_EXTRACTOR_SOURCES}
    ../src/DXFExporter.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/LineExtractor.h
    ../include/DXFExporter.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
target_include_directories(tst_LineExtraction PRIVATE ../include)
target_compile_definitions(tst_LineExtraction PRIVATE SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tst_LineExtraction PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets)
add_test(NAME tst_LineExtraction COMMAND tst_LineExtraction)

# CommandManager dirty-flag / undo stack
add_executable(tst_CommandManager
    tst_CommandManager.cpp
    ../src/CommandManager.cpp
    ../include/CommandManager.h
    ../include/Command.h
)
target_include_directories(tst_CommandManager PRIVATE ../include)
target_link_libraries(tst_CommandManager PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME tst_CommandManager COMMAND tst_CommandManager)

# CanvasGeometry pure helpers
add_executable(tst_CanvasGeometry
    tst_CanvasGeometry.cpp
    ../src/CanvasGeometry.cpp
    ../include/CanvasGeometry.h
)
target_include_directories(tst_CanvasGeometry PRIVATE ../include)
target_link_libraries(tst_CanvasGeometry PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME tst_CanvasGeometry COMMAND tst_CanvasGeometry)

# CanvasZoom pure helpers (E5)
add_executable(tst_CanvasZoom
    tst_CanvasZoom.cpp
    ../src/CanvasZoom.cpp
    ../include/CanvasZoom.h
)
target_include_directories(tst_CanvasZoom PRIVATE ../include)
target_link_libraries(tst_CanvasZoom PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME tst_CanvasZoom COMMAND tst_CanvasZoom)

# CanvasCoords pure helpers (E7 / E12)
add_executable(tst_CanvasCoords
    tst_CanvasCoords.cpp
    ../src/CanvasCoords.cpp
    ../include/CanvasCoords.h
)
target_include_directories(tst_CanvasCoords PRIVATE ../include)
target_link_libraries(tst_CanvasCoords PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME tst_CanvasCoords COMMAND tst_CanvasCoords)

# LineMoveOps pure helpers (E7) — needs LinePrimitive from DrawingPrimitive
set(TEST_LINE_MOVE_SOURCES
    tst_LineMoveOps.cpp
    ../src/LineMoveOps.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/LineMoveOps.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
add_executable(tst_LineMoveOps ${TEST_LINE_MOVE_SOURCES})
target_include_directories(tst_LineMoveOps PRIVATE ../include)
target_link_libraries(tst_LineMoveOps PRIVATE Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test Qt6::Network Qt6::Widgets)
add_test(NAME tst_LineMoveOps COMMAND tst_LineMoveOps)

# SelectionPolicy (E8)
set(TEST_SELECTION_POLICY_SOURCES
    tst_SelectionPolicy.cpp
    ../src/SelectionPolicy.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/SelectionPolicy.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
add_executable(tst_SelectionPolicy ${TEST_SELECTION_POLICY_SOURCES})
target_include_directories(tst_SelectionPolicy PRIVATE ../include)
target_link_libraries(tst_SelectionPolicy PRIVATE Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test Qt6::Network Qt6::Widgets)
add_test(NAME tst_SelectionPolicy COMMAND tst_SelectionPolicy)

# ControlPointHit pure helpers (E8)
add_executable(tst_ControlPointHit
    tst_ControlPointHit.cpp
    ../src/ControlPointHit.cpp
    ../include/ControlPointHit.h
)
target_include_directories(tst_ControlPointHit PRIVATE ../include)
target_link_libraries(tst_ControlPointHit PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME tst_ControlPointHit COMMAND tst_ControlPointHit)

# GroupOps pure helpers (E9)
set(TEST_GROUP_OPS_SOURCES
    tst_GroupOps.cpp
    ../src/GroupOps.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/GroupOps.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
add_executable(tst_GroupOps ${TEST_GROUP_OPS_SOURCES})
target_include_directories(tst_GroupOps PRIVATE ../include)
target_link_libraries(tst_GroupOps PRIVATE Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test Qt6::Network Qt6::Widgets)
add_test(NAME tst_GroupOps COMMAND tst_GroupOps)

# TextResizeOps pure helpers (E10)
set(TEST_TEXT_RESIZE_SOURCES
    tst_TextResizeOps.cpp
    ../src/TextResizeOps.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/TextResizeOps.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
add_executable(tst_TextResizeOps ${TEST_TEXT_RESIZE_SOURCES})
target_include_directories(tst_TextResizeOps PRIVATE ../include)
target_link_libraries(tst_TextResizeOps PRIVATE Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test Qt6::Network Qt6::Widgets)
add_test(NAME tst_TextResizeOps COMMAND tst_TextResizeOps)

# InteractionCancel pure helpers (E10)
add_executable(tst_InteractionCancel
    tst_InteractionCancel.cpp
    ../src/InteractionCancel.cpp
    ../include/InteractionCancel.h
)
target_include_directories(tst_InteractionCancel PRIVATE ../include)
target_link_libraries(tst_InteractionCancel PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_InteractionCancel COMMAND tst_InteractionCancel)

# InteractionPhase pure helpers (E11)
add_executable(tst_InteractionPhase
    tst_InteractionPhase.cpp
    ../src/InteractionPhase.cpp
    ../include/InteractionPhase.h
)
target_include_directories(tst_InteractionPhase PRIVATE ../include)
target_link_libraries(tst_InteractionPhase PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_InteractionPhase COMMAND tst_InteractionPhase)

# ToolSwitchOps pure helpers (E13)
add_executable(tst_ToolSwitchOps
    tst_ToolSwitchOps.cpp
    ../src/ToolSwitchOps.cpp
    ../include/ToolSwitchOps.h
    ../include/DrawingTool.h
)
target_include_directories(tst_ToolSwitchOps PRIVATE ../include)
target_link_libraries(tst_ToolSwitchOps PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_ToolSwitchOps COMMAND tst_ToolSwitchOps)

# PrimitiveJsonOps pure helpers (E14)
set(TEST_PRIMITIVE_JSON_SOURCES
    tst_PrimitiveJsonOps.cpp
    ../src/PrimitiveJsonOps.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/PrimitiveJsonOps.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
add_executable(tst_PrimitiveJsonOps ${TEST_PRIMITIVE_JSON_SOURCES})
target_include_directories(tst_PrimitiveJsonOps PRIVATE ../include)
target_link_libraries(tst_PrimitiveJsonOps PRIVATE Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test Qt6::Network Qt6::Widgets)
add_test(NAME tst_PrimitiveJsonOps COMMAND tst_PrimitiveJsonOps)

# Primitive toJson/createFromJson round-trip (week-3 safety)
add_executable(tst_PrimitiveRoundTrip
    tst_PrimitiveRoundTrip.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
target_include_directories(tst_PrimitiveRoundTrip PRIVATE ../include)
target_link_libraries(tst_PrimitiveRoundTrip PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_PrimitiveRoundTrip COMMAND tst_PrimitiveRoundTrip)

# Fake / validation paths for AIImageClient (no network)
add_executable(tst_AIImageClient
    tst_AIImageClient.cpp
    ../src/AIImageClient.cpp
    ../src/AIImageClientNanoBanana.cpp
    ../src/AIImageClientProviders.cpp
    ../src/AISettingsDialog.cpp
    ../include/AIImageClient.h
    ../include/AISettingsDialog.h
)
target_include_directories(tst_AIImageClient PRIVATE ../include)
target_link_libraries(tst_AIImageClient PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Test)
add_test(NAME tst_AIImageClient COMMAND tst_AIImageClient)
set_tests_properties(tst_AIImageClient PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# UnitsConverter
add_executable(tst_UnitsConverter
    tst_UnitsConverter.cpp
    ../src/UnitsConverter.cpp
    ../include/UnitsConverter.h
)
target_include_directories(tst_UnitsConverter PRIVATE ../include)
target_link_libraries(tst_UnitsConverter PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_UnitsConverter COMMAND tst_UnitsConverter)

# MagneticSnap + SmartDrawingConstraints
add_executable(tst_MagneticSnap
    tst_MagneticSnap.cpp
    ../src/MagneticSnap.cpp
    ../src/SmartDrawingConstraints.cpp
    ../include/MagneticSnap.h
    ../include/SmartDrawingConstraints.h
)
target_include_directories(tst_MagneticSnap PRIVATE ../include)
target_link_libraries(tst_MagneticSnap PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME tst_MagneticSnap COMMAND tst_MagneticSnap)

# PaperModel
add_executable(tst_PaperModel
    tst_PaperModel.cpp
    ../src/PaperModel.cpp
    ../src/CanvasRenderer.cpp
    ../include/PaperModel.h
    ../include/CanvasRenderer.h
)
target_include_directories(tst_PaperModel PRIVATE ../include)
target_link_libraries(tst_PaperModel PRIVATE Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_PaperModel COMMAND tst_PaperModel)

# OverlayRenderer
add_executable(tst_OverlayRenderer
    tst_OverlayRenderer.cpp
    ../src/OverlayRenderer.cpp
    ../include/OverlayRenderer.h
)
target_include_directories(tst_OverlayRenderer PRIVATE ../include)
target_link_libraries(tst_OverlayRenderer PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME tst_OverlayRenderer COMMAND tst_OverlayRenderer)

# SelectionTransform handle geometry
add_executable(tst_SelectionTransform
    tst_SelectionTransform.cpp
    ../src/SelectionTransform.cpp
    ../include/SelectionTransform.h
)
target_include_directories(tst_SelectionTransform PRIVATE ../include)
target_link_libraries(tst_SelectionTransform PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME tst_SelectionTransform COMMAND tst_SelectionTransform)

# ObjectLayoutOps align / distribute (pure layout math)
add_executable(tst_ObjectLayoutOps
    tst_ObjectLayoutOps.cpp
    ../src/ObjectLayoutOps.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/ObjectLayoutOps.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
target_include_directories(tst_ObjectLayoutOps PRIVATE ../include)
target_link_libraries(tst_ObjectLayoutOps PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_ObjectLayoutOps COMMAND tst_ObjectLayoutOps)

# ---------------------------------------------------------------------------
# Week-1 safety net: LayerManager, CommandServer, ProjectFile, Dispatcher
# ---------------------------------------------------------------------------

# QObject headers needed so AUTOMOC emits vtables for canvas/primitives stack
set(TEST_CANVAS_MOC_HEADERS
    ../include/DrawingCanvas.h
    ../include/LayerManager.h
    ../include/SelectionManager.h
    ../include/GridManager.h
    ../include/CommandManager.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
    ../include/ClassicTextTool.h
    ../include/EdgeSelectionTool.h
    ../include/CanvasSnapshot.h
)

# Drawing canvas + deps (shared by dispatcher / command-server link)
set(TEST_CANVAS_BUNDLE
    ${TEST_CANVAS_MOC_HEADERS}
    ../src/DrawingCanvas.cpp
    ../src/DrawingCanvasDocument.cpp
    ../src/DrawingCanvasFacades.cpp
    ../src/DrawingCanvasHosts.cpp
    ../src/DrawingCanvasInput.cpp
    ../src/DrawingCanvasPaint.cpp
    ../src/AirbrushEngine.cpp
    ../src/AlignmentGuides.cpp
    ../src/BrushStrokeOps.cpp
    ../src/BrushStrokePrimitive.cpp
    ../src/CanvasContextMenu.cpp
    ../src/CanvasCoords.cpp
    ../src/CanvasGeometry.cpp
    ../src/CanvasKeyMap.cpp
    ../src/CanvasPaintPipeline.cpp
    ../src/CanvasPrimitiveOps.cpp
    ../src/CanvasRenderer.cpp
    ../src/CanvasZoom.cpp
    ../src/ControlPointEditController.cpp
    ../src/ControlPointHit.cpp
    ../src/DrawingToolRegistry.cpp
    ../src/GridManager.cpp
    ../src/GroupOps.cpp
    ../src/InteractionCancel.cpp
    ../src/InteractionPhase.cpp
    ../src/Layer.cpp
    ../src/LayerManager.cpp
    ../src/LayerOrderOps.cpp
    ../src/LineMoveOps.cpp
    ../src/MagneticSnap.cpp
    ../src/MoveController.cpp
    ../src/ObjectLayoutOps.cpp
    ../src/OverlayRenderer.cpp
    ../src/PaperModel.cpp
    ../src/PathToolFinalize.cpp
    ../src/PrimitiveJsonOps.cpp
    ../src/RulerRenderer.cpp
    ../src/SelectController.cpp
    ../src/SelectionGestureController.cpp
    ../src/SelectionManager.cpp
    ../src/SelectionPolicy.cpp
    ../src/SelectionTransform.cpp
    ../src/SmartDrawingConstraints.cpp
    ../src/TextRenderer.cpp
    ../src/TextResizeOps.cpp
    ../src/ToolCursor.cpp
    ../src/ToolSwitchOps.cpp
    ../src/UnitsConverter.cpp
    ../src/ViewPanController.cpp
    ../src/ClassicTextTool.cpp
    ../src/ClassicTextToolEditor.cpp
    ../src/ClassicTextToolInput.cpp
    ../src/ClassicTextToolResize.cpp
    ../src/CommandManager.cpp
    ../src/Commands.cpp
    ../src/CommandsLayers.cpp
    ../src/CommandsImageMask.cpp
    ../src/CommandsText.cpp
    ../src/CommandsSnapshot.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/EdgeSelectionTool.cpp
    ../src/DXFExporter.cpp
    ../src/CanvasSnapshot.cpp
    ../src/tools/AngleLineTool.cpp
    ../src/tools/ArcTool.cpp
    ../src/tools/BezierTool.cpp
    ../src/tools/BlurTool.cpp
    ../src/tools/BrushTool.cpp
    ../src/tools/CircleTool.cpp
    ../src/tools/CurveTool.cpp
    ../src/tools/EllipseTool.cpp
    ../src/tools/EraserTool.cpp
    ../src/tools/FillTool.cpp
    ../src/tools/ImageTool.cpp
    ../src/tools/LineTool.cpp
    ../src/tools/MeasureTool.cpp
    ../src/tools/MoveTool.cpp
    ../src/tools/PolygonTool.cpp
    ../src/tools/RectangleTool.cpp
    ../src/tools/SelectTool.cpp
    ../src/tools/SplineTool.cpp
    ../src/tools/TextTool.cpp
)

set(TEST_DISPATCHER_SOURCES
    ../src/AutomationCommandCatalog.cpp
    ../src/DrawingCommandDispatcher.cpp
    ../src/DrawingCommandDispatcherDraw.cpp
    ../src/DrawingCommandDispatcherEdit.cpp
    ../src/DrawingCommandDispatcherIO.cpp
    ../src/DrawingCommandDispatcherImage.cpp
    ../src/ImageToDrawingEngine.cpp
    ../src/ImageToDrawingEngineMosaic.cpp
    ../src/ImageToDrawingEngineTrace.cpp
    ../src/ImageToDrawingEnginePhotoCopy.cpp
)

# Shared canvas stack — compile once (was 3× duplicate builds; OOMs GHA macOS).
add_library(ds_test_canvas STATIC ${TEST_CANVAS_BUNDLE})
target_include_directories(ds_test_canvas PUBLIC ../include)
target_link_libraries(ds_test_canvas PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets)

# Image resize handles, positive geometry, aspect lock, and persistence
add_executable(tst_ImagePrimitiveResize tst_ImagePrimitiveResize.cpp)
target_include_directories(tst_ImagePrimitiveResize PRIVATE ../include)
target_link_libraries(tst_ImagePrimitiveResize PRIVATE
    ds_test_canvas
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_ImagePrimitiveResize COMMAND tst_ImagePrimitiveResize)
set_tests_properties(tst_ImagePrimitiveResize PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# LayerManager (light) — still needs MOC for LayerManager / DrawingPrimitive
add_executable(tst_LayerManager
    tst_LayerManager.cpp
    ../src/LayerManager.cpp
    ../src/Layer.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/LayerManager.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
)
target_include_directories(tst_LayerManager PRIVATE ../include)
target_link_libraries(tst_LayerManager PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_LayerManager COMMAND tst_LayerManager)

# CommandServer HTTP (links canvas symbols referenced by server endpoints)
add_executable(tst_CommandServer
    tst_CommandServer.cpp
    ../src/CommandServer.cpp
    ../src/AutomationCommandCatalog.cpp
    ../include/CommandServer.h
    ../include/AutomationCommandCatalog.h
)
target_include_directories(tst_CommandServer PRIVATE ../include)
target_link_libraries(tst_CommandServer PRIVATE
    ds_test_canvas
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_CommandServer COMMAND tst_CommandServer)
set_tests_properties(tst_CommandServer PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Project save/load via LayerManager (+ spinner widgets on load)
add_executable(tst_ProjectFileService
    tst_ProjectFileService.cpp
    stubs/DrawingCanvasProjectStubs.cpp
    ../src/ProjectFileService.cpp
    ../src/LayerManager.cpp
    ../src/Layer.cpp
    ../src/CommandManager.cpp
    ../src/SpinnerDialog.cpp
    ../src/ColorfulSpinner.cpp
    ${DRAWING_PRIMITIVE_SOURCES}
    ${IMAGE_PRIMITIVE_SOURCES}
    ../src/SAM2Client.cpp
    ../src/MaskCache.cpp
    ../src/CanvasRenderer.cpp
    ../include/LayerManager.h
    ../include/CommandManager.h
    ../include/DrawingPrimitive.h
    ../include/ImagePrimitive.h
    ../include/SAM2Client.h
    ../include/MaskCache.h
    ../include/SpinnerDialog.h
    ../include/ColorfulSpinner.h
    ../include/ProjectFileService.h
)
target_include_directories(tst_ProjectFileService PRIVATE ../include)
target_link_libraries(tst_ProjectFileService PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_ProjectFileService COMMAND tst_ProjectFileService)
set_tests_properties(tst_ProjectFileService PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_DrawingCommandDispatcher
    tst_DrawingCommandDispatcher.cpp
    ${TEST_DISPATCHER_SOURCES}
)
target_include_directories(tst_DrawingCommandDispatcher PRIVATE ../include)
target_link_libraries(tst_DrawingCommandDispatcher PRIVATE
    ds_test_canvas
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_DrawingCommandDispatcher COMMAND tst_DrawingCommandDispatcher)
set_tests_properties(tst_DrawingCommandDispatcher PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# AIWorkflowController with injected Fake client (after TEST_CANVAS_BUNDLE)
add_executable(tst_AIWorkflowController
    tst_AIWorkflowController.cpp
    ../src/AIWorkflowController.cpp
    ../src/AIWorkflowControllerComposite.cpp
    ../src/AIWorkflowControllerPlace.cpp
    ../src/AIImageClient.cpp
    ../src/AIImageClientNanoBanana.cpp
    ../src/AIImageClientProviders.cpp
    ../src/AIGenerateDialog.cpp
    ../src/AISettingsDialog.cpp
    ../src/AICompositeDialog.cpp
    ../src/AIProviderCatalog.cpp
    ../src/CompositeHelper.cpp
    ../src/RemoteSDHelper.cpp
    ../include/AIWorkflowController.h
    ../include/AIImageClient.h
    ../include/AIGenerateDialog.h
    ../include/AISettingsDialog.h
    ../include/AICompositeDialog.h
    ../include/RemoteSDHelper.h
)
target_include_directories(tst_AIWorkflowController PRIVATE ../include)
target_link_libraries(tst_AIWorkflowController PRIVATE
    ds_test_canvas
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
add_test(NAME tst_AIWorkflowController COMMAND tst_AIWorkflowController)
set_tests_properties(tst_AIWorkflowController PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Remote Stable Diffusion networking boundaries and async lifecycle
add_executable(tst_RemoteSDHelper
    tst_RemoteSDHelper.cpp
    ../src/RemoteSDHelper.cpp
    ../include/RemoteSDHelper.h
)
target_include_directories(tst_RemoteSDHelper PRIVATE ../include)
target_link_libraries(tst_RemoteSDHelper PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Test)
add_test(NAME tst_RemoteSDHelper COMMAND tst_RemoteSDHelper)
set_tests_properties(tst_RemoteSDHelper PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
