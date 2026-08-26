// webWindow.cpp -- the Web (Emscripten/Canvas) implementation of platform.h's
// contract. DOM setup, event wiring, and presenting pixels via a Canvas 2D
// context. Nothing here knows about cards/cursor/fonts; see platform.h's
// file comment for the boundary this respects. Mirrors win32Window.cpp's/
// xlibWindow.cpp's structure/responsibilities where the platform allows --
// read those files' comments for the parts of the design that aren't
// platform-specific.
//
// Two real, permanent differences from the other two platforms (not bugs to
// fix, just what a browser sandbox actually offers):
//
// - No blocking event loop. GetMessageW/XNextEvent can block their thread
//   indefinitely because each owns a whole OS process; a browser tab can't
//   block its one JS thread that way without freezing the page entirely.
//   run() instead calls emscripten_set_main_loop(..., simulate_infinite_loop
//   = true), which unwinds the call stack back to the browser's event loop
//   via a special internal mechanism rather than a normal C++ return --
//   critically, that unwind does NOT run this frame's (or main()'s, above
//   it) destructors, so main.cpp's onKey/onResize/onResizeEnd lambdas
//   (which capture Cursor/Canvas/etc. by reference from main()'s stack)
//   stay valid for as long as the page lives, exactly as if run() really
//   had blocked forever. mainLoopTick itself does nothing -- every actual
//   event (keystrokes, resize) already arrives the instant it happens via
//   its own DOM callback below, not by being polled here.
//
// - No way to force the OS's Caps Lock state off. win32Window.cpp's
//   SetWindowsHookEx dance and xlibWindow.cpp's XkbLockModifiers both
//   suppress the *system* lock state while focused, so typing never
//   depends on whatever it happened to be before the window got focus (see
//   win32Window.cpp's file comment). No browser API exposes that kind of
//   OS-level keyboard-state mutation at all -- this shell only tracks the
//   physical key's own press/release edges (KeyEvent::Kind::CapsLock,
//   Cursor's own "held" state), same as the others, but if the real OS
//   Caps Lock happens to be latched on, the browser's own text composition
//   will still capitalize accordingly, independent of fj's app-level
//   command-mode tracking. Accepted gap, not fixed here.

#include "platform.h"

#include <emscripten.h>
#include <emscripten/eventloop.h>
#include <emscripten/html5.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

struct PlatformWindow::Impl
{
    std::function<void(const KeyEvent&)> onKey;
    std::function<void(int width_px, int height_px)> onResize;
    std::function<void(int width_px, int height_px)> onResizeEnd;

    // Debounces onResizeEnd the same way xlibWindow.cpp's select()-based
    // loop does (X11 and the browser's 'resize' event share the same
    // problem: neither has a protocol-level "the drag just ended" signal
    // the way Win32's WM_ENTERSIZEMOVE/WM_EXITSIZEMOVE does) -- 0 means no
    // resize is currently pending settlement.
    int resizeSettleTimerId{0};
    int pendingWidth{0};
    int pendingHeight{0};

    // Tracks the physical Caps Lock key's own held/released state (set by
    // onKeydownEvent/onKeyupEvent below) -- fj_web_on_char reads this to
    // work around a web-only gap: unlike win32Window.cpp/xlibWindow.cpp,
    // which force the OS's Caps Lock state off while focused (see this
    // file's header comment), there's no browser API to stop physically
    // pressing Caps Lock from actually engaging the real OS lock. So while
    // it's held, the 'input' event for the home-row nav keys (i/j/k/l)
    // arrives already uppercased by the OS, same as typing any letter with
    // real Caps Lock on anywhere else -- Cursor's command-mode dispatch
    // expects the unshifted key identity, exactly what it already gets on
    // the other two platforms. See fj_web_on_char.
    bool capsLockHeld{false};

    // present()'s BGRX(Pixel) -> RGBA(ImageData) scratch buffer -- see
    // present()'s comment for why this conversion is unique to this
    // platform. Reused/resized in place across calls rather than
    // reallocated every frame, same reasoning as win32Window.cpp's
    // lastPixels.
    std::vector<uint8_t> rgbaScratch;
};

