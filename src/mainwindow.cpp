#include "mainwindow.h"
#include "squareGraphicsView.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QResizeEvent>

namespace
{
const qreal PHYSICAL_SIDE_IN = 8.0;
}

MainWindow::MainWindow() : QMainWindow()
{
    m_scene = new QGraphicsScene(this);
    const qreal x = 0.0;
    const qreal y = 0.0;
    const qreal w = PHYSICAL_SIDE_IN;
    const qreal h = PHYSICAL_SIDE_IN;
    m_scene->setSceneRect(x, y, w, h);
    m_scene->setBackgroundBrush(QBrush(Qt::black));
    
    m_view = new SquareGraphicsView(m_scene);
    m_view->setSceneRect(x, y, w, h);
    Q_ASSERT(m_scene->sceneRect() == m_view->sceneRect());
    
    resize(640, 640);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    const int width_px = event->size().width();
    const int height_px = event->size().height();

    // Set view geometry
    {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        if (width_px < height_px)
        {
            // Fill width
            x = 0;
            y = (height_px - width_px) / 2.0;
            w = width_px;
            h = width_px;
        }
        else
        {
            // Fill height
            x = (width_px - height_px) / 2.0;
            y = 0;
            w = height_px;
            h = height_px;
        }
        m_view->setGeometry(x, y, w, h);
    }

    // Calculate square side in inches
    qreal side_in;
    {
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
        
        // Set side in pixels, dpi based on shorter width/height
        int side_px = 0;
        qreal dpi = 0;
        if (width_px < height_px)
        {
            side_px = width_px;
            dpi = dpiX;
        }
        else
        {
            side_px = height_px;
            dpi = dpiY;
        }

        // Calculate side in inches
        side_in = side_px / dpi;
    }

    // Set title with percent of actual size
    {
        const int percent = side_in / PHYSICAL_SIDE_IN * 100.0;
        const QString title =
            QString("FJ - %1\"x%1\" %2%").arg(PHYSICAL_SIDE_IN).arg(percent);
        setWindowTitle(title);
    }
}
