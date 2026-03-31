#pragma once
#include <qsystemdetection.h>
class QGraphicsView;
class QObject;

#ifdef Q_OS_WINDOWS

typedef struct HHOOK__*
    HHOOK; // This is the official way Windows forward-declares it

class CapsLockModifier
{
  public:
    CapsLockModifier(QGraphicsView* view);
    ~CapsLockModifier();
    static QObject* keyReceiver();

  private:
    void toggleCapsState();
    bool m_capsOn{false};
    HHOOK m_hook{nullptr};
    static QObject* m_keyReceiver;
};
#endif

#ifdef Q_OS_LINUX

class CapsLockModifier
{
  public:
    CapsLockModifier(QGraphicsView* view);
    ~CapsLockModifier();

  private:
    Display* m_display = nullptr;
    Window m_root = None;
    int m_capsKeycode = 0;
    bool m_capsOn = false;

    void toggleCapsState();
    void grabCapsLock();
    void ungrabCapsLock();
};

#endif