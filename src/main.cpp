#include "mainwindow.h"
#include <QApplication>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsRectItem>

int main(int argc, char* argv[])
{
#if 0
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
#endif
    QApplication app(argc, argv);

    // Create the scene
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 400, 300); // Set scene size

    // Create a blue square (50x50 pixels)
    QGraphicsRectItem *square = scene.addRect(0, 0, 50, 50);
    square->setBrush(Qt::blue); // Set fill color to blue

    // Center the square in the scene
    square->setPos((scene.width() - square->rect().width()) / 2,
                   (scene.height() - square->rect().height()) / 2);

    // Create and set up the view
    QGraphicsView view(&scene);
    view.setWindowTitle("Blue Square Example");
    view.resize(420, 320); // Slightly larger than scene for borders
    view.show();

    return app.exec();    
}