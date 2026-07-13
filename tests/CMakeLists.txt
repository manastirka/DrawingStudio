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

