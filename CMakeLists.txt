###############################################################################
# Setup build environment

# Specify the minimum version of CMake required
#     v3.20 for C++23 
#       https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html#prop_tgt:CXX_STANDARD
cmake_minimum_required(VERSION 3.20) 

# Use `dump_vars()` to see cmake variables. Helpful for debugging.
function(dump_vars)
    get_cmake_property(_variableNames VARIABLES)
    list (SORT _variableNames)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
endfunction()

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set CMAKE_PREFIX_PATH to the Qt6 installation prefix
set(CMAKE_PREFIX_PATH "../qt_v6.8.3/install")

# Need language set before find_package(Theads) is called
project(TopLevelProject LANGUAGES CXX)

# Find Qt6
find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Automatically run moc, uic, and rcc when needed
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# MSVC: Static runtime libraries
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

###############################################################################
# Setup project

project(fj)
set(SOURCES
    resources/resources.qrc
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/squareGraphicsView.cpp
    src/squareGraphicsView.h
)
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(fj Qt6::Widgets)