// xlibWindow.cpp -- the Linux/X11 implementation of platform.h's contract.
// Window creation, the event loop, KeyEvent translation, and presenting
// pixels via XShm/XPutImage. Nothing here knows about cards/cursor/fonts;
// see platform.h's file comment for the boundary this respects. Mirrors
// win32Window.cpp's structure/responsibilities closely -- read that file's
// comments for the parts of the design that aren't platform-specific.
//
// CapsLock is simpler here than on Win32: X11 reports ordinary KeyPress/
// KeyRelease for the physical Caps Lock key itself (no low-level global
// hook needed to see both edges the way Win32's toggle-key coalescing
// required). To stop the OS-level lock state from silently capitalizing
// subsequent typed text while held, this forces the modifier off on
// FocusIn (remembering its prior state) and restores it on FocusOut --
// same idea as win32Window.cpp's handleActivate, different mechanism
// (XkbLockModifiers instead of a hook).

#include "platform.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XShm.h>
#include <X11/keysym.h>

#include <sys/ipc.h>
#include <sys/select.h>
#include <sys/shm.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>

struct PlatformWindow::Impl
{
    Display* display{nullptr};
    int screen{0};
    Window window{0};
    Atom wmDeleteWindow{0};
    XIM inputMethod{nullptr};
    XIC inputContext{nullptr};

    bool originalCapsLockOn{false};

    // Lazily (re)allocated in present() whenever the live window size
    // changes -- see ensureImage. shmAttached false means XShm wasn't
    // available and image->data is a plain malloc'd buffer instead.
    XShmSegmentInfo shmInfo{};
    XImage* image{nullptr};
    int imageWidth{0};
    int imageHeight{0};
    bool shmAttached{false};

    std::function<void(const KeyEvent&)> onKey;
    std::function<void(int width_px, int height_px)> onResize;
    std::function<void(int width_px, int height_px)> onResizeEnd;
};

