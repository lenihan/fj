#include "mainwindow.h"
#include "squareGraphicsView.h"
#include <QResizeEvent>

namespace
{
const qreal PHYSICAL_SIDE_IN = 8.0;
}

MainWindow::MainWindow() : QMainWindow()
{
    m_scene = new QGraphicsScene(this);
    m_view = new SquareGraphicsView(m_scene, this);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    const int width = event->size().width();
    const int height = event->size().height();

    // Set view geometry
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
    m_scene->setSceneRect(0, 0, w, h);

    // Get dpi
    qreal dpiX = 0.0;
    qreal dpiY = 0.0;
    {
        QWidget* mainWindow = window();
        Q_ASSERT(mainWindow);
        QScreen* screen = mainWindow->screen();
        Q_ASSERT(screen);
        dpiX = screen->physicalDotsPerInchX(); // 132 on Surface Pro 11,
                                               // 109.22 34" Dell
        dpiY = screen->physicalDotsPerInchY(); // 129 on Surface Pro 11,
                                               // 109.18 34" Dell
    }

    // Calculate square side in inches
    int side_px = 0;
    qreal dpi = 0;
    if (width < height)
    {
        side_px = width;
        dpi = dpiX;
    }
    else
    {
        side_px = height;
        dpi = dpiY;
    }
    const qreal side_in = side_px / dpi;

    // Set title with percent of actual size
    const int percent = side_in / PHYSICAL_SIDE_IN * 100.0;
    const QString title =
        QString("FJ - %1\"x%1\" %2%").arg(PHYSICAL_SIDE_IN).arg(percent);
    setWindowTitle(title);
}
