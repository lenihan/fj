#include "disableCapsLock.h"
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
