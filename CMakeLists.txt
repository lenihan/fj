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

###############################################################################
# bakeFont: offline Hack -> bitmap glyph atlas generator
# (see tools/offline/bakeFont). Standalone, Windows-only, no Qt -- never
# shipped, only run by hand to regenerate resources/hackAtlas.h/.cpp, which
# is what's actually checked in for the core renderer to embed.

if(WIN32)
    add_executable(bakeFont tools/offline/bakeFont/bakeFont.cpp)
    target_link_libraries(bakeFont gdi32 user32)
    set_target_properties(bakeFont PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endif()

###############################################################################
# core: the portable, no-Qt, no-platform-code half of the Qt-removal pivot
# (see PLAN_addendum.md). Not wired into the fj executable yet -- src/main.cpp
# and friends are still the old Qt app, kept alive on this branch until a
# platform shell exists to replace it. platform.h has no .cpp of its own
# (each shell provides one); it's listed here only so it shows up in IDEs.
# resources/hackAtlas.* is bakeFont's generated output, not hand-maintained
# source -- included here as data the core embeds, not code to edit.

add_library(core STATIC
    src/canvas.cpp
    src/canvas.h
    src/platform.h
    resources/hackAtlas.cpp
    resources/hackAtlas.h
)
target_include_directories(core PUBLIC src resources)
set_target_properties(core PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
