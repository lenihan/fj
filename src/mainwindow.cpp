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
    rectItem = new AnimatedGraphicsItem(AnimatedGraphicsItem::ShapeType::Rectangle, QRectF(-50, -50, 100, 50), Qt::blue);
    scene->addItem(rectItem);

    // Add a circle
    circleItem = new AnimatedGraphicsItem(AnimatedGraphicsItem::ShapeType::Circle, QRectF(0, 0, 50, 50), Qt::red);
    scene->addItem(circleItem);

    // Create and configure the view
    view = new QGraphicsView(scene, this);
    view->setRenderHint(QPainter::Antialiasing);
    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setCentralWidget(view);

    // Set window properties
    setWindowTitle("fj");
    resize(500, 500);

    // Animate the rectangle (move horizontally)
    rectAnimation = new QPropertyAnimation(rectItem, "pos");
    rectAnimation->setDuration(2000); // 2 seconds
    rectAnimation->setStartValue(QPointF(-150, -50));
    rectAnimation->setEndValue(QPointF(150, -50));
    rectAnimation->setLoopCount(-1); // Loop indefinitely
    rectAnimation->setEasingCurve(QEasingCurve::InOutSine); // Smooth motion
    rectAnimation->start();

    // Animate the circle (move vertically)
    circleAnimation = new QPropertyAnimation(circleItem, "pos");
    circleAnimation->setDuration(2000); // 2 seconds
    circleAnimation->setStartValue(QPointF(0, -150));
    circleAnimation->setEndValue(QPointF(0, 150));
    circleAnimation->setLoopCount(-1); // Loop indefinitely
    circleAnimation->setEasingCurve(QEasingCurve::InOutSine); // Smooth motion
    circleAnimation->start();
}

MainWindow::~MainWindow()
{
    delete rectAnimation;
    delete circleAnimation;
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