namespace
{

// The physical size/resolution of one XRandR output (monitor) --
// deliberately not the legacy core-protocol DisplayWidthMM/DisplayWidth
// (the whole X *screen*, which on any modern multi-monitor setup can
// span several physical displays combined, and which some virtual/
// remote X servers answer with a fabricated-looking value instead of
// admitting they don't know -- WSLg reports exactly 96dpi's worth of
// fake millimeters rather than 0). XRandR reports per-output physical
// size honestly: an output with no real EDID data reports 0mm, which
// this treats as "unknown" rather than accepting a plausible-but-wrong
// number, the same mistake GetDeviceCaps(..., LOGPIXELSX) turned out to
// be on the Win32 side (see PLAN.md's "Physical accuracy").
//
// Tries the primary output first (XRRGetOutputPrimary), then falls back
// to the first connected output with an active mode and a nonzero
// physical size. Returns 0 (not a fallback DPI itself -- that's
// displayDpi's job) if nothing usable is found -- KeyEvent::
// Kind::Calibrate exists for exactly that case.
int queryOutputDpi(Display* display)
{
    Window root = DefaultRootWindow(display);
    XRRScreenResources* resources = XRRGetScreenResourcesCurrent(display, root);
    if (!resources)
        return 0;

    auto tryOutput = [&](RROutput outputId) -> int
    {
        if (outputId == None)
            return 0;
        XRROutputInfo* output = XRRGetOutputInfo(display, resources, outputId);
        if (!output)
            return 0;
        int result = 0;
        if (output->connection == RR_Connected && output->crtc != None && output->mm_width > 0)
        {
            if (XRRCrtcInfo* crtc = XRRGetCrtcInfo(display, resources, output->crtc))
            {
                if (crtc->width > 0)
                    result = static_cast<int>(std::lround(crtc->width / (output->mm_width / 25.4)));
                XRRFreeCrtcInfo(crtc);
            }
        }
        XRRFreeOutputInfo(output);
        return result;
    };

    int dpi = tryOutput(XRRGetOutputPrimary(display, root));
    for (int i = 0; i < resources->noutput && dpi <= 0; ++i)
        dpi = tryOutput(resources->outputs[i]);

    XRRFreeScreenResources(resources);
    return dpi;
}

bool capsLockOn(Display* display)
{
    XKeyboardState state{};
    XGetKeyboardControl(display, &state);
    return (state.led_mask & 1) != 0; // bit 0 is the Caps Lock LED (XGetKeyboardControl docs)
}

void setCapsLockOn(Display* display, bool on)
{
    if (capsLockOn(display) == on)
        return;
    XkbLockModifiers(display, XkbUseCoreKbd, LockMask, on ? LockMask : 0);
}

void handleFocus(PlatformWindow::Impl& impl, bool focused)
{
    if (focused)
    {
        impl.originalCapsLockOn = capsLockOn(impl.display);
        setCapsLockOn(impl.display, false);
    }
    else
    {
        setCapsLockOn(impl.display, impl.originalCapsLockOn);
    }
}

// Decodes the first UTF-8 codepoint of a Xutf8LookupString result. fj's
// atlas is fixed-pitch ASCII plus two arrows, so a single key event never
// needs more than one codepoint in practice.
char32_t decodeUtf8(const char* s)
{
    auto b = [&](int i) { return static_cast<unsigned char>(s[i]); };
    unsigned char c0 = b(0);
    if (c0 < 0x80)
        return c0;
    if ((c0 & 0xE0) == 0xC0)
        return static_cast<char32_t>(((c0 & 0x1F) << 6) | (b(1) & 0x3F));
    if ((c0 & 0xF0) == 0xE0)
        return static_cast<char32_t>(((c0 & 0x0F) << 12) | ((b(1) & 0x3F) << 6) | (b(2) & 0x3F));
    if ((c0 & 0xF8) == 0xF0)
        return static_cast<char32_t>(((c0 & 0x07) << 18) | ((b(1) & 0x3F) << 12) | ((b(2) & 0x3F) << 6) |
                                      (b(3) & 0x3F));
    return 0;
}

void handleKeyEvent(PlatformWindow::Impl& impl, XEvent& event)
{
    if (!impl.onKey)
        return;

    bool pressed = (event.type == KeyPress);
    KeySym keysym = XLookupKeysym(&event.xkey, 0);

    if (keysym == XK_Caps_Lock) // both edges matter -- see platform.h's Kind::CapsLock comment
    {
        impl.onKey({KeyEvent::Kind::CapsLock, 0, pressed});
        return;
    }

    if (!pressed) // everything else is press-only (platform.h)
        return;

    if (keysym == XK_Return || keysym == XK_KP_Enter)
    {
        impl.onKey({KeyEvent::Kind::Enter, 0, true});
        return;
    }
    if (keysym == XK_BackSpace)
    {
        impl.onKey({KeyEvent::Kind::Backspace, 0, true});
        return;
    }
    if (keysym == XK_F5) // see platform.h's Kind::Calibrate comment
    {
        impl.onKey({KeyEvent::Kind::Calibrate, 0, true});
        return;
    }

    // Printable text: Xutf8LookupString decodes through the input method
    // (handles dead keys/compose sequences correctly), giving real UTF-8
    // -- XLookupString alone is Latin-1 only and would mangle anything
    // outside that range.
    char buf[32];
    Status status = 0;
    int len = impl.inputContext
                  ? Xutf8LookupString(impl.inputContext, &event.xkey, buf, sizeof(buf) - 1, nullptr, &status)
                  : 0;
    if (len <= 0 || status == XBufferOverflow)
        return;
    buf[len] = '\0';

    char32_t codepoint = decodeUtf8(buf);
    // Control characters (\r, \b, Esc, Tab) arrive here too on some
    // layouts -- Enter/Backspace are handled above via keysym, and
    // Escape/Tab/Delete aren't part of this contract (see platform.h's
    // file comment), so only forward genuine printable text.
    if (codepoint < 0x20 || codepoint == 0x7F)
        return;

    impl.onKey({KeyEvent::Kind::Char, codepoint, true});
}

// Frees whatever ensureImage last allocated. XShm images need their
// shared-memory segment torn down separately from the XImage struct
// itself -- and image->data must be cleared to nullptr before
// XDestroyImage runs, or it will try to free() the shmat'd pointer
// (undefined behavior: shm memory is only ever valid to release via
// shmdt, never free()). No shmctl(IPC_RMID) needed here -- ensureImage
// already marks the segment for removal right after shmget succeeds, so
// this shmdt is what actually frees it.
void destroyImage(PlatformWindow::Impl& impl)
{
    if (!impl.image)
        return;
    if (impl.shmAttached)
    {
        XShmDetach(impl.display, &impl.shmInfo);
        impl.image->data = nullptr;
        XDestroyImage(impl.image);
        shmdt(impl.shmInfo.shmaddr);
        impl.shmAttached = false;
    }
    else
    {
        XDestroyImage(impl.image); // frees the malloc'd data buffer for us
    }
    impl.image = nullptr;
}

// XShmAttach failures don't come back through its return value -- the X
// server reports them asynchronously as a protocol error, which Xlib's
// default error handler treats as fatal (prints and kills the process).
// XShmQueryExtension only confirms the server *advertises* the extension,
// not that this particular connection can actually use it (WSLg's virtual
// X server is a real example: it advertises MIT-SHM but rejects the
// attach) -- so probing for real support means temporarily installing an
// error handler, issuing the attach, and forcing it to be processed
// immediately (XSync) before checking whether it actually went through.
// This is the standard pattern (SDL and others do the same thing), not
// something specific to this app.
bool g_shmAttachFailed = false;

int shmErrorHandler(Display*, XErrorEvent*)
{
    g_shmAttachFailed = true;
    return 0;
}

// Whether *this connection* can actually attach XShm segments, not just
// whether one particular attach did -- support is a fact about the
// connection, established once, not something that can start being true
// or false on a later call. Cached after the first real probe (see
// tryShmAttach) so ensureImage's near-constant reallocation during a
// live resize (the size changes on almost every tick) doesn't pay for
// the XSync round trip below more than once -- that round trip measured
// as real, recurring latency once it stopped being masked by the bigger
// cost fixed alongside it (see PLAN.md).
std::optional<bool> g_shmSupported;

bool tryShmAttach(Display* display, XShmSegmentInfo& shmInfo)
{
    if (g_shmSupported.has_value())
    {
        // Already proven either way on this connection -- if it's known
        // to work, just attach directly with no error-trapping dance; if
        // it's known not to, ensureImage never calls this again at all
        // (see its own g_shmSupported check).
        XShmAttach(display, &shmInfo);
        return true;
    }

    g_shmAttachFailed = false;
    XErrorHandler previous = XSetErrorHandler(shmErrorHandler);
    XShmAttach(display, &shmInfo);
    XSync(display, False);
    XSetErrorHandler(previous);
    g_shmSupported = !g_shmAttachFailed;
    return *g_shmSupported;
}

// (Re)allocates impl.image at exactly width x height if it isn't already
// that size -- called fresh at the top of every present() (mirrors
// win32Window.cpp's blit() querying the live client rect every call
// rather than caching it), so a live resize is picked up automatically.
void ensureImage(PlatformWindow::Impl& impl, int width, int height)
{
    if (impl.image && impl.imageWidth == width && impl.imageHeight == height)
        return;

    destroyImage(impl);

    Visual* visual = DefaultVisual(impl.display, impl.screen);
    auto depth = static_cast<unsigned int>(DefaultDepth(impl.display, impl.screen));
    auto w = static_cast<unsigned int>(width);
    auto h = static_cast<unsigned int>(height);

    // g_shmSupported.value_or(true): try XShm unless it's already known
    // not to work on this connection (see tryShmAttach) -- no point
    // repeating XShmCreateImage/shmget/shmat just to fail the same way
    // again on every subsequent reallocation.
    if (g_shmSupported.value_or(true) && XShmQueryExtension(impl.display))
    {
        impl.image = XShmCreateImage(impl.display, visual, depth, ZPixmap, nullptr, &impl.shmInfo, w, h);
        if (impl.image)
        {
            impl.shmInfo.shmid =
                shmget(IPC_PRIVATE, static_cast<std::size_t>(impl.image->bytes_per_line) * height, IPC_CREAT | 0600);
            if (impl.shmInfo.shmid != -1)
            {
                // Mark for removal now, not after detaching: a SysV shm
                // segment marked IPC_RMID is only actually freed once its
                // last attachment goes away, so this is safe even if
                // shmat below fails or the process crashes before
                // destroyImage's explicit cleanup runs -- no leaked
                // system-wide shm segment either way.
                shmctl(impl.shmInfo.shmid, IPC_RMID, nullptr);

                impl.shmInfo.shmaddr = impl.image->data = static_cast<char*>(shmat(impl.shmInfo.shmid, nullptr, 0));
                impl.shmInfo.readOnly = False;
                if (impl.shmInfo.shmaddr != reinterpret_cast<char*>(-1) &&
                    tryShmAttach(impl.display, impl.shmInfo))
                {
                    impl.shmAttached = true;
                }
                else
                {
                    if (impl.shmInfo.shmaddr != reinterpret_cast<char*>(-1))
                        shmdt(impl.shmInfo.shmaddr);
                    impl.image->data = nullptr; // don't let XDestroyImage free() the shmat'd/failed pointer
                    XDestroyImage(impl.image);
                    impl.image = nullptr;
                }
            }
            else
            {
                XDestroyImage(impl.image);
                impl.image = nullptr;
            }
        }
    }

    if (!impl.image) // XShm unavailable or failed -- plain XImage/XPutImage fallback
    {
        auto* data = static_cast<char*>(std::malloc(static_cast<std::size_t>(width) * height * 4));
        impl.image = XCreateImage(impl.display, visual, depth, ZPixmap, 0, data, w, h, 32, 0);
        impl.shmAttached = false;
    }

    impl.imageWidth = width;
    impl.imageHeight = height;
}

// Blits whatever ensureImage/present last wrote into impl.image, without
// re-resampling. impl.image already holds the last frame at the exact
// size it was drawn for, so this is also what re-paints the window when
// the X server/WM asks for one (Expose -- see run()) without a new
// present() call, e.g. right after the window is first mapped, or after
// being uncovered -- the equivalent of win32Window.cpp's WM_PAINT
// re-blitting its own stored lastPixels.
void blitCurrentImage(PlatformWindow::Impl& impl)
{
    if (!impl.image)
        return;
    GC gc = DefaultGC(impl.display, impl.screen);
    auto w = static_cast<unsigned>(impl.imageWidth);
    auto h = static_cast<unsigned>(impl.imageHeight);
    if (impl.shmAttached)
        XShmPutImage(impl.display, impl.window, gc, impl.image, 0, 0, 0, 0, w, h, False);
    else
        XPutImage(impl.display, impl.window, gc, impl.image, 0, 0, 0, 0, w, h);
    XFlush(impl.display);
}

} // namespace

