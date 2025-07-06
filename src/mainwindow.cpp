#include "mainwindow.h"

#include "squareGraphicsView.h"

#include <QGraphicsScale>
#include <QWheelEvent>


MainWindow::MainWindow() : QMainWindow()
{
    // Desired size in inches
    m_desiredInches = 8.0;

    setWindowTitle("fj");

    m_scene = new QGraphicsScene(this);
    m_view = new SquareGraphicsView(m_scene, this);
    m_view->setScene(m_scene);

    setCentralWidget(m_view);
}

void MainWindow::wheelEvent(QWheelEvent* event)
{
    // Zoom in/out with mouse wheel
    qreal scaleFactor = 1.15;
    if (event->angleDelta().y() > 0)
    {
        m_view->scale(scaleFactor, scaleFactor); // Zoom in
    }
    else
    {
        m_view->scale(1.0 / scaleFactor, 1.0 / scaleFactor); // Zoom out
    }
}