#include "mainwindow.h"

#include "squareGraphicsView.h"

#include <QGraphicsScale>
#include <QGridLayout>
#include <QScreen>

MainWindow::MainWindow() : QMainWindow()
{
    // Create central widget and layout
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QGridLayout* layout = new QGridLayout(centralWidget);

    // Create square widget with scene
    m_scene = new QGraphicsScene(this);
    m_view = new SquareGraphicsView(m_scene, this);

    // Add the square widget to the layout
    layout->addWidget(m_view, 0, 0, 1, 1, Qt::AlignCenter);

    // Allow the layout to stretch and fill available space
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}
