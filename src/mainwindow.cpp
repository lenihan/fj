#include "mainwindow.h"
#include "squareGraphicsView.h"
#include <QResizeEvent>

MainWindow::MainWindow() : QMainWindow()
{
    m_scene = new QGraphicsScene(this);
    m_view = new SquareGraphicsView(m_scene, this);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    const int width = event->size().width();
    const int height = event->size().height();

    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    if (width < height)
    {
        // Fill width
        x = 0;
        y = (height - width) / 2.0;
        w = width;
        h = width;
    }
    else
    {
        // Fill height
        x = (width - height) / 2.0;
        y = 0;
        w = height;
        h = height;
    }
    m_view->setGeometry(x, y, w, h);
}
