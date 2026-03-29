#include "disableCapsLock.h"

#ifdef Q_OS_WINDOWS
#include <QGraphicsView>
#include <qassert.h>

// Replace Caps     Lock with Pause key for key down/up detection
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) 
    {
        KBDLLHOOKSTRUCT* p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (p->vkCode == VK_CAPITAL) 
        {
            // === REPLACE Caps Lock with Pause key ===
            bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            
            // === Manually send replacement key instead ===
            INPUT input = {};
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = VK_PAUSE;           // Change to VK_SCROLL, VK_F24, etc. if you prefer
            input.ki.dwFlags = isDown ? 0 : KEYEVENTF_KEYUP;
            
            SendInput(1, &input, sizeof(INPUT));
            return 1;   // 1 = eat the key
        }
    }

    // Let all other keys pass through normally
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

DisableCapsLock::DisableCapsLock(QGraphicsView* view)
{
    Q_ASSERT(view);
    // If not visible, disabling caps lock could cause this window to open in background
    Q_ASSERT(view->isVisible()); 

    const SHORT state = GetKeyState(VK_CAPITAL);
    m_capsOn = (state & 0x0001) != 0;
    if (m_capsOn)
    {
        simulateCapsPressRelease();
    }
    HHOOK m_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                    GetModuleHandle(nullptr), 0);
}

DisableCapsLock::~DisableCapsLock()
{
    UnhookWindowsHookEx(m_hook);
    if (m_capsOn)
    {
        simulateCapsPressRelease();
    }
}

void DisableCapsLock::simulateCapsPressRelease()
{
    // Caps press
    keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY | 0, 0);
    // Caps release
    keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
}

#endif



#ifdef Q_OS_LINUX

#include "disableCapsLock.h"
#include <QCoreApplication>
#include <QKeyEvent>
#include <QDebug>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/Xutil.h>

DisableCapsLock::DisableCapsLock(QGraphicsView* view)
    : QObject(view)
{
    Q_ASSERT(view);
    Q_ASSERT(view->isVisible());

    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        qWarning() << "DisableCapsLock: Failed to open X11 display - probably running on Wayland or headless";
        return;
    }

    m_root = DefaultRootWindow(m_display);
    m_capsKeycode = XKeysymToKeycode(m_display, XK_Caps_Lock);

    // Get initial Caps Lock state (LED)
    unsigned int state = 0;
    if (XkbGetIndicatorState(m_display, XkbUseCoreKbd, &state) == Success) {
        m_capsOn = (state & 0x01) != 0;
    }

    if (m_capsOn) {
        toggleCapsState();  // Turn LED off initially (mimics Windows behavior)
    }

    grabCapsLock();

    // Install event filter as backup / for the view
    view->installEventFilter(this);
    QCoreApplication::instance()->installEventFilter(this);

}

DisableCapsLock::~DisableCapsLock()
{
    ungrabCapsLock();

    if (m_display) {
        if (m_capsOn) {
            toggleCapsState();  // Restore original LED state
        }
        XCloseDisplay(m_display);
    }
}

void DisableCapsLock::grabCapsLock()
{
    if (!m_display || !m_capsKeycode) return;

    // Grab Caps Lock for many common modifier combinations (including NumLock, etc.)
    const unsigned int masks[] = {
        0,
        Mod2Mask,           // NumLock
        LockMask,           // CapsLock (self)
        Mod2Mask | LockMask,
        ShiftMask,
        ControlMask,
        Mod1Mask,           // Alt
        // Add more if needed (e.g. Mod4Mask for Super)
    };

    for (unsigned int mask : masks) {
        XGrabKey(m_display, m_capsKeycode, mask, m_root,
                 False, GrabModeAsync, GrabModeAsync);
    }

    XFlush(m_display);
    qDebug() << "DisableCapsLock: Caps Lock grabbed via X11 API";
}

void DisableCapsLock::ungrabCapsLock()
{
    if (!m_display || !m_capsKeycode) return;
    XUngrabKey(m_display, m_capsKeycode, AnyModifier, m_root);
    XFlush(m_display);
}

bool DisableCapsLock::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);

        if (ke->key() == Qt::Key_CapsLock) {
            bool isPress = (event->type() == QEvent::KeyPress);

            // Create and post Pause key event instead
            QKeyEvent* pauseEvent = new QKeyEvent(
                isPress ? QEvent::KeyPress : QEvent::KeyRelease,
                Qt::Key_Pause,
                ke->modifiers(),
                0);

            QCoreApplication::postEvent(watched, pauseEvent);

            return true;  // Eat the Caps Lock event
        }
    }
    return QObject::eventFilter(watched, event);
}

void DisableCapsLock::toggleCapsState()
{
    if (!m_display || !m_capsKeycode) return;

    // Simulate physical Caps Lock press + release to toggle LED/state
    XTestFakeKeyEvent(m_display, m_capsKeycode, True,  CurrentTime);
    XTestFakeKeyEvent(m_display, m_capsKeycode, False, CurrentTime);
    XFlush(m_display);
}

#endif