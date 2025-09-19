#include "mainwindow.h"
#include "squareGraphicsView.h"
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

    // Create and set up the view
    SquareGraphicsView view(&scene);
    view.setWindowTitle("Blue Square Example");
    view.resize(420, 320); // Slightly larger than scene for borders
    view.show();

    return app.exec();    
}