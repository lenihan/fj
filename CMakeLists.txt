###############################################################################
# Setup build environment

# Specify the minimum version of CMake required
#     v3.20 for C++23
#       https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html#prop_tgt:CXX_STANDARD
cmake_minimum_required(VERSION 3.20)

project(fj LANGUAGES CXX)

# CTest bundles with CMake -- a test *runner* (invokes an executable, checks
# its exit code, aggregates results for `ctest`/CI), not a testing framework
# (no assertion macros, no test discovery). include(CTest) wires that up:
# defines BUILD_TESTING (default ON) and calls enable_testing().
include(CTest)

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
# source (win32Window.cpp on Windows, xlibWindow.cpp on Linux) selected
# below -- only one is ever compiled into a given binary (see platform.h's
# file comment). resources/hackAtlas.* is bakeFont's generated output, not
# hand-maintained source -- included here as data the core embeds, not
# code to edit.

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
    src/keyboardPanel.cpp
    src/keyboardPanel.h
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
elseif(EMSCRIPTEN)
    list(APPEND CORE_SOURCES src/webWindow.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND CORE_SOURCES src/xlibWindow.cpp)
    find_package(X11 REQUIRED)
    if(NOT X11_Xext_FOUND)
        message(FATAL_ERROR "libXext (XShm) not found -- install libxext-dev / libXext-devel")
    endif()
    if(NOT X11_Xrandr_FOUND)
        message(FATAL_ERROR "libXrandr not found -- install libxrandr-dev / libXrandr-devel")
    endif()
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
elseif(UNIX AND NOT APPLE)
    target_include_directories(core PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(core PUBLIC ${X11_LIBRARIES} ${X11_Xext_LIB} ${X11_Xrandr_LIB})
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
elseif(EMSCRIPTEN)
    # webWindow.cpp never returns from PlatformWindow::run() in any real
    # sense (see platform.h) -- ALLOW_MEMORY_GROWTH because a Zettelkasten
    # note store has no natural fixed size to preallocate for. ASSERTIONS
    # in Debug only: real runtime cost, so Release stays default (off).
    # ccall isn't exported to the Module object by default (EXPORTED_RUNTIME_
    # METHODS defaults to empty) -- webWindow.cpp's hidden-input 'input'
    # listener needs it to call back into fj_web_on_char for every typed
    # character. --shell-file swaps out emcc's default demo HTML (a spare
    # canvas/resize/pointer-lock/Fullscreen scaffold meant for apps that
    # hand Emscripten's runtime a canvas to drive, which webWindow.cpp never
    # does -- see webShell.html) for fj's own minimal page.
    set_target_properties(fj PROPERTIES
        SUFFIX ".html"
        LINK_DEPENDS ${CMAKE_SOURCE_DIR}/src/webShell.html
    )
    target_link_options(fj PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_RUNTIME_METHODS=ccall
        --shell-file=${CMAKE_SOURCE_DIR}/src/webShell.html
        $<$<CONFIG:Debug>:-sASSERTIONS=1>
    )
endif()

###############################################################################
# fjTests: automated regression tests (see PLAN.md's TODO) -- drives Cursor
# directly with scripted KeyEvent sequences, no PlatformWindow/platform
# shell needed at all (that's the whole point: headless, fast, no window to
# open). Catch2 is this project's one deliberate exception to the
# zero-third-party-dependency policy (see PLAN.md's "Dependency policy") --
# it's dev-only tooling that never ships in fj.exe/fj.wasm, same category as
# bakeFont being Windows-only/never-shipped above, not a runtime dependency
# of the actual app.
#
# Skipped entirely under Emscripten: these tests run as a normal desktop
# executable via ctest, not in a browser -- nothing here needs a browser
# environment, so there's no reason to make the wasm build carry it too.
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(fjTests tests/cursorTests.cpp tests/keyboardPanelTests.cpp)
    target_link_libraries(fjTests PRIVATE core Catch2::Catch2WithMain)
    set_target_properties(fjTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

    # Catch2's own CMake module providing catch_discover_tests(): runs
    # fjTests once at build time to ask it (via --list-test-names-only) what
    # TEST_CASEs it contains, then registers each one as its own separate
    # ctest entry -- `ctest` reports pass/fail per TEST_CASE, not just
    # "fjTests exited nonzero" for the whole suite.
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(fjTests)
endif()
