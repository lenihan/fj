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

# Define the project
project(fj)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set Qt6_DIR to the directory containing Qt6Config.cmake
# set(Qt6_DIR "C:/Users/david/git/qt_v6.8.3/install/lib/cmake/Qt6")
# C:\Users\david\git\qt_v6.8.3\install\lib\cmake\Qt6BundledZLIB\Qt6BundledZLIBConfig.cmake

# Set CMAKE_PREFIX_PATH to the Qt6 installation prefix
set(CMAKE_PREFIX_PATH "../qt_v6.8.3/install")

# Find Qt6
find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Automatically run moc, uic, and rcc when needed
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# MSVC: Static runtime libraries
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/animatedgraphicsitem.cpp
    src/animatedgraphicsitem.h
)

# Add the resource file
set(RESOURCES resources/resources.qrc)

# Qt resource compilation
qt6_add_resources(RESOURCE_FILES ${RESOURCES})

add_executable(${PROJECT_NAME} ${SOURCES} ${RESOURCE_FILES})

# Link Qt libraries
target_link_libraries(fj PRIVATE Qt6::Widgets)

# dump_vars()