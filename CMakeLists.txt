cmake_minimum_required(VERSION 3.20)

project(DrawingStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core Network Concurrent OpenGL OpenGLWidgets)
find_package(Qt6 OPTIONAL_COMPONENTS Pdf)

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
    src/ImageAdjustments.cpp
    src/ImageAdjustments_Part2.cpp
    src/DrawingCanvas.cpp
    src/DrawingPrimitive.cpp
    src/ImagePrimitive.cpp
    src/BrushStrokePrimitive.cpp
    src/PropertyPanel.cpp
    src/DrawingProject.cpp
    src/Layer.cpp
    src/LayerManager.cpp
    src/LayerPanel.cpp
    src/ColorPalette.cpp
    src/CanvasRenderer.cpp
    src/SAM2Client.cpp
    src/EdgeSelectionTool.cpp
    src/CanvasSnapshot.cpp
    src/CommandManager.cpp
    src/Commands.cpp
    src/MaskCache.cpp
    src/ColorfulSpinner.cpp
    src/SpinnerDialog.cpp
    src/ShadowGenerator.cpp
    src/AdvancedTextEditor.cpp
    src/ClassicTextTool.cpp
    src/SimpleTextPanel.cpp
    src/ToolSuggestionService.cpp
    src/DiffusionHelper.cpp
    src/CoreMLDiffusionHelper.cpp
    src/RemoteSDHelper.cpp
    src/GridManager.cpp
    include/GridManager.h
    src/SelectionManager.cpp
    include/SelectionManager.h
    src/CommandServer.cpp
    src/LineExtractor.cpp
    src/DXFExporter.cpp
    src/LineExtractionDialog.cpp
    src/SAM2ServiceManager.cpp
    src/AISettingsDialog.cpp
    src/AIImageClient.cpp
    src/AIProviderCatalog.cpp
    src/AIGenerateDialog.cpp
    src/AICompositeDialog.cpp
    src/CompositeHelper.cpp
)
set(HEADERS
    include/MainWindow.h
    include/CanvasSnapshot.h
    include/CommandManager.h
    include/Command.h
    include/Commands.h
    include/DrawingCanvas.h
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
)
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

# Create include directory
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/include)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/resources)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/resources/icons)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)

# Add tests
enable_testing()
add_subdirectory(tests)
