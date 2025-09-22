cmake_minimum_required(VERSION 3.20)
project(GuitarBuilder VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)
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

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/DrawingCanvas.cpp
    src/DrawingPrimitive.cpp
    src/GuitarComponent.cpp
    src/ComponentLibrary.cpp
    src/GuitarProject.cpp
    src/PropertyPanel.cpp
    src/GuitarDimensions.cpp
    src/BlueprintManager.cpp
    src/BlueprintParser.cpp
    src/GuitarSpecsDatabase.cpp
    src/GuitarSpecsBrowserDialog.cpp
    src/ComponentDatabase.cpp
    src/SimpleComponentPropertiesDialog.cpp
)

set(HEADERS
    include/MainWindow.h
    include/DrawingCanvas.h
    include/DrawingPrimitive.h
    include/GuitarComponent.h
    include/ComponentLibrary.h
    include/GuitarProject.h
    include/PropertyPanel.h
    include/GuitarDimensions.h
    include/BlueprintManager.h
    include/BlueprintParser.h
    include/GuitarSpecsDatabase.h
    include/GuitarSpecsBrowserDialog.h
    include/ComponentDatabase.h
    include/SimpleComponentPropertiesDialog.h
)

# Create executable
add_executable(GuitarBuilder ${SOURCES} ${HEADERS})

# Link Qt libraries
target_link_libraries(GuitarBuilder Qt6::Core Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)

# Link PDF support if available
if(TARGET Qt6::Pdf)
    target_link_libraries(GuitarBuilder Qt6::Pdf)
    target_compile_definitions(GuitarBuilder PRIVATE HAVE_QT_PDF)
endif()

# Resources will be added later
# qt6_add_resources(GuitarBuilder "resources" PREFIX "/icons" FILES
#     resources/icons/new.png
#     resources/icons/open.png
#     resources/icons/save.png
# )

# Create include directory
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/include)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/resources)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/resources/icons)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)