PlatformWindow::PlatformWindow(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
PlatformWindow::~PlatformWindow()
{
    if (!m_impl) // moved-from
        return;
    destroyImage(*m_impl);
    if (m_impl->inputContext)
        XDestroyIC(m_impl->inputContext);
    if (m_impl->inputMethod)
        XCloseIM(m_impl->inputMethod);
    if (m_impl->display)
    {
        setCapsLockOn(m_impl->display, m_impl->originalCapsLockOn);
        if (m_impl->window)
            XDestroyWindow(m_impl->display, m_impl->window);
        XCloseDisplay(m_impl->display);
    }
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

    // X11 has no protocol-level "the interactive resize just ended" event
    // the way Win32 has WM_ENTERSIZEMOVE/WM_EXITSIZEMOVE (see platform.h's
    // run() comment for why that distinction matters) -- debounced
    // instead: resizePending tracks whether a ConfigureNotify has arrived
    // that onResizeEnd hasn't fired for yet, and the select() below waits
    // up to kResizeSettleMs for the *next* one before deciding the resize
    // has settled. That means blocking with a timeout instead of
    // XNextEvent's plain indefinite block, hence polling the display
    // connection's own fd (ConnectionNumber) rather than a Unix-specific
    // input-handling API of its own.
    constexpr int kResizeSettleMs = 300;
    bool resizePending = false;
    int pendingWidth = 0;
    int pendingHeight = 0;
    int xfd = ConnectionNumber(m_impl->display);

    while (true)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        timeval tv{0, kResizeSettleMs * 1000};
        // nullptr (block indefinitely) when nothing is pending -- only a
        // live resize needs the timeout at all.
        int ready = select(xfd + 1, &fds, nullptr, nullptr, resizePending ? &tv : nullptr);

        if (ready == 0 && resizePending)
        {
            // Timed out waiting for the next tick -- the resize has
            // settled. main.cpp only re-picks its baked atlas and
            // redraws crisply here, not on every onResize tick above.
            resizePending = false;
            if (m_impl->onResizeEnd)
                m_impl->onResizeEnd(pendingWidth, pendingHeight);
            continue;
        }

        while (XPending(m_impl->display))
        {
            XEvent event;
            XNextEvent(m_impl->display, &event);
            if (XFilterEvent(&event, None)) // let the input method consume its own events (compose/dead keys) first
                continue;

            switch (event.type)
            {
            case ClientMessage:
                if (static_cast<Atom>(event.xclient.data.l[0]) == m_impl->wmDeleteWindow)
                    return;
                break;
            case Expose:
                // count == 0 is the last Expose in a batch covering one
                // repaint region -- redrawing the whole window (always,
                // cheap) on every one of those would be redundant work.
                if (event.xexpose.count == 0)
                    blitCurrentImage(*m_impl);
                break;
            case ConfigureNotify:
                // No aspect-ratio correction here (an earlier version
                // tried to hold the window to a fixed shape by resizing/
                // repositioning it back on every tick): fighting the WM's
                // own live-resize tracking made interactive dragging feel
                // glitchy on every edge and corner. The window can be any
                // shape now -- main.cpp fits the square monitor into
                // whatever shape this reports, letterboxed/pillarboxed,
                // via Canvas::blitScaled.
                if (m_impl->onResize)
                    m_impl->onResize(event.xconfigure.width, event.xconfigure.height);
                resizePending = true;
                pendingWidth = event.xconfigure.width;
                pendingHeight = event.xconfigure.height;
                break;
            case FocusIn:
                handleFocus(*m_impl, true);
                break;
            case FocusOut:
                handleFocus(*m_impl, false);
                break;
            case KeyPress:
            case KeyRelease:
                handleKeyEvent(*m_impl, event);
                break;
            default:
                break;
            }
        }
    }
}

