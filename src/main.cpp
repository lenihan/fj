#include "squareGraphicsView.h"
#include <QApplication>
#include <QGraphicsScene>

namespace
{
    const qreal PAGE_WIDTH_IN = 5.5;   // half-letter width
    const qreal PAGE_HEIGHT_IN = 8.5;  // letter height
    const qreal SPREAD_WIDTH_IN = PAGE_WIDTH_IN * 2.0;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    QGraphicsScene scene;
    scene.setSceneRect(0.0, 0.0, SPREAD_WIDTH_IN, PAGE_HEIGHT_IN);
    scene.setBackgroundBrush(QBrush(Qt::black));
    
    SquareGraphicsView view(&scene);
    view.setSceneRect(scene.sceneRect());
    Q_ASSERT(scene.sceneRect() == view.sceneRect());
    
    view.resize(640, 640);
    view.show();
    return app.exec();    
}