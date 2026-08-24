// win32Window.cpp -- the Windows implementation of platform.h's contract.
// Window creation, the message loop, KeyEvent translation, and presenting
// pixels via GDI. Nothing here knows about cards/cursor/fonts; see
// platform.h's file comment for the boundary this respects.
//
// CapsLock needs a low-level keyboard hook (WH_KEYBOARD_LL), not plain
// WM_KEYDOWN/WM_KEYUP: physically pressing Caps Lock is a toggle to
// Windows, not a held key, so ordinary key messages only report the two
// edges as an instant down+up pair and Windows also flips its own toggle
// state (which would start capitalizing subsequent WM_CHAR letters). The
// hook sees the raw hardware edges before Windows' toggle-key state
// machine coalesces them, and eating the event there (return 1) stops
// Windows from acting on it at all. This is a near-direct port of the old
// Qt app's capsLockModifier.cpp, which did the same thing to forward a
// synthetic QKeyEvent instead of a KeyEvent.

#include "platform.h"

#include <windows.h>

#include <cassert>
#include <cmath>
#include <vector>

struct PlatformWindow::Impl
{
    HWND hwnd{nullptr};
    HHOOK capsLockHook{nullptr};
    bool originalCapsLockOn{false};
    double aspectRatio{1.0}; // width/height locked during interactive resize; see WM_SIZING
    std::function<void(const KeyEvent&)> onKey;
    std::function<void(int width_px, int height_px)> onResize;

    // Kept so WM_PAINT (e.g. after alt-tab, or another window dragged over
    // ours) has something to redraw with -- present() doesn't get called
    // again just because the OS wants a repaint.
    std::vector<Pixel> lastPixels;
    int lastWidth{0};
    int lastHeight{0};
};

namespace
{

constexpr wchar_t kWindowClassName[] = L"fjWindowClass";

// SetWindowsHookEx needs a plain function pointer, so the hook proc can't
// capture anything -- it reaches the one live window through this. Only
// one PlatformWindow is ever meant to exist per process (platform.h's
// "only one implementation is ever linked in" reasoning applies here too),
// so a single pointer is enough.
PlatformWindow::Impl* g_activeImpl = nullptr;

bool capsLockOn()
{
    return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
}

void setCapsLockOn(bool on)
{
    if (capsLockOn() == on)
        return;
    keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY, 0);
    keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
}

LRESULT CALLBACK lowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        auto* p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (p->vkCode == VK_CAPITAL && g_activeImpl && g_activeImpl->onKey)
        {
            bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            g_activeImpl->onKey({KeyEvent::Kind::CapsLock, 0, isDown});
            return 1; // eat it -- stop Windows from toggling caps lock itself
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

std::wstring toWide(const char* s)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (len <= 0)
        return {};
    std::wstring wide(static_cast<std::size_t>(len - 1), L'\0'); // drop the null MultiByteToWideChar counted
    MultiByteToWideChar(CP_UTF8, 0, s, -1, wide.data(), len);
    return wide;
}

// A "Fix Calibration..." entry on the window's system menu (opened by
// right-clicking the title bar, clicking the window icon, or Alt+Space) --
// the calibration button this is standing in for. A real custom-drawn
// caption button (the more literal reading of "a button on the title
// bar") turned out not to be practical: modern Windows composites the
// title bar via DWM from its own cached surface, and the classic
// GetWindowDC-into-the-non-client-area trick that worked before Vista
// doesn't reliably survive that compositing -- it was tried and simply
// didn't render. The system menu is 100% standard OS UI, not fighting
// DWM at all, and still lives on/is reached from the title bar.
constexpr UINT kCalibrateMenuId = 0x1000; // below WM_SYSCOMMAND's reserved 0xF000+ range

void addCalibrateMenuItem(HWND hwnd)
{
    HMENU sysMenu = GetSystemMenu(hwnd, FALSE);
    if (!sysMenu)
        return;
    AppendMenuW(sysMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sysMenu, MF_STRING, kCalibrateMenuId, L"Fix Calibration...");
}

