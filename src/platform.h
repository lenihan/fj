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
// (see PLAN.md) -- no focus/mouse events, no window-close
// callback, no Escape key, because nothing in the current code uses them.
// Resize *is* part of the contract (run()'s onResize) -- live window
// resizing is a real feature (see main.cpp), not just window-chrome noise.

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
        Char,      // printable character typed; see codepoint. This is also
                   // how navigation/commands arrive -- fj is home-row
                   // navigated (i/k/j/l, not arrow keys), so there's no
                   // separate Up/Down/Left/Right key kind here. Cursor
                   // interprets a Char as text-input vs. a command
                   // depending on its own keyboard mode; that dispatch is
                   // Cursor's job, not the platform contract's.
        Enter,
        Backspace,
        CapsLock,  // both edges matter, not just a toggle: holding caps
                   // down forces command mode for as long as it's held
                   // (squareGraphicsView.cpp's m_wasTypingMode dance),
                   // which needs pressed state at press AND release.
        Calibrate, // "treat the window's current physical size as exactly
                   // Card::kWidth_in/kHeight_in" -- an escape hatch for
                   // when the OS-reported monitor size (see
                   // win32Window.cpp's displayDpi) is wrong, which no
                   // amount of DPI-awareness correctness can fix, since
                   // it's a hardware/EDID data problem, not a DPI-API one.
                   // The user drag-resizes against a ruler, then sends
                   // this; main.cpp is the only handler (Cursor never
                   // sees it -- this is a window-physical-size concern,
                   // not a card-editing one) and persists the result so
                   // future launches start out correct too. Modeled as a
                   // KeyEvent rather than a new callback so it doesn't
                   // need its own app-surface UI -- see main.cpp.
    };

    Kind kind;
    char32_t codepoint{0}; // valid only when kind == Char
    bool pressed{true};    // key down vs up; only CapsLock ever reports
                            // false -- every other kind is press-only
};

// Declared, never defined, in this header -- each platform's .cpp provides
// the one implementation compiled into a given binary. No virtual base and
// no template backend parameter (see PLAN.md): since only one
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
    // and onResize for every change in client-area size (including the
    // implicit one right after window creation isn't reported here -- the
    // caller already knows its own initial size, since it's what it asked
    // createPlatformWindow for) until the window is closed. The core has
    // no other work competing for the thread, so this is deliberately
    // synchronous, not pollable/async.
    void run(std::function<void(const KeyEvent&)> onKey, std::function<void(int width_px, int height_px)> onResize);

    // Presents a finished frame. pixels.size() must equal w * h. The
    // platform shell stretches this to whatever the window's current
    // client size actually is (not necessarily w x h -- see onResize
    // above), so present() itself never needs to know the window size.
    void present(std::span<const Pixel> pixels, int w, int h);

    // Updates the title bar text (e.g. a live "fj - 5.00"x3.00" 100%"
    // readout as the window is resized).
    void setTitle(const std::string& title);

    // Public only so a platform shell's free functions (a WndProc, a
    // low-level keyboard hook proc -- neither of which can be a
    // PlatformWindow member, since the OS calls them by plain function
    // pointer) can name the type to reach their window's state. It's still
    // opaque everywhere outside that one .cpp: forward-declared here and
    // never defined except there.
    struct Impl;

  private:
    // createPlatformWindow is the only way to get a live Impl (a real HWND/
    // Xlib Window/...) into a PlatformWindow -- m_impl is private, so
    // without this friendship the free function below couldn't construct
    // one despite being declared right next to the class it builds.
    friend std::expected<PlatformWindow, std::string> createPlatformWindow(int width_px, int height_px,
                                                                             double aspectRatio, const char* title);
    explicit PlatformWindow(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> m_impl;
};

// std::expected (C++23) instead of a nullable return: window/context
// creation is the one place in this contract that can fail, and the
// failure reason (which CreateWindowEx/XCreateWindow/etc. call, what OS
// error) is exactly the kind of information a bool or null pointer throws
// away and a caller building an error dialog or log line will want back.
//
// aspectRatio (width/height) is what the platform shell locks the window
// to while the user drags a resize border (see win32Window.cpp's
// WM_SIZING) so the window (the emulated monitor -- see main.cpp's file
// comment) is only ever scaled uniformly, never distorted. In practice
// this is Monitor::kWidth_in/kHeight_in (main.cpp passes it directly),
// the *monitor's* declared shape (layout.h) -- not Card::kWidth_in/
// kHeight_in, the card shown on it, though CardItem::cellHeight_px
// anchoring row height to Card::kHeight_in (see cardItem.cpp) is what
// makes the card's own rendered shape match Card::kWidth_in/kHeight_in
// in the first place, rather than whatever the baked font's glyph
// proportions would otherwise imply (see tools/offline/bakeFont).
std::expected<PlatformWindow, std::string> createPlatformWindow(int width_px, int height_px, double aspectRatio,
                                                                  const char* title);

// Primary display DPI (pixels per inch), queried once before any window
// exists. Not a resize/DPI-change API (see the file comment -- that's out
// of scope): its one caller is main(), which needs it exactly once, before
// createPlatformWindow(), to pick the integer glyph-atlas render scale
// that best maps onto Card::kWidth_in/kHeight_in (see layout.h and
// PLAN.md's "Coordinate system (core)"). X-axis DPI only -- Y
// differs in practice by a pixel or two on real hardware, which this
// ignores the same way the best-fit scale itself is already an
// approximation.
int displayDpi();
