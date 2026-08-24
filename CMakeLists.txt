###############################################################################
# Setup build environment

# Specify the minimum version of CMake required
#     v3.20 for C++23
#       https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html#prop_tgt:CXX_STANDARD
cmake_minimum_required(VERSION 3.20)

project(fj LANGUAGES CXX)

###############################################################################
# bakeFont: offline Hack -> bitmap glyph atlas generator
# (see tools/offline/bakeFont). Standalone, Windows-only -- never shipped,
# only run by hand to regenerate resources/hackAtlas.h/.cpp, which is
# what's actually checked in for the core renderer to embed.

if(WIN32)
    add_executable(bakeFont tools/offline/bakeFont/bakeFont.cpp)
    target_link_libraries(bakeFont gdi32 user32)
    set_target_properties(bakeFont PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endif()

###############################################################################
# core: the portable, no-platform-code half of the Qt-removal pivot (see
# PLAN_addendum.md). Not wired into a real executable yet -- there's no
# platform shell (Win32/Xlib/Emscripten) to drive it. The old Qt6/
# QGraphicsItem app this replaces lives only on main now, not this branch.
#
# cardItem/cardStack/contentItem/cursor/tocItem have no .cpp yet (headers
# only, still being drafted); platform.h has no .cpp of its own (each
# platform shell provides one). Both are listed here only so they show up
# in IDEs. resources/hackAtlas.* is bakeFont's generated output, not
# hand-maintained source -- included here as data the core embeds, not
# code to edit.

add_library(core STATIC
    src/canvas.cpp
    src/canvas.h
    src/cardItem.h
    src/cardStack.h
    src/contentItem.h
    src/cursor.h
    src/layout.h
    src/platform.h
    src/tocItem.h
    src/types.h
    resources/hackAtlas.cpp
    resources/hackAtlas.h
)
target_include_directories(core PUBLIC src resources)
set_target_properties(core PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