// Stretches a top-down 32bpp buffer to fill whatever the window's client
// area currently is -- not necessarily srcW x srcH. That mismatch is
// exactly what makes live resizing feel continuous: the rendered card
// only changes resolution in the discrete steps of whichever baked atlas
// is active (main.cpp's onResize), but this stretch covers every pixel in
// between, every time Windows repaints (interactive drag included, since
// CS_HREDRAW | CS_VREDRAW forces a repaint on every size change). `pixels`
// already matches Pixel's documented in-memory layout (platform.h), so
// this is a direct StretchDIBits with no per-pixel conversion.
//
// HALFTONE gives a properly area-averaged resample instead of nearest-
// neighbor's blockiness; it requires SetBrushOrgEx after every
// SetStretchBltMode(HALFTONE) call or the dither pattern drifts (a
// documented GDI quirk, not optional).
void blit(HDC hdc, const Pixel* pixels, int srcW, int srcH)
{
    if (!pixels || srcW <= 0 || srcH <= 0)
        return;

    RECT client{};
    GetClientRect(WindowFromDC(hdc), &client);
    int destW = client.right - client.left;
    int destH = client.bottom - client.top;
    if (destW <= 0 || destH <= 0)
        return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = srcW;
    bmi.bmiHeader.biHeight = -srcH; // negative: top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, nullptr);
    StretchDIBits(hdc, 0, 0, destW, destH, 0, 0, srcW, srcH, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

// Mirrors CapsLockModifier::onWindowActiveChanged(): force system Caps
// Lock off (after saving its state) whenever our window becomes active, so
// typing in this app never depends on whatever the OS's toggle state
// happened to be before it got focus; restore that state on deactivate so
// other apps aren't left with our forced-off state.
void handleActivate(PlatformWindow::Impl& impl, bool active)
{
    if (active)
    {
        impl.originalCapsLockOn = capsLockOn();
        if (impl.capsLockHook)
        {
            UnhookWindowsHookEx(impl.capsLockHook);
            impl.capsLockHook = nullptr;
        }
        setCapsLockOn(false);
        impl.capsLockHook = SetWindowsHookExW(WH_KEYBOARD_LL, lowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
    }
    else
    {
        if (impl.capsLockHook)
        {
            UnhookWindowsHookEx(impl.capsLockHook);
            impl.capsLockHook = nullptr;
        }
        setCapsLockOn(impl.originalCapsLockOn);
    }
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* impl = reinterpret_cast<PlatformWindow::Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CREATE:
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; // every pixel is repainted every frame; skip the default erase to avoid flicker
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (impl)
            blit(hdc, impl->lastPixels.data(), impl->lastWidth, impl->lastHeight);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ACTIVATE:
        if (impl)
            handleActivate(*impl, LOWORD(wParam) != WA_INACTIVE);
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == kCalibrateMenuId)
        {
            if (impl && impl->onKey)
                impl->onKey({KeyEvent::Kind::Calibrate, 0, true});
            return 0;
        }
        break;
    case WM_SIZE:
        if (impl && impl->onResize)
            impl->onResize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_SIZING:
    {
        // Keeps the window locked to the card's aspect ratio while the
        // user drags a resize border, so the client area (and thus the
        // stretched card blit() draws) is never distorted and present()
        // never needs letterbox/fill-color logic -- a deliberate departure
        // from the old Qt app, which allowed free-aspect resize and
        // letterboxed instead (see PLAN.md).
        auto* dragRect = reinterpret_cast<RECT*>(lParam);
        RECT windowRect{};
        RECT clientRect{};
        GetWindowRect(hwnd, &windowRect);
        GetClientRect(hwnd, &clientRect); // size only; origin is client-relative (0,0)
        int chromeW = (windowRect.right - windowRect.left) - (clientRect.right - clientRect.left);
        int chromeH = (windowRect.bottom - windowRect.top) - (clientRect.bottom - clientRect.top);

        int clientW = (dragRect->right - dragRect->left) - chromeW;
        int clientH = (dragRect->bottom - dragRect->top) - chromeH;

        // Dragging a pure top/bottom edge drives from the new height;
        // every other edge/corner (including all four corners) drives
        // from the new width -- a common simplification for aspect-locked
        // resize that keeps corner-dragging from fighting a "which
        // dimension leads" ambiguity.
        double aspectRatio = impl ? impl->aspectRatio : 1.0;
        if (wParam == WMSZ_TOP || wParam == WMSZ_BOTTOM)
            clientW = static_cast<int>(std::lround(clientH * aspectRatio));
        else
            clientH = static_cast<int>(std::lround(clientW / aspectRatio));

        int newW = clientW + chromeW;
        int newH = clientH + chromeH;

        if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT)
            dragRect->left = dragRect->right - newW;
        else
            dragRect->right = dragRect->left + newW;

        if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
            dragRect->top = dragRect->bottom - newH;
        else
            dragRect->bottom = dragRect->top + newH;

        return TRUE;
    }
    case WM_KEYDOWN:
        if (impl && impl->onKey)
        {
            if (wParam == VK_RETURN)
                impl->onKey({KeyEvent::Kind::Enter, 0, true});
            else if (wParam == VK_BACK)
                impl->onKey({KeyEvent::Kind::Backspace, 0, true});
            else if (wParam == VK_F5) // see platform.h's Kind::Calibrate comment
                impl->onKey({KeyEvent::Kind::Calibrate, 0, true});
            // Everything else either arrives as WM_CHAR below (printable
            // text, including i/k/j/l home-row navigation -- see
            // platform.h's Kind::Char comment) or isn't part of this
            // contract (Escape/Tab/Delete: see platform.h's file comment).
        }
        break;
    case WM_CHAR:
        if (impl && impl->onKey)
        {
            auto c = static_cast<char32_t>(wParam);
            // WM_CHAR also reports control characters (\r, \b, \t, Esc) --
            // those are handled above via WM_KEYDOWN instead, so only
            // forward genuine printable text here.
            if (c >= 0x20 && c != 0x7F)
                impl->onKey({KeyEvent::Kind::Char, c, true});
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

PlatformWindow::PlatformWindow(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
PlatformWindow::~PlatformWindow()
{
    if (!m_impl) // moved-from
        return;
    if (m_impl->capsLockHook)
        UnhookWindowsHookEx(m_impl->capsLockHook);
    setCapsLockOn(m_impl->originalCapsLockOn);
    if (g_activeImpl == m_impl.get())
        g_activeImpl = nullptr;
}
PlatformWindow::PlatformWindow(PlatformWindow&&) noexcept = default;
PlatformWindow& PlatformWindow::operator=(PlatformWindow&&) noexcept = default;

void PlatformWindow::run(std::function<void(const KeyEvent&)> onKey,
                          std::function<void(int width_px, int height_px)> onResize)
{
    m_impl->onKey = std::move(onKey);
    m_impl->onResize = std::move(onResize);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void PlatformWindow::present(std::span<const Pixel> pixels, int w, int h)
{
    assert(pixels.size() == static_cast<std::size_t>(w) * static_cast<std::size_t>(h));

    m_impl->lastPixels.assign(pixels.begin(), pixels.end());
    m_impl->lastWidth = w;
    m_impl->lastHeight = h;

    HDC hdc = GetDC(m_impl->hwnd);
    blit(hdc, m_impl->lastPixels.data(), w, h);
    ReleaseDC(m_impl->hwnd, hdc);
}

void PlatformWindow::setTitle(const std::string& title)
{
    SetWindowTextW(m_impl->hwnd, toWide(title.c_str()).c_str());
}

std::expected<PlatformWindow, std::string> createPlatformWindow(int width_px, int height_px, double aspectRatio,
                                                                  const char* title)
{
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = wndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kWindowClassName; // no hbrBackground: we paint every pixel ourselves
        if (!RegisterClassExW(&wc))
            return std::unexpected("RegisterClassExW failed");
        classRegistered = true;
    }

    auto impl = std::make_unique<PlatformWindow::Impl>();
    impl->aspectRatio = aspectRatio;

    // width_px/height_px are the desired CLIENT area (the canvas' pixel
    // size, chosen in main() to match the physical card target) -- grow
    // the window rect so the client area actually ends up that size once
    // the OS adds its title bar/borders.
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect{0, 0, width_px, height_px};
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowExW(0, kWindowClassName, toWide(title).c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                                 rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, hInstance,
                                 impl.get());
    if (!hwnd)
        return std::unexpected("CreateWindowExW failed");

    // AdjustWindowRect's border-size guess (GetSystemMetrics(SM_CXSIZEFRAME)
    // etc., not itself DPI-parameterized) doesn't always land the real
    // client area on exactly width_px x height_px -- measure what
    // CreateWindowExW actually produced and correct for the difference
    // directly, rather than trusting the prediction (this matters for
    // main.cpp's physical-inch sizing: a client area off by even a few
    // percent measurably fails to be Card::kWidth_in/kHeight_in on a real
    // screen).
    RECT actualClient{};
    GetClientRect(hwnd, &actualClient);
    int clientDeltaW = width_px - (actualClient.right - actualClient.left);
    int clientDeltaH = height_px - (actualClient.bottom - actualClient.top);
    if (clientDeltaW != 0 || clientDeltaH != 0)
    {
        RECT windowRect{};
        GetWindowRect(hwnd, &windowRect);
        SetWindowPos(hwnd, nullptr, 0, 0, (windowRect.right - windowRect.left) + clientDeltaW,
                     (windowRect.bottom - windowRect.top) + clientDeltaH, SWP_NOMOVE | SWP_NOZORDER);
    }

    impl->hwnd = hwnd;
    g_activeImpl = impl.get();

    addCalibrateMenuItem(hwnd);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return PlatformWindow(std::move(impl));
}

int displayDpi()
{
    // Must happen before any DPI query or window creation to take effect;
    // idempotent, so calling it here (this is always main()'s first call
    // into the platform shell) is enough without a separate startup hook.
    //
    // Per-monitor-v2, not plain SetProcessDPIAware's "system DPI aware":
    // system-DPI-aware processes can still have their entire window
    // bitmap-scaled by the OS on top of whatever the app itself draws
    // (DPI virtualization) if the actual monitor's scaling differs from
    // what Windows decided the "system" DPI was -- which silently
    // invalidates every physical-inch pixel calculation in this file and
    // main.cpp, since the app has no way to see or account for a scaling
    // pass it doesn't know is happening. Per-monitor-v2 awareness turns
    // that off entirely: this process always receives and draws in real,
    // unscaled physical pixels. (True per-monitor *tracking* -- reacting
    // to WM_DPICHANGED when the window moves to a differently-scaled
    // monitor -- is still deferred; this only stops the OS from silently
    // rescaling the one monitor the window opens on.)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HDC screenDC = GetDC(nullptr);
    int widthPx = GetDeviceCaps(screenDC, HORZRES);
    int widthMm = GetDeviceCaps(screenDC, HORZSIZE);
    ReleaseDC(nullptr, screenDC);

    // Deliberately NOT GetDeviceCaps(..., LOGPIXELSX): that returns 96 x
    // the OS's display-scaling percentage (a UI-size preference the user
    // picked, e.g. 100%/125%/150%/200%), which has no necessary
    // relationship to the monitor's actual pixel density -- using it here
    // measurably produced a card that was NOT physically 5"x3" on a real
    // screen. HORZSIZE (the monitor's physical width in mm, from its
    // EDID) combined with HORZRES (its horizontal resolution) gives the
    // display's true pixels-per-inch instead, which is what "1 inch on
    // screen == 1 physical inch" actually requires.
    //
    // Some displays (especially virtual/remote ones, or a bad EDID) report
    // HORZSIZE as 0 or nonsense; 96 (the ordinary "unscaled" baseline) is
    // a reasonable fallback rather than dividing by zero or trusting a
    // clearly-wrong value.
    if (widthMm <= 0)
        return 96;

    double widthIn = widthMm / 25.4;
    return static_cast<int>(std::lround(widthPx / widthIn));
}
