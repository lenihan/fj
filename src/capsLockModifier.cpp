#include "capsLockModifier.h"

#ifdef Q_OS_WINDOWS
#include <QCoreApplication>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QKeyEvent>
#include <Windows.h>
#include <qassert.h>

QObject* CapsLockModifier::m_keyReceiver = nullptr;

// Called before Caps Lock is processed by system.
// Throw away system response to Caps Lock.
// Send Caps Lock directly to our app.
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (p->vkCode == VK_CAPITAL)
        {
            QObject* target = CapsLockModifier::keyReceiver();
            const bool isDown =
                (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            QKeyEvent* keyEvent =
                new QKeyEvent(isDown ? QEvent::KeyPress : QEvent::KeyRelease,
                              Qt::Key_CapsLock, Qt::NoModifier);
            QCoreApplication::postEvent(target, keyEvent);
            return 1; // 1 = eat the key
        }
    }

    // Let all other keys pass through normally
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

CapsLockModifier::CapsLockModifier(QGraphicsView* view)
{
    Q_ASSERT(view);
    // Ensure view is already visable BEFORE attempting to disable Caps Lock.
    // When this code executed before view->show(), view came up in the
    // background.
    Q_ASSERT(view->isVisible());

    CapsLockModifier::m_keyReceiver = view;

    // If Caps Lock enabled, disable it with a simulated Caps Lock keypress
    const SHORT state = GetKeyState(VK_CAPITAL);
    m_capsOn = (state & 0x0001) != 0;
    if (m_capsOn)
        toggleCapsState();
    // Setup hook to handle Caps Lock presses to hide them from system
    HHOOK m_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                    GetModuleHandle(nullptr), 0);
}

CapsLockModifier::~CapsLockModifier()
{
    // Turn off hook
    UnhookWindowsHookEx(m_hook);

    // Return to original Caps Lock state
    if (m_capsOn)
        toggleCapsState();
}

QObject* CapsLockModifier::keyReceiver() { return m_keyReceiver; }

void CapsLockModifier::toggleCapsState()
{
    // Caps Lock press
    keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY | 0, 0);
    // Caps Lock release
    keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
}

#endif

#ifdef Q_OS_LINUX

#include <QCoreApplication>
#include <QDebug>
#include <QKeyEvent>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

DisableCapsLock::CapsLockModifier(QGraphicsView* view) : QObject(view)
{
    Q_ASSERT(view);
    Q_ASSERT(view->isVisible());

    m_display = XOpenDisplay(nullptr);
    if (!m_display)
    {
        qWarning() << "DisableCapsLock: Failed to open X11 display - probably "
                      "running on Wayland or headless";
        return;
    }

    m_root = DefaultRootWindow(m_display);
    m_capsKeycode = XKeysymToKeycode(m_display, XK_Caps_Lock);

    // Get initial Caps Lock state (LED)
    unsigned int state = 0;
    if (XkbGetIndicatorState(m_display, XkbUseCoreKbd, &state) == Success)
    {
        m_capsOn = (state & 0x01) != 0;
    }

    if (m_capsOn)
    {
        toggleCapsState(); // Turn LED off initially (mimics Windows behavior)
    }

    grabCapsLock();

    // Install event filter as backup / for the view
    view->installEventFilter(this);
    QCoreApplication::instance()->installEventFilter(this);
}

DisableCapsLock::~DisableCapsLock()
{
    ungrabCapsLock();

    if (m_display)
    {
        if (m_capsOn)
        {
            toggleCapsState(); // Restore original LED state
        }
        XCloseDisplay(m_display);
    }
}

void DisableCapsLock::grabCapsLock()
{
    if (!m_display || !m_capsKeycode)
        return;

    // Grab Caps Lock for many common modifier combinations (including NumLock,
    // etc.)
    const unsigned int masks[] = {
        0,
        Mod2Mask, // NumLock
        LockMask, // CapsLock (self)
        Mod2Mask | LockMask,
        ShiftMask,
        ControlMask,
        Mod1Mask, // Alt
        // Add more if needed (e.g. Mod4Mask for Super)
    };

    for (unsigned int mask : masks)
    {
        XGrabKey(m_display, m_capsKeycode, mask, m_root, False, GrabModeAsync,
                 GrabModeAsync);
    }

    XFlush(m_display);
    qDebug() << "DisableCapsLock: Caps Lock grabbed via X11 API";
}

void DisableCapsLock::ungrabCapsLock()
{
    if (!m_display || !m_capsKeycode)
        return;
    XUngrabKey(m_display, m_capsKeycode, AnyModifier, m_root);
    XFlush(m_display);
}

void DisableCapsLock::toggleCapsState()
{
    if (!m_display || !m_capsKeycode)
        return;

    // Simulate physical Caps Lock press + release to toggle LED/state
    XTestFakeKeyEvent(m_display, m_capsKeycode, True, CurrentTime);
    XTestFakeKeyEvent(m_display, m_capsKeycode, False, CurrentTime);
    XFlush(m_display);
}

#endif