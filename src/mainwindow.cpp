#include "mainwindow.h"

#include "squareGraphicsView.h"

#include <QGraphicsScale>
#include <QScreen>

MainWindow::MainWindow() : QMainWindow()
{
    m_scene = new QGraphicsScene(this);
    m_view = new SquareGraphicsView(m_scene, this);
    m_view->setScene(m_scene);

    setCentralWidget(m_view);
}

