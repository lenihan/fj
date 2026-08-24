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
#include <vector>

struct PlatformWindow::Impl
{
    HWND hwnd{nullptr};
    HHOOK capsLockHook{nullptr};
    bool originalCapsLockOn{false};
    std::function<void(const KeyEvent&)> onKey;

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

// Blits a top-down 32bpp buffer 1:1 at the origin. `pixels` already matches
// Pixel's documented in-memory layout (platform.h), so this is a direct
// StretchDIBits with no per-pixel conversion.
void blit(HDC hdc, const Pixel* pixels, int w, int h)
{
    if (!pixels || w <= 0 || h <= 0)
        return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // negative: top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(hdc, 0, 0, w, h, 0, 0, w, h, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
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
    case WM_KEYDOWN:
        if (impl && impl->onKey)
        {
            if (wParam == VK_RETURN)
                impl->onKey({KeyEvent::Kind::Enter, 0, true});
            else if (wParam == VK_BACK)
                impl->onKey({KeyEvent::Kind::Backspace, 0, true});
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

void PlatformWindow::run(std::function<void(const KeyEvent&)> onKey)
{
    m_impl->onKey = std::move(onKey);

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

std::expected<PlatformWindow, std::string> createPlatformWindow(int width_px, int height_px, const char* title)
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

    impl->hwnd = hwnd;
    g_activeImpl = impl.get();

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return PlatformWindow(std::move(impl));
}

int displayDpi()
{
    // Must happen before any DPI query or window creation to take effect;
    // idempotent, so calling it here (this is always main()'s first call
    // into the platform shell) is enough without a separate startup hook.
    // System-DPI-aware only, not per-monitor -- true per-monitor awareness
    // is deferred along with resize/DPI-change support (see platform.h).
    SetProcessDPIAware();

    HDC screenDC = GetDC(nullptr);
    int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
    ReleaseDC(nullptr, screenDC);
    return dpi;
}
