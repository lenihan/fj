###############################################################################
# Setup build environment

# Specify the minimum version of CMake required
#     v3.20 for C++23 
#       https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html#prop_tgt:CXX_STANDARD
cmake_minimum_required(VERSION 3.20) 

project(fj LANGUAGES CXX)

# Find Qt6
find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Automatically run moc, uic, and rcc when needed
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(SOURCES
    resources/resources.qrc
    src/capsLockModifier.cpp
    src/capsLockModifier.h
    src/cardItem.cpp
    src/cardItem.h
    src/cardStack.cpp
    src/cardStack.h
    src/common.h
    src/contentItem.cpp
    src/contentItem.h
    src/cursor.cpp
    src/cursor.h
    src/main.cpp
    src/rowItem.cpp
    src/rowItem.h
    src/squareGraphicsView.cpp
    src/squareGraphicsView.h
    src/tocItem.cpp
    src/tocItem.h
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} Qt6::Widgets)

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
endif()