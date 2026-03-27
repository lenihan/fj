#include "squareGraphicsView.h"
#include "constants.h"
#include <QApplication>
#include <QGraphicsScene>
#include <QScreen>
#include <windows.h>


LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) 
    {
        KBDLLHOOKSTRUCT* p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (p->vkCode == VK_CAPITAL) 
        {
            // === REPLACE Caps Lock with Pause key ===
            // KBDLLHOOKSTRUCT fake = *p;                    // copy original struct
            // fake.vkCode = VK_PAUSE;                       // change to your replacement key

            // Re-inject the event as the new key
            // return CallNextHookEx(nullptr, nCode, wParam, reinterpret_cast<LPARAM>(&fake));
            // Do NOT return 1 here → we want the fake key to reach Qt

            // qDebug() << "CapsLock";
            // CONSUME the event → prevents toggle + LED + case change
            // return 1;   // 1 = eat the key
            
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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    

    
    QGraphicsScene scene;
    scene.setSceneRect(Screen::kLeft_scn, Screen::kTop_scn, Screen::kWidth_scn, Screen::kHeight_scn);
    scene.setBackgroundBrush(QBrush(Qt::black));
    
    SquareGraphicsView view(&scene);
    view.setSceneRect(scene.sceneRect());
    Q_ASSERT(scene.sceneRect() == view.sceneRect());
    
    // Set window size so that 1 inch on screen is 1 inch in real world
    {
        QWidget* mainWindow = view.window();
        Q_ASSERT(mainWindow);
        QScreen* screen = mainWindow->screen();
        Q_ASSERT(screen);
        const qreal dpiX = screen->physicalDotsPerInchX(); // 132 on Surface Pro 11,
                                                           // 109.22 34" Dell
        const qreal dpiY = screen->physicalDotsPerInchY(); // 129 on Surface Pro 11,
                                                           // 109.18 34" Dell
        const int width_px = qCeil(dpiX * Screen::kWidth_scn);
        const int height_px = qCeil(dpiY * Screen::kHeight_scn);                                                 
        view.resize(width_px, height_px);
    }

    view.show();



    SHORT state = GetKeyState(VK_CAPITAL);
    bool capsOn = (state & 0x0001) != 0; 
    if (capsOn)
    {
        // Turn it OFF by simulating a press + release
        keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY | 0, 0);
        keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }

    HHOOK m_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
    ///////////////////////////////////////////////////////////////////////////




    const int appReturnCode = app.exec();   
    
    
    
    
    
    
    
    
    UnhookWindowsHookEx(m_hHook); 

    if (capsOn)// != currentlyOn)
    {
        // Toggle it back to original by sending one press+release
        keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY | 0, 0);
        keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
    return appReturnCode;
}