namespace
{

// Only one PlatformWindow is ever meant to exist per process (same
// reasoning as win32Window.cpp's g_activeImpl/xlibWindow.cpp's single Impl)
// -- DOM event callbacks are plain C-linkage function pointers with a
// void* userData slot, not capturing lambdas, so in practice userData is
// always this. Kept as a global too (rather than only threading userData
// through) because the JS glue's fj_web_on_char bridge (see below) has no
// userData slot of its own to carry it -- Module.ccall just calls a named
// function with the arguments JS gives it.
PlatformWindow::Impl* g_activeImpl = nullptr;

constexpr int kResizeSettleMs = 300; // matches xlibWindow.cpp's debounce window

int physicalSize(int cssSize, double dpr)
{
    return static_cast<int>(std::lround(cssSize * dpr));
}

// --- DOM setup, presenting, and title (JS glue) -------------------------

// UTF8ToString is a JS runtime helper, not a global always linked in by
// default -- this declares the dependency explicitly so the linker keeps
// it regardless of build flags (see em_macros.h's EM_JS_DEPS comment)
// instead of relying on it happening to already be pulled in by something
// else.
EM_JS_DEPS(fjWebWindowDeps, "$UTF8ToString");

// Builds the whole DOM this shell needs: a canvas to present into, and an
// off-screen, permanently-focused <input> to receive real text. Why an
// <input> and not just a 'keydown' listener: 'keydown' only reports a
// physical key, not a composed character -- dead keys, IME composition
// (CJK and otherwise), and non-US layouts all need a real editable DOM
// element's 'input' event to come out correct, the same job WM_CHAR does
// on Win32 and Xutf8LookupString does on Xlib. Only one PlatformWindow is
// ever created (see g_activeImpl), so this stashes canvas/ctx/input on
// `window.__fj` once rather than needing every other EM_JS function below
// to look them up again.
EM_JS(void, fjCreateDom, (int width_px, int height_px, double dpr, const char* title), {
    document.title = UTF8ToString(title);
    document.body.style.margin = '0';
    document.body.style.overflow = 'hidden';
    document.body.style.background = '#000';

    var canvas = document.createElement('canvas');
    canvas.id = 'fjCanvas';
    canvas.style.position = 'absolute';
    canvas.style.left = '0';
    canvas.style.top = '0';
    canvas.width = width_px;
    canvas.height = height_px;
    canvas.style.width = (width_px / dpr) + 'px';
    canvas.style.height = (height_px / dpr) + 'px';
    document.body.appendChild(canvas);

    var input = document.createElement('input');
    input.id = 'fjInput';
    input.type = 'text';
    input.autocomplete = 'off';
    input.autocapitalize = 'off';
    input.spellcheck = false;
    input.style.position = 'fixed';
    input.style.left = '-1000px';
    input.style.top = '0';
    input.style.opacity = '0';
    document.body.appendChild(input);

    // e.data is the text this 'input' event actually produced (a full IME
    // composition can commit more than one character at once) -- iterated
    // as `for...of` a string, which walks real Unicode code points (not
    // UTF-16 code units), so a codepoint outside the BMP still comes
    // through as one value instead of two mismatched surrogate halves.
    // The field is cleared after every event so it never accumulates text
    // fj isn't using and Backspace always has nothing-but-empty to act on.
    input.addEventListener('input', function(e)
    {
        var text = e.data || "";
        for (var i = 0; i < text.length; )
        {
            var cp = text.codePointAt(i);
            Module.ccall('fj_web_on_char', null, ['number'], [cp]);
            i += (cp > 0xFFFF) ? 2 : 1;
        }
        input.value = "";
    });
    // fj has no other interactive element to tab to -- losing focus (a
    // stray click, alt-tab back) would silently stop all text/IME input,
    // so it's reclaimed immediately rather than requiring the user to
    // notice and click some specific spot to get it back.
    input.addEventListener('blur', function() { setTimeout(function() { input.focus(); }, 0); });
    document.addEventListener('mousedown', function() { input.focus(); });
    input.focus();

    window.__fj = {canvas: canvas, ctx: canvas.getContext('2d'), input: input};
});

EM_JS(void, fjResizeCanvas, (int width_px, int height_px, double dpr), {
    var canvas = window.__fj.canvas;
    canvas.width = width_px;
    canvas.height = height_px;
    canvas.style.width = (width_px / dpr) + 'px';
    canvas.style.height = (height_px / dpr) + 'px';
});

// rgba points at present()'s already-converted scratch buffer (w*h*4
// bytes, RGBA) inside wasm linear memory -- HEAPU8 is the runtime's own
// live view over that memory, accessible by bare name from EM_JS code
// (this is JS spliced directly into the runtime's own scope, unlike
// arbitrary external JS, which would need to go through Module.HEAPU8 and
// isn't guaranteed to be exported at all -- see EXPORTED_RUNTIME_METHODS).
// No copy beyond the one present() already made converting BGRX -> RGBA.
EM_JS(void, fjPresentFrame, (const uint8_t* rgba, int w, int h), {
    var bytes = new Uint8ClampedArray(HEAPU8.buffer, rgba, w * h * 4);
    window.__fj.ctx.putImageData(new ImageData(bytes, w, h), 0, 0);
});

EM_JS(void, fjSetTitle, (const char* title), { document.title = UTF8ToString(title); });

// --- C++ <-> JS bridge ---------------------------------------------------

// extern "C" so Module.ccall (fjCreateDom's 'input' listener above) can
// find it by its literal, unmangled name; EMSCRIPTEN_KEEPALIVE so the
// linker exports it despite nothing in this translation unit calling it
// directly in C++ (its only caller is that JS listener).
extern "C" EMSCRIPTEN_KEEPALIVE void fj_web_on_char(uint32_t codepoint)
{
    if (!g_activeImpl || !g_activeImpl->onKey)
        return;

    // See Impl::capsLockHeld's comment: while Caps Lock is physically held,
    // the browser has already real-uppercased any A-Z the OS's own (un-
    // suppressible) lock state applies -- undo that so the nav-key dispatch
    // sees the same unshifted codepoint it would on Win32/Xlib. Command
    // mode never treats held-Caps character input as literal text, so
    // there's no legitimate case here where the original case mattered.
    if (g_activeImpl->capsLockHeld && codepoint >= 'A' && codepoint <= 'Z')
        codepoint += ('a' - 'A');

    g_activeImpl->onKey({KeyEvent::Kind::Char, static_cast<char32_t>(codepoint), true});
}

// --- Native event callbacks ------------------------------------------

void resizeSettled(void* userData)
{
    auto* impl = static_cast<PlatformWindow::Impl*>(userData);
    impl->resizeSettleTimerId = 0;
    if (impl->onResizeEnd)
        impl->onResizeEnd(impl->pendingWidth, impl->pendingHeight);
}

bool onResizeEvent(int /*eventType*/, const EmscriptenUiEvent* uiEvent, void* userData)
{
    auto* impl = static_cast<PlatformWindow::Impl*>(userData);
    double dpr = emscripten_get_device_pixel_ratio();
    int width_px = physicalSize(uiEvent->windowInnerWidth, dpr);
    int height_px = physicalSize(uiEvent->windowInnerHeight, dpr);

    // Cheap tick first (see platform.h's run() comment): resize the
    // backing store/CSS box and hand main.cpp the new size immediately so
    // it can re-stretch what it already rendered. The atlas re-pick and
    // full redraw only happen once the debounce below decides the resize
    // has settled.
    fjResizeCanvas(width_px, height_px, dpr);
    if (impl->onResize)
        impl->onResize(width_px, height_px);

    impl->pendingWidth = width_px;
    impl->pendingHeight = height_px;
    if (impl->resizeSettleTimerId)
        emscripten_clear_timeout(impl->resizeSettleTimerId);
    impl->resizeSettleTimerId = emscripten_set_timeout(resizeSettled, kResizeSettleMs, impl);

    return false; // no default browser action on window resize to suppress
}

// Returning true here (for the four keys this shell actually intercepts)
// tells the Emscripten runtime to call the DOM event's preventDefault()
// for us -- see library_html5.js's handling of this callback's return
// value. That matters most for Backspace: unhandled, some browsers
// navigate back a page on Backspace outside a genuinely-editable context,
// and even inside one, preventDefault stops the input's own default
// delete-and-fire-'input' behavior so Backspace is only ever handled once,
// via Kind::Backspace, never also as a stray 'input' event. Everything
// else returns false and falls through to the input element's own
// 'input' event (see fjCreateDom) -- that's how printable text actually
// arrives (fj_web_on_char), not from this callback.
bool onKeydownEvent(int /*eventType*/, const EmscriptenKeyboardEvent* keyEvent, void* userData)
{
    auto* impl = static_cast<PlatformWindow::Impl*>(userData);
    if (!impl->onKey)
        return false;

    std::string_view key(keyEvent->key);
    if (key == "CapsLock") // both edges matter -- see platform.h's Kind::CapsLock comment
    {
        impl->capsLockHeld = true; // see Impl::capsLockHeld's comment
        impl->onKey({KeyEvent::Kind::CapsLock, 0, true});
        return true;
    }
    if (key == "Enter")
    {
        impl->onKey({KeyEvent::Kind::Enter, 0, true});
        return true;
    }
    if (key == "Backspace")
    {
        impl->onKey({KeyEvent::Kind::Backspace, 0, true});
        return true;
    }
    if (key == "F5") // see platform.h's Kind::Calibrate comment -- also stops the browser's own page reload
    {
        impl->onKey({KeyEvent::Kind::Calibrate, 0, true});
        return true;
    }
    return false;
}

bool onKeyupEvent(int /*eventType*/, const EmscriptenKeyboardEvent* keyEvent, void* userData)
{
    auto* impl = static_cast<PlatformWindow::Impl*>(userData);
    if (std::string_view(keyEvent->key) == "CapsLock")
    {
        impl->capsLockHeld = false; // see Impl::capsLockHeld's comment
        if (impl->onKey)
            impl->onKey({KeyEvent::Kind::CapsLock, 0, false});
    }
    return false;
}

// Does nothing on purpose -- see this file's header comment. Every real
// event is delivered by its own DOM callback the instant it happens;
// emscripten_set_main_loop exists here only to keep run() from actually
// returning.
void mainLoopTick() {}

} // namespace

