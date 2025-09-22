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
    const QBrush blackBrush(Qt::black);
    m_scene->setBackgroundBrush(blackBrush);
    resize(1000, 1000);

    m_view = new SquareGraphicsView(m_scene, this);
    m_view->setSceneRect(x, y, w, h);
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    m_view->show();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    // QRect rect_px = m_view->viewport()->rect();
    // const QPoint sceneCenter_px = m_view->viewport()->rect().center();

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
    
    // // Scale
    // {
    //     const qreal s = side_in / PHYSICAL_SIDE_IN;
    //     m_view->resetTransform();
    //     m_view->scale(s, s);
    // }

    // // Re-align top-left to (0, 0)
    // {
    //     // Get the current top-left scene point
    //     const QPointF currentTopLeft_scene = m_view->mapToScene(0, 0);

    //     // Calculate translation needed to move top-left to (0, 0)
    //     const QPointF delta_scene = QPointF(0, 0) - currentTopLeft_scene;

    //     // Apply translation
    //     m_view->translate(delta_scene.x(), delta_scene.y());
    // }
    
    // QRect r_px = m_view->viewport()->rect();
    // QPointF topLeft_in = m_view->mapToScene(r_px.topLeft());
    // QPointF bottomRight_in = m_view->mapToScene(r_px.bottomRight());

    // Set title with percent of actual size
    {
        const int percent = side_in / PHYSICAL_SIDE_IN * 100.0;
        const QString title =
            QString("FJ - %1\"x%1\" %2%").arg(PHYSICAL_SIDE_IN).arg(percent);
        setWindowTitle(title);
    }
}
