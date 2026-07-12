cmake_minimum_required(VERSION 3.16)
project(DrawingStudioTests)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Test Gui Network OpenGL OpenGLWidgets Widgets)

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
    ../src/DrawingPrimitive.cpp
    ../src/ImagePrimitive.cpp
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
    ../src/LineExtractor.cpp
    ../src/DXFExporter.cpp
    ../src/DrawingPrimitive.cpp
    ../src/ImagePrimitive.cpp
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