PlatformWindow::PlatformWindow(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
PlatformWindow::~PlatformWindow()
{
    if (!m_impl) // moved-from
        return;
    if (m_impl->resizeSettleTimerId)
        emscripten_clear_timeout(m_impl->resizeSettleTimerId);
    if (g_activeImpl == m_impl.get())
        g_activeImpl = nullptr;
}
PlatformWindow::PlatformWindow(PlatformWindow&&) noexcept = default;
PlatformWindow& PlatformWindow::operator=(PlatformWindow&&) noexcept = default;

void PlatformWindow::run(std::function<void(const KeyEvent&)> onKey,
                          std::function<void(int width_px, int height_px)> onResize,
                          std::function<void(int width_px, int height_px)> onResizeEnd)
{
    m_impl->onKey = std::move(onKey);
    m_impl->onResize = std::move(onResize);
    m_impl->onResizeEnd = std::move(onResizeEnd);

    emscripten_set_keydown_callback("#fjInput", m_impl.get(), false, onKeydownEvent);
    emscripten_set_keyup_callback("#fjInput", m_impl.get(), false, onKeyupEvent);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, m_impl.get(), false, onResizeEvent);

    emscripten_set_main_loop(mainLoopTick, 0, true); // see this file's header comment
}

void PlatformWindow::present(std::span<const Pixel> pixels, int w, int h)
{
    assert(pixels.size() == static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    if (w <= 0 || h <= 0)
        return;

    // Canvas ImageData is spec-mandated RGBA byte order regardless of host
    // endianness -- unlike win32Window.cpp's DIB and xlibWindow.cpp's
    // XImage, which both happen to already match Pixel's documented BGRX
    // in-memory layout (platform.h) on a little-endian host and so need no
    // conversion at all, this is the one platform where that coincidence
    // doesn't hold and a real per-pixel repack is unavoidable. Working
    // from Pixel's integer value (bit-shifting out R/G/B) rather than its
    // in-memory byte layout sidesteps having to reason about endianness
    // here at all.
    auto& rgba = m_impl->rgbaScratch;
    rgba.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
    for (std::size_t i = 0; i < pixels.size(); ++i)
    {
        Pixel p = pixels[i];
        rgba[i * 4 + 0] = static_cast<uint8_t>((p >> 16) & 0xFF); // R
        rgba[i * 4 + 1] = static_cast<uint8_t>((p >> 8) & 0xFF);  // G
        rgba[i * 4 + 2] = static_cast<uint8_t>(p & 0xFF);         // B
        rgba[i * 4 + 3] = 0xFF;                                   // A
    }

    fjPresentFrame(rgba.data(), w, h);
}

void PlatformWindow::setTitle(const std::string& title)
{
    fjSetTitle(title.c_str());
}

std::expected<PlatformWindow, std::string> createPlatformWindow(int width_px, int height_px, const char* title)
{
    auto impl = std::make_unique<PlatformWindow::Impl>();

    double dpr = emscripten_get_device_pixel_ratio();
    fjCreateDom(width_px, height_px, dpr, title);

    g_activeImpl = impl.get();
    return PlatformWindow(std::move(impl));
}

std::optional<int> displayDpi()
{
    // Browsers expose no EDID/physical-monitor-size API at all -- unlike
    // win32Window.cpp's GetDeviceCaps(..., HORZSIZE) or xlibWindow.cpp's
    // XRandR mm query, both of which can honestly answer "unknown" and
    // let main.cpp fall back (see platform.h's displayDpi comment).
    // devicePixelRatio is the best a web page gets: CSS defines 1px as
    // 1/96in at ratio 1, so 96 * ratio is a real pixel-density reading,
    // but it conflates true monitor PPI with whatever OS/browser zoom
    // percentage the user has chosen -- the same ambiguity Win32's
    // LOGPIXELSX was rejected for elsewhere in this codebase (see
    // PLAN.md's "Physical accuracy"), just with no way here to avoid it.
    // Always answers something rather than nullopt (a browser always has
    // *a* devicePixelRatio) -- Kind::Calibrate exists for the user to
    // correct whatever this guesses, same as on the other two platforms.
    double dpr = emscripten_get_device_pixel_ratio();
    if (dpr <= 0.0)
        return std::nullopt;
    return static_cast<int>(std::lround(96.0 * dpr));
}
