#include "mainwindow.h"
#include "squareGraphicsView.h"
#include <QApplication>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Create the scene
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 400, 300); // Set scene size

    // Create and set up the view
    SquareGraphicsView view(&scene);
    view.setWindowTitle("Blue Square Example");
    view.resize(420, 320); // Slightly larger than scene for borders
    view.show();

    return app.exec();    
}