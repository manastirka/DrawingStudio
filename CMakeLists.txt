cmake_minimum_required(VERSION 3.20)

project(DrawingStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_DRAWINGSTUDIO_APP "Build the DrawingStudio GUI app (needs prebuilt SD libs)" ON)
option(BUILD_DRAWINGSTUDIO_TESTS "Build unit tests" ON)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core Network Concurrent OpenGL OpenGLWidgets)
find_package(Qt6 OPTIONAL_COMPONENTS Pdf Test)

# Enable Qt MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Add stable-diffusion.cpp
# Using pre-built library to avoid CMake submodule issues
# Library built in external/stable-diffusion.cpp/build/
set(SD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/stable-diffusion.cpp)
set(SD_LIB_DIR ${CMAKE_SOURCE_DIR}/external/stable-diffusion.cpp/build)
set(SD_GGML_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/stable-diffusion.cpp/ggml/include)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindowConnect.cpp
    src/MainWindowMenus.cpp
    src/MainWindowToolbars.cpp
    src/MainWindowDocks.cpp
    src/MainWindowExportAIText.cpp
    src/MainWindowImageMask.cpp
    src/MainWindowFile.cpp
    src/MainWindowTools.cpp
    src/DrawingCanvas.cpp
    src/DrawingCanvasHosts.cpp
    src/DrawingCanvasInput.cpp
    src/DrawingCanvasPaint.cpp
    src/DrawingCanvasDocument.cpp
    src/DrawingCanvasFacades.cpp
    src/DrawingToolRegistry.cpp
    src/InteractionPhase.cpp
    src/ToolSwitchOps.cpp
    src/PrimitiveJsonOps.cpp
    src/CanvasPrimitiveOps.cpp
    src/CanvasPaintPipeline.cpp
    src/CanvasZoom.cpp
    src/BrushStrokeOps.cpp
    src/CanvasCoords.cpp
    src/LineMoveOps.cpp
    src/SelectionPolicy.cpp
    src/ControlPointHit.cpp
    src/GroupOps.cpp
    src/LayerOrderOps.cpp
    src/TextResizeOps.cpp
    src/InteractionCancel.cpp
    src/CanvasGeometry.cpp
    src/UnitsConverter.cpp
    src/MagneticSnap.cpp
    src/SmartDrawingConstraints.cpp
    src/PaperModel.cpp
    src/RulerRenderer.cpp
    src/OverlayRenderer.cpp
    src/SelectionTransform.cpp
    src/MoveController.cpp
    src/SelectController.cpp
    src/SelectionGestureController.cpp
    src/ControlPointEditController.cpp
    src/ViewPanController.cpp
    src/AirbrushEngine.cpp
    src/AlignmentGuides.cpp
    src/TextRenderer.cpp
    src/ObjectLayoutOps.cpp
    src/ToolCursor.cpp
    src/CanvasKeyMap.cpp
    src/CanvasContextMenu.cpp
    src/PathToolFinalize.cpp
    src/tools/LineTool.cpp
    src/tools/RectangleTool.cpp
    src/tools/CircleTool.cpp
    src/tools/EllipseTool.cpp
    src/tools/MeasureTool.cpp
    src/tools/ArcTool.cpp
    src/tools/AngleLineTool.cpp
    src/tools/BezierTool.cpp
    src/tools/CurveTool.cpp
    src/tools/SplineTool.cpp
    src/tools/PolygonTool.cpp
    src/tools/EraserTool.cpp
    src/tools/FillTool.cpp
    src/tools/BrushTool.cpp
    src/tools/BlurTool.cpp
    src/tools/SelectTool.cpp
    src/tools/MoveTool.cpp
    src/tools/ImageTool.cpp
    src/tools/TextTool.cpp
    src/DrawingPrimitive.cpp
    src/DrawingPrimitiveShapes.cpp
    src/DrawingPrimitiveCurves.cpp
    src/DrawingPrimitiveAnnot.cpp
    src/ImagePrimitive.cpp
    src/ImagePrimitiveDetect.cpp
    src/ImagePrimitiveMask.cpp
    src/BrushStrokePrimitive.cpp
    src/PropertyPanel.cpp
    src/ColorButton.cpp
    src/PropertyPanelTool.cpp
    src/PropertyPanelPrimitive.cpp
    src/PropertyPanelEditors.cpp
    src/DrawingProject.cpp
    src/Layer.cpp
    src/LayerManager.cpp
    src/LayerPanel.cpp
    src/LayerPanelLayerItem.cpp
    src/LayerPanelPrimitiveItem.cpp
    src/ColorPalette.cpp
    src/CanvasRenderer.cpp
    src/SAM2Client.cpp
    src/EdgeSelectionTool.cpp
    src/CanvasSnapshot.cpp
    src/CommandManager.cpp
    src/Commands.cpp
    src/CommandsLayers.cpp
    src/CommandsImageMask.cpp
    src/CommandsText.cpp
    src/CommandsSnapshot.cpp
    src/MaskCache.cpp
    src/ColorfulSpinner.cpp
    src/SpinnerDialog.cpp
    src/ShadowGenerator.cpp
    src/AdvancedTextEditor.cpp
    src/ClassicTextTool.cpp
    src/ClassicTextToolResize.cpp
    src/ClassicTextToolEditor.cpp
    src/ClassicTextToolInput.cpp
    src/SimpleTextPanel.cpp
    src/SimpleTextPanelSetup.cpp
    src/SimpleTextPanelEffects.cpp
    src/ToolSuggestionService.cpp
    src/DiffusionHelper.cpp
    src/CoreMLDiffusionHelper.cpp
    src/RemoteSDHelper.cpp
    src/GridManager.cpp
    include/GridManager.h
    src/SelectionManager.cpp
    include/SelectionManager.h
    src/CommandServer.cpp
    src/AutomationCommandCatalog.cpp
    src/DrawingCommandDispatcher.cpp
    src/DrawingCommandDispatcherDraw.cpp
    src/DrawingCommandDispatcherEdit.cpp
    src/DrawingCommandDispatcherIO.cpp
    src/DrawingCommandDispatcherImage.cpp
    src/FloatingMaskPanel.cpp
    src/ImageAdjustmentController.cpp
    src/ImageAdjustmentControllerBlur.cpp
    src/AIWorkflowController.cpp
    src/AIWorkflowControllerComposite.cpp
    src/AIWorkflowControllerPlace.cpp
    src/ImageExportService.cpp
    src/ProjectFileService.cpp
    src/PrimitivePropertyApplier.cpp
    src/TextEditingController.cpp
    src/ToolOptionsBar.cpp
    src/ToolOptionsBarUpdate.cpp
    src/ToolOptionsBarMaskPresets.cpp
    src/ToolIconProvider.cpp
    src/ImageToDrawingEngine.cpp
    src/ImageToDrawingEngineMosaic.cpp
    src/ImageToDrawingEngineTrace.cpp
    src/ImageToDrawingEnginePhotoCopy.cpp
    src/LineExtractor.cpp
    src/LineExtractorImage.cpp
    src/LineExtractorGeometry.cpp
    src/DXFExporter.cpp
    src/LineExtractionDialog.cpp
    src/SAM2ServiceManager.cpp
    src/AISettingsDialog.cpp
    src/AIImageClient.cpp
    src/AIImageClientNanoBanana.cpp
    src/AIImageClientProviders.cpp
    src/AIProviderCatalog.cpp
    src/AIGenerateDialog.cpp
    src/AICompositeDialog.cpp
    src/CompositeHelper.cpp
    src/MathExpression.cpp
    src/MathFormulaRenderer.cpp
    src/MathGraphRenderer.cpp
    src/MathInsertDialog.cpp
    src/PhysicsCatalog.cpp
    src/PhysicsSolverDialog.cpp
)
set(HEADERS
    include/MainWindow.h
    include/CanvasSnapshot.h
    include/CommandManager.h
    include/Command.h
    include/Commands.h
    include/DrawingCanvas.h
    include/CanvasGeometry.h
    include/UnitsConverter.h
    include/MagneticSnap.h
    include/SmartDrawingConstraints.h
    include/PaperModel.h
    include/RulerRenderer.h
    include/OverlayRenderer.h
    include/SelectionTransform.h
    include/MoveController.h
    include/SelectController.h
    include/SelectionGestureController.h
    include/ControlPointEditController.h
    include/ViewPanController.h
    include/AirbrushEngine.h
    include/AlignmentGuides.h
    include/TextRenderer.h
    include/ObjectLayoutOps.h
    include/DrawingTool.h
    include/ToolCursor.h
    include/CanvasKeyMap.h
    include/CanvasContextMenu.h
    include/PathToolFinalize.h
    include/CanvasPrimitiveOps.h
    include/CanvasPaintPipeline.h
    include/CanvasZoom.h
    include/BrushStrokeOps.h
    include/CanvasCoords.h
    include/LineMoveOps.h
    include/SelectionPolicy.h
    include/ControlPointHit.h
    include/GroupOps.h
    include/LayerOrderOps.h
    include/TextResizeOps.h
    include/InteractionCancel.h
    include/DrawingToolRegistry.h
    include/InteractionPhase.h
    include/ToolSwitchOps.h
    include/PrimitiveJsonOps.h
    include/tools/IDrawingTool.h
    include/tools/LineTool.h
    include/tools/RectangleTool.h
    include/tools/CircleTool.h
    include/tools/EllipseTool.h
    include/tools/MeasureTool.h
    include/tools/ArcTool.h
    include/tools/AngleLineTool.h
    include/tools/BezierTool.h
    include/tools/CurveTool.h
    include/tools/SplineTool.h
    include/tools/PolygonTool.h
    include/tools/EraserTool.h
    include/tools/FillTool.h
    include/tools/BrushTool.h
    include/tools/BlurTool.h
    include/tools/SelectTool.h
    include/tools/MoveTool.h
    include/tools/ImageTool.h
    include/tools/TextTool.h
    include/DrawingPrimitive.h
    include/ImagePrimitive.h
    include/PropertyPanel.h
    include/DrawingProject.h
    include/Layer.h
    include/LayerManager.h
    include/LayerPanel.h
    include/ColorPalette.h
    include/CanvasRenderer.h
    include/SAM2Client.h
    include/EdgeSelectionTool.h
    include/MaskCache.h
    include/ColorfulSpinner.h
    include/SpinnerDialog.h
    include/ShadowGenerator.h
    include/AdvancedTextEditor.h
    include/ClassicTextTool.h
    include/SimpleTextPanel.h
    include/ToolSuggestionService.h
    include/DiffusionHelper.h
    include/CoreMLDiffusionHelper.h
    include/RemoteSDHelper.h
    include/CommandServer.h
    include/AutomationCommandCatalog.h
    include/DrawingCommandDispatcher.h
    include/FloatingMaskPanel.h
    include/ImageAdjustmentController.h
    include/AIWorkflowController.h
    include/ImageExportService.h
    include/ProjectFileService.h
    include/PrimitivePropertyApplier.h
    include/TextEditingController.h
    include/ToolOptionsBar.h
    include/ToolIconProvider.h
    include/ImageToDrawingEngine.h
    include/LineExtractor.h
    include/DXFExporter.h
    include/LineExtractionDialog.h
    include/SAM2ServiceManager.h
    include/AISettingsDialog.h
    include/AIImageClient.h
    include/AIProviderCatalog.h
    include/AIGenerateDialog.h
    include/AICompositeDialog.h
    include/CompositeHelper.h
    include/MathExpression.h
    include/MathFormulaRenderer.h
    include/MathGraphRenderer.h
    include/MathInsertDialog.h
    include/PhysicsCatalog.h
    include/PhysicsSolverDialog.h
)
if(BUILD_DRAWINGSTUDIO_APP)
    # Create executable
    add_executable(DrawingStudio ${SOURCES} ${HEADERS})

    # Include stable-diffusion.cpp headers
    target_include_directories(DrawingStudio PRIVATE
        ${SD_INCLUDE_DIR}
        ${SD_GGML_INCLUDE_DIR}
    )

    # Link Qt libraries and stable-diffusion
    # Metal is DISABLED to prevent crashes - using CPU-only mode
    message(STATUS "Stable Diffusion: CPU-only mode (Metal disabled)")
    target_link_libraries(DrawingStudio PRIVATE
        Qt6::Widgets
        Qt6::Gui
        Qt6::Core
        Qt6::Network
        Qt6::Concurrent
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        ${SD_LIB_DIR}/libstable-diffusion.a
        ${SD_LIB_DIR}/ggml/src/libggml.a
        ${SD_LIB_DIR}/ggml/src/libggml-cpu.a
        ${SD_LIB_DIR}/ggml/src/ggml-blas/libggml-blas.a
        ${SD_LIB_DIR}/ggml/src/libggml-base.a
        "-framework Foundation"
        "-framework Accelerate"
    )

    # Link PDF support if available
    if(TARGET Qt6::Pdf)
        target_link_libraries(DrawingStudio PRIVATE Qt6::Pdf)
        target_compile_definitions(DrawingStudio PRIVATE HAVE_QT_PDF)
    endif()
else()
    message(STATUS "BUILD_DRAWINGSTUDIO_APP=OFF — skipping main app / SD link")
endif()

# Create include directory
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/include)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/resources)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/resources/icons)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)

# Add tests
if(BUILD_DRAWINGSTUDIO_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
