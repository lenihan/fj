#pragma once

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