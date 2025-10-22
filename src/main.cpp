#include "squareGraphicsView.h"
#include <QApplication>
#include <QGraphicsScene>

namespace
{
    // A4: Paper,      8.27" x 11.69" 
    // A5: Half of A4, 5.83" x  8.27"
    // A6: Half of A5, 4.13" x  5.83"
    // 
    // A5 fits inside FJ screen
    // FJ screen fits inside A4
    // For two pages, use 2 A6 portrait or landscape
    //     - Design for A5 usage
    //     - Print to A4
    const qreal PHYSICAL_SIDE_IN = 11.69; 
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    QGraphicsScene scene;
    const qreal x = 0.0;
    const qreal y = 0.0;
    const qreal w = PHYSICAL_SIDE_IN;
    const qreal h = PHYSICAL_SIDE_IN;
    scene.setSceneRect(x, y, w, h);
    scene.setBackgroundBrush(QBrush(Qt::black));
    
    SquareGraphicsView view(&scene);
    view.setSceneRect(scene.sceneRect());
    Q_ASSERT(scene.sceneRect() == view.sceneRect());
    
    view.resize(640, 640);
    view.show();
    return app.exec();    
}