#include "mainwindow.h"
#include "squareGraphicsView.h"
#include <QApplication>
#include <QGraphicsScene>

namespace
{
const qreal PHYSICAL_SIDE_IN = 8.0;
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
    view.setSceneRect(x, y, w, h);
    Q_ASSERT(scene.sceneRect() == view.sceneRect());
    
    view.resize(640, 640);
    view.show();

    return app.exec();    
}