void PlatformWindow::present(std::span<const Pixel> pixels, int srcW, int srcH)
{
    assert(pixels.size() == static_cast<std::size_t>(srcW) * static_cast<std::size_t>(srcH));
    if (srcW <= 0 || srcH <= 0)
        return;

    // Trusts srcW/srcH as the size to present at, full stop -- no
    // XGetWindowAttributes call to independently re-query the window's
    // "live" size and compare against it the way an earlier version did.
    // That query is itself an X round-trip (real latency under WSLg's
    // proxied connection specifically), and worse, during a fast live
    // resize it routinely raced main.cpp's own already-current size:
    // by the time this ran, XGetWindowAttributes could report a *newer*
    // size than the ConfigureNotify that triggered this present() call,
    // making srcW/destW mismatch on nearly every tick and silently fall
    // back to a full bilinear resample -- exactly the "can't keep up
    // with the drag" cost this was supposed to have already fixed one
    // layer up (see PLAN.md). There's nothing to race once nothing else
    // gets queried: if the window has already moved on by the time this
    // lands on screen, the next present() -- already imminent, since
    // another resize tick either just fired or is about to -- corrects
    // it.
    ensureImage(*m_impl, srcW, srcH);
    if (!m_impl->image)
        return;

    auto* dst = reinterpret_cast<Pixel*>(m_impl->image->data);
    for (int y = 0; y < srcH; ++y)
    {
        std::memcpy(&dst[static_cast<std::size_t>(y) * srcW], &pixels[static_cast<std::size_t>(y) * srcW],
                    static_cast<std::size_t>(srcW) * sizeof(Pixel));
    }

    blitCurrentImage(*m_impl);
}

