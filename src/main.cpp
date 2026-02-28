#include "squareGraphicsView.h"
#include <QApplication>
#include <QGraphicsScene>

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
    
    view.resize(640, 640);
    view.show();
    return app.exec();    
}