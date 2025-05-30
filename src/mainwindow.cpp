#include "mainwindow.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QWheelEvent>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // Create the scene
    scene = new QGraphicsScene(this);
    scene->setSceneRect(-200, -200, 400, 400); // Set scene size

    // Add a rectangle
    QGraphicsRectItem *rect = new QGraphicsRectItem(-50, -50, 100, 50);
    rect->setBrush(Qt::blue);
    scene->addItem(rect);

    // Add a circle
    QGraphicsEllipseItem *circle = new QGraphicsEllipseItem(0, 0, 50, 50);
    circle->setBrush(Qt::red);
    scene->addItem(circle);

    // Create and configure the view
    view = new QGraphicsView(scene, this);
    view->setRenderHint(QPainter::Antialiasing);
    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setCentralWidget(view);

    // Set window properties
    setWindowTitle("fj");
    resize(1000, 1000);
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    // Zoom in/out with mouse wheel
    qreal scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        view->scale(scaleFactor, scaleFactor); // Zoom in
    }
    else {
        view->scale(1.0 / scaleFactor, 1.0 / scaleFactor); // Zoom out
    }
}