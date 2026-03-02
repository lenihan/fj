#include "squareGraphicsView.h"
#include <QApplication>
#include <QGraphicsScene>
#include <QScreen>

namespace
{
    const qreal DISPLAY_WIDTH_IN = 5.0;   // Width of 3x5 card
    const qreal DISPLAY_HEIGHT_IN = 5.0;  // Height of 3x5 card + 2" for UI
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    QGraphicsScene scene;
    scene.setSceneRect(0.0, 0.0, DISPLAY_WIDTH_IN, DISPLAY_HEIGHT_IN);
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
        const int width_px = qCeil(dpiX * DISPLAY_WIDTH_IN);
        const int height_px = qCeil(dpiY * DISPLAY_HEIGHT_IN);                                                 
        view.resize(width_px, height_px);
    }

    view.show();
    return app.exec();    
}