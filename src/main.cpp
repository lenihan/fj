#include "squareGraphicsView.h"
#include "common.h"
#include "capsLockModifier.h"
#include <QApplication>
#include <QGraphicsScene>
#include <QScreen>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    QGraphicsScene scene;
    scene.setSceneRect(Screen::kLeft_scen, Screen::kTop_scen, Screen::kWidth_scen, Screen::kHeight_scen);
    scene.setBackgroundBrush(QBrush(Colors::kBlack));
    
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
        const int width_px = qCeil(dpiX * Screen::kWidth_scen);
        const int height_px = qCeil(dpiY * Screen::kHeight_scen);                                                 
        view.resize(width_px, height_px);
    }

    view.show();
    CapsLockModifier capsLockModifier(&view);
    return app.exec();
}