void PlatformWindow::setTitle(const std::string& title)
{
    XStoreName(m_impl->display, m_impl->window, title.c_str());
    Atom netWmName = XInternAtom(m_impl->display, "_NET_WM_NAME", False);
    Atom utf8String = XInternAtom(m_impl->display, "UTF8_STRING", False);
    XChangeProperty(m_impl->display, m_impl->window, netWmName, utf8String, 8, PropModeReplace,
                     reinterpret_cast<const unsigned char*>(title.c_str()), static_cast<int>(title.size()));
    XFlush(m_impl->display);
}

std::expected<PlatformWindow, std::string> createPlatformWindow(int width_px, int height_px, const char* title)
{
    Display* display = XOpenDisplay(nullptr);
    if (!display)
        return std::unexpected("XOpenDisplay failed -- no X server available (is DISPLAY set?)");

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    Window window = XCreateSimpleWindow(display, root, 0, 0, static_cast<unsigned>(width_px),
                                         static_cast<unsigned>(height_px), 0, BlackPixel(display, screen),
                                         BlackPixel(display, screen));
    if (!window)
    {
        XCloseDisplay(display);
        return std::unexpected("XCreateSimpleWindow failed");
    }

    XSelectInput(display, window,
                 ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask | FocusChangeMask);

    // No XSizeHints PAspect hint: the window can be any shape (see
    // createPlatformWindow's platform.h-side comment) -- an earlier
    // version set one to lock the shape, then found that with min_aspect
    // == max_aspect, tested window managers (GNOME/Mutter, at least)
    // refuse any single-edge drag that can't satisfy the ratio mid-drag,
    // and a runtime ConfigureNotify-based correction to work around that
    // fought the WM's own live-resize tracking instead. Not needed at
    // all once the window doesn't need to stay square.

    // _NET_WM_NAME (UTF-8) alongside the legacy XStoreName (Latin-1, but
    // every character fj's title contains today is ASCII, so it's a
    // harmless, more-compatible fallback for window managers/taskbars
    // that don't read _NET_WM_NAME).
    XStoreName(display, window, title);
    Atom netWmName = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8String = XInternAtom(display, "UTF8_STRING", False);
    XChangeProperty(display, window, netWmName, utf8String, 8, PropModeReplace,
                     reinterpret_cast<const unsigned char*>(title), static_cast<int>(std::strlen(title)));

    // _NET_WM_PID: standard EWMH way for window managers/task tools
    // (also `xdotool search --pid`) to associate this window with its
    // owning process. Nothing in fj itself depends on this being set.
    Atom netWmPid = XInternAtom(display, "_NET_WM_PID", False);
    auto pid = static_cast<unsigned long>(getpid());
    XChangeProperty(display, window, netWmPid, XA_CARDINAL, 32, PropModeReplace,
                     reinterpret_cast<const unsigned char*>(&pid), 1);

    // WM_DELETE_WINDOW: without this, the window manager's close button
    // kills the X connection out from under us instead of giving run()'s
    // event loop a chance to exit cleanly.
    Atom wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteWindow, 1);

    // Xutf8LookupString (KeyPress -> Kind::Char translation) needs an
    // input method + input context to decode compose sequences/dead keys
    // into real UTF-8. Not fatal if unavailable (handleKeyEvent just
    // skips Char events without an inputContext) -- better to run
    // without Unicode text input than fail to open the window at all.
    XIM inputMethod = XOpenIM(display, nullptr, nullptr, nullptr);
    XIC inputContext = nullptr;
    if (inputMethod)
        inputContext = XCreateIC(inputMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
                                  window, XNFocusWindow, window, nullptr);

    auto impl = std::make_unique<PlatformWindow::Impl>();
    impl->display = display;
    impl->screen = screen;
    impl->window = window;
    impl->wmDeleteWindow = wmDeleteWindow;
    impl->inputMethod = inputMethod;
    impl->inputContext = inputContext;

    XMapWindow(display, window);
    XFlush(display);

    return PlatformWindow(std::move(impl));
}

std::optional<int> displayDpi()
{
    Display* display = XOpenDisplay(nullptr);
    if (!display)
        return std::nullopt; // no display to query yet -- main.cpp picks the fallback, not this layer

    int dpi = queryOutputDpi(display);
    XCloseDisplay(display);
    if (dpi <= 0)
        return std::nullopt; // queryOutputDpi's own "unknown" case -- see its comment
    return dpi;
}
