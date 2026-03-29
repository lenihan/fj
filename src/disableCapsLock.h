#pragma once
#include <qsystemdetection.h>

#ifdef Q_OS_WINDOWS
#include <windows.h>

class QGraphicsView;

class DisableCapsLock
{
  public:
    DisableCapsLock(QGraphicsView* view);
    ~DisableCapsLock();

  private:
    void simulateCapsPressRelease();
    bool m_capsOn{false};
    HHOOK m_hook{nullptr};
};
#endif

#ifdef Q_OS_LINUX

#include <QObject>
#include <QGraphicsView>

class DisableCapsLock : public QObject
{
    Q_OBJECT

public:
    explicit DisableCapsLock(QGraphicsView* view);
    ~DisableCapsLock() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    Display* m_display = nullptr;
    Window   m_root = None;
    int      m_capsKeycode = 0;
    bool     m_capsOn = false;

    void toggleCapsState();
    void grabCapsLock();
    void ungrabCapsLock();
};

#endif