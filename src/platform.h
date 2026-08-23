// platform.h -- the only contract between fj's core and a platform shell.
//
// A platform shell (one .cpp per platform: win32Window.cpp, xlibWindow.cpp,
// webWindow.cpp -- only one is ever compiled into a given binary, chosen by
// CMake) does exactly three things: open a window, turn OS input events
// into KeyEvents for the core, and present the core's finished pixel
// buffer. It knows nothing about cards/cursor/fonts; the core knows
// nothing about HWNDs/Xlib/JS.
//
// Drawing primitives (fillRect/line/triangle/blitGlyph) are NOT part of
// this contract -- they're a separate core-side header that operates
// directly on the pixel buffer the core already owns. They never cross
// this boundary.
//
// Scoped to exactly what src/cursor.cpp and src/cardItem.cpp need today
// (see PLAN_addendum.md) -- no resize/focus/mouse events, no window-close
// callback, no Escape key, because nothing in the current code uses them.

#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>

// One pixel, 0x00RRGGBB in the low 24 bits. Byte order in memory (BB GG RR
// XX) matches a top-down 32bpp Win32 DIB section, so presenting on Win32 is
// a direct StretchDIBits with no per-pixel conversion.
using Pixel = uint32_t;

struct KeyEvent
{
    enum class Kind
    {
        Char, // printable character typed; see codepoint
        Up,
        Down,
        Left,
        Right,
        Enter,
        Backspace,
        CapsLock, // both edges matter, not just a toggle: PLAN.md's "hold
                  // caps down, enter should shakeCardNo" needs pressed
                  // state at the moment Enter arrives
    };

    Kind kind;
    char32_t codepoint{0}; // valid only when kind == Char
    bool pressed{true};    // key down vs up; only CapsLock ever reports
                            // false -- every other kind is press-only
};

// Declared, never defined, in this header -- each platform's .cpp provides
// the one implementation compiled into a given binary. No virtual base and
// no template backend parameter (see PLAN_addendum.md): since only one
// implementation is ever linked in, neither buys anything, and both cost
// build time or runtime indirection for nothing.
//
// The platform-specific window handle (HWND, Xlib Window, ...) lives behind
// an opaque Impl pointer (the pimpl idiom) instead of as a direct member.
// That's not about dispatch -- it's the only way to keep <windows.h>/
// Xlib.h out of this header, which matters because core translation units
// include platform.h just to call createPlatformWindow() and must not drag
// in platform headers to do it.
//
// Because Impl is only forward-declared here, the destructor and move
// operations can't be implicitly generated in this header (unique_ptr<Impl>
// would need Impl's definition to know how to delete it) -- they're
// declared here and defined with '= default' in each platform's .cpp,
// after Impl is complete.
class PlatformWindow
{
  public:
    ~PlatformWindow();
    PlatformWindow(PlatformWindow&&) noexcept;
    PlatformWindow& operator=(PlatformWindow&&) noexcept;

    // Blocks, owns the event loop, and invokes onKey for every input event
    // until the window is closed. The core has no other work competing for
    // the thread, so this is deliberately synchronous, not pollable/async.
    void run(std::function<void(const KeyEvent&)> onKey);

    // Presents a finished frame. pixels.size() must equal w * h.
    void present(std::span<const Pixel> pixels, int w, int h);

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// std::expected (C++23) instead of a nullable return: window/context
// creation is the one place in this contract that can fail, and the
// failure reason (which CreateWindowEx/XCreateWindow/etc. call, what OS
// error) is exactly the kind of information a bool or null pointer throws
// away and a caller building an error dialog or log line will want back.
std::expected<PlatformWindow, std::string> createPlatformWindow(int width_px, int height_px, const char* title);
