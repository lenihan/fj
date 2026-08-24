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
# PLAN.md). The old Qt6/QGraphicsItem app this replaces lives only
# on main now, not this branch.
#
# platform.h's declarations are implemented by exactly one platform shell
# source (win32Window.cpp on Windows) selected below -- only one is ever
# compiled into a given binary (see platform.h's file comment). resources/
# hackAtlas.* is bakeFont's generated output, not hand-maintained source --
# included here as data the core embeds, not code to edit.

set(CORE_SOURCES
    src/canvas.cpp
    src/canvas.h
    src/cardItem.cpp
    src/cardItem.h
    src/cardStack.cpp
    src/cardStack.h
    src/contentItem.cpp
    src/contentItem.h
    src/cursor.cpp
    src/cursor.h
    src/layout.h
    src/platform.h
    src/textUtil.h
    src/tocItem.cpp
    src/tocItem.h
    src/types.h
    resources/hackAtlas.cpp
    resources/hackAtlas.h
)
if(WIN32)
    list(APPEND CORE_SOURCES src/win32Window.cpp)
endif()

add_library(core STATIC ${CORE_SOURCES})
target_include_directories(core PUBLIC src resources)
set_target_properties(core PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
if(WIN32)
    target_link_libraries(core PUBLIC gdi32 user32)
    # win32Window.cpp calls the W-suffixed Win32 API explicitly throughout,
    # but a few macros (IDC_ARROW, MAKEINTRESOURCE) still switch on
    # UNICODE/_UNICODE rather than the call site -- define both so they
    # resolve to the wide forms those calls actually expect.
    target_compile_definitions(core PUBLIC UNICODE _UNICODE)
endif()

###############################################################################
# fj: the actual app -- core plus main.cpp's platform-agnostic composition
# of Cursor/Canvas/PlatformWindow (see main.cpp's file comment).

add_executable(fj src/main.cpp)
target_link_libraries(fj PRIVATE core)
set_target_properties(fj PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
if(WIN32)
    # Debug: console subsystem, so stderr (window-creation failures, any
    # future debug prints) is visible for free. Release: Windows subsystem,
    # so fj launches without a console window alongside it. main.cpp stays
    # plain `int main()` either way -- WIN32_EXECUTABLE alone would default
    # the entry point to WinMainCRTStartup (needing a WinMain we don't
    # have), so /ENTRY:mainCRTStartup pins it back to the ordinary CRT
    # startup that calls main() in both configs.
    set_target_properties(fj PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    target_link_options(fj PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)

    # Declares per-monitor-v2 DPI awareness in fj.exe's embedded manifest.
    # win32Window.cpp's displayDpi() also calls SetProcessDpiAwarenessContext
    # at runtime, but a runtime call can lose a race against DPI awareness
    # already having been implicitly decided before main() runs (CRT
    # startup, the first touch of USER32, etc.) -- a manifest is what
    # Microsoft actually recommends, since the OS loader applies it before
    # any of the process's own code executes, with no timing ambiguity.
    set_target_properties(fj PROPERTIES VS_DPI_AWARE "PerMonitor")
endif()
