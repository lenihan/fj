#include "squareGraphicsView.h"
#include "constants.h"
#include "disableCapsLock.h"
#include <QApplication>
#include <QGraphicsScene>
#include <QScreen>

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
    DisableCapsLock disableCapsLock(&view);
    return app.exec();
}