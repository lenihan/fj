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
#include <optional>
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
    // until the window is closed. The core has no other work competing
    // for the thread, so this is deliberately synchronous, not
    // pollable/async.
    //
    // Two different resize callbacks, not one, because they're genuinely
    // different jobs: onResize fires on every tick of a live resize
    // (including the implicit one right after window creation isn't
    // reported here -- the caller already knows its own initial size,
    // since it's what it asked createPlatformWindow for) and is expected
    // to be cheap -- main.cpp just re-stretches whatever it already
    // rendered into the new size (see Canvas::blitScaled), not re-picking
    // a baked atlas or rebuilding anything. onResizeEnd fires once, after
    // a resize settles (the user releases the mouse, or a non-interactive
    // resize like a maximize/snap completes), and is where main.cpp
    // actually re-picks the best atlas and redraws crisply. Calling
    // onResizeEnd's work on every onResize tick instead (an earlier
    // version did) meant a big drag visibly stepped through several
    // discrete crisp re-renders along the way, not a single one at the
    // end -- distinct from smooth continuous stretching, which is what
    // dragging should look like.
    //
    // "Settled" has no portable definition at the X11 protocol level the
    // way it does on Win32 (WM_ENTERSIZEMOVE/WM_EXITSIZEMOVE) --
    // xlibWindow.cpp debounces instead (no new ConfigureNotify for a
    // short window). Both platforms guarantee onResizeEnd fires exactly
    // once per completed resize, interactive or not.
    void run(std::function<void(const KeyEvent&)> onKey, std::function<void(int width_px, int height_px)> onResize,
             std::function<void(int width_px, int height_px)> onResizeEnd);

    // Presents a finished frame. pixels.size() must equal w * h, which
    // should already match the window's current client size (see
    // onResize above) -- main.cpp builds its output canvas at exactly
    // that size every time (see Canvas::blitScaled), rather than handing
    // over some other size and relying on this call to stretch/fit it. A
    // platform shell should present w x h as given, not independently
    // re-query its own live client rect to compare against it -- that
    // query is itself real latency (an X round-trip under Xlib), and
    // during a fast live resize it can easily observe a *newer* size
    // than the one that produced these pixels, turning "let me
    // defensively handle a mismatch" into "manufacture one on nearly
    // every tick, falling back to an expensive resample that made a live
    // resize unable to keep up with its own drag" (see PLAN.md; an
    // earlier version of xlibWindow.cpp did exactly this). If the window
    // has already moved on by the time this lands on screen, trust that
    // the next present() -- already imminent, another resize tick either
    // just fired or is about to -- corrects it, rather than guarding
    // against a race here.
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
                                                                             const char* title);
    explicit PlatformWindow(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> m_impl;
};

// std::expected (C++23) instead of a nullable return: window/context
// creation is the one place in this contract that can fail, and the
// failure reason (which CreateWindowEx/XCreateWindow/etc. call, what OS
// error) is exactly the kind of information a bool or null pointer throws
// away and a caller building an error dialog or log line will want back.
//
// No aspect-ratio parameter: the window can be resized to any shape the
// user wants (see main.cpp's file comment) -- main.cpp fits the square
// monitor into whatever shape the window actually is, letterboxed/
// pillarboxed, rather than the platform shell constraining the window's
// own shape during the drag. An earlier version locked the window to a
// fixed aspect ratio here (win32Window.cpp's WM_SIZING, xlibWindow.cpp's
// ConfigureNotify correction); both turned out to fight the OS's own
// live-resize tracking and make interactive dragging feel glitchy on
// both platforms, for a constraint the rendering side can now satisfy on
// its own without the window itself needing to cooperate.
std::expected<PlatformWindow, std::string> createPlatformWindow(int width_px, int height_px, const char* title);

// Primary display DPI (pixels per inch), queried once before any window
// exists. Not a resize/DPI-change API (see the file comment -- that's out
// of scope): its one caller is main(), which needs it exactly once, before
// createPlatformWindow(), to pick the integer glyph-atlas render scale
// that best maps onto Card::kWidth_in/kHeight_in (see layout.h and
// PLAN.md's "Coordinate system (core)"). X-axis DPI only -- Y
// differs in practice by a pixel or two on real hardware, which this
// ignores the same way the best-fit scale itself is already an
// approximation.
//
// nullopt, not some baked-in fallback number, when the platform genuinely
// can't determine a real reading (bad/missing EDID data, a virtual
// display with no physical output at all -- see win32Window.cpp's and
// xlibWindow.cpp's own comments for their specific unknown cases). Each
// platform shell used to silently return 96 in that case, which let
// main.cpp's title bar claim a confident "100%" for a size that was
// actually just a guess -- the same "plausible-but-wrong beats an honest
// unknown" mistake PLAN.md's Physical accuracy section already covers
// elsewhere. main.cpp is the one place that gets to decide what to do
// with "unknown" (pick a fallback for initial sizing, but say so in the
// title), not this layer.
std::optional<int> displayDpi();
