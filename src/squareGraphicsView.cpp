#include "squareGraphicsView.h"

#include <QApplication>
#include <QGraphicsRectItem>
#include <QScreen>

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(parent), m_scene(scene)
{
    Q_ASSERT(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setAlignment(Qt::AlignCenter);

    // Want view to always show, no scroll bars needed
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto blueRect = new QGraphicsRectItem();
    QPen bluePen = QPen(Qt::blue);
    blueRect->setPen(bluePen);
    blueRect->setBrush(Qt::blue);
    blueRect->setRect(10, 10, 10, 10);
    m_scene->addItem(blueRect);

    m_text = new QGraphicsTextItem("FJ: Keep your hands on the keyboard!\n0O 1l 5S");
    QFont font("Hack", 12);
    m_text->setFont(font);

    m_scene->addItem(m_text);
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
#if 1
    QScreen *screen = QApplication::primaryScreen();
    Q_ASSERT(screen);
    const qreal dpiX = screen->physicalDotsPerInchX();
    const qreal dpiY = screen->physicalDotsPerInchY();
    
    // qreal dpiX = 1.0;
    // qreal dpiY = 2.0;
    resetTransform();
    scale(dpiX/dpiX, dpiY/dpiX);
    // TODO: Need to update items in scene for new scene settings

    // Get the view's size
    int width = viewport()->width();
    int height = viewport()->height();

    // Use the smaller dimension to keep the scene square
    int side = qMin(width, height);

    // Set the scene's rectangle to be square and centered
    qreal offsetX = (width - side) / 2.0;
    qreal offsetY = (height - side) / 2.0;
    m_scene->setSceneRect(offsetX, offsetY, side, side);
#else
    {
        auto vw = viewport()->width();
        auto vh = viewport()->height();
        auto w = width();
        auto h = height();
        // int i;
        // i++;
    }
    QScreen *screen = QApplication::primaryScreen();
    Q_ASSERT(screen);
    const qreal dpiX = screen->physicalDotsPerInchX();
    const qreal dpiY = screen->physicalDotsPerInchY();
    scale(dpiX/dpiX, dpiY/dpiX);

    // Get the view's size
    const int width_px = viewport()->width();
    const int height_px = viewport()->height();
 
    // Convert size to inches
    const qreal width_in = width_px / dpiX;
    const qreal height_in = height_px / dpiY;
    
    // Use the smaller dimension to keep the scene square
    const qreal smallerSide_in = width_in <= height_in ? width_in : height_in;
    
    // Set the scene's rectangle to be square and centered
    qreal offsetX_in = (width_in - smallerSide_in) / 2.0;
    qreal offsetY_in = (height_in - smallerSide_in) / 2.0;
    
    const int offsetX_px = offsetX_in * dpiX;
    const int offsetY_px = offsetY_in * dpiY;
    const int sceneWidth_px = smallerSide_in * dpiX;
    const int sceneHeight_px = smallerSide_in * dpiY;
    
    m_scene->setSceneRect(offsetX_px, offsetY_px, sceneWidth_px, sceneHeight_px);
    m_sceneBorder->setRect(m_scene->sceneRect());

    // Scale view based on dpi\

    // scale(2.0, 1.0);
    
    // Set title
    QWidget* mainWindow = window();
    Q_ASSERT(mainWindow);
    const qreal desired_in = 8.0;
    const int percent = smallerSide_in / desired_in * 100.0;
    const QString title = QString("FJ - %1\"x%1\" %2%").arg(desired_in).arg(percent);
    mainWindow->setWindowTitle(title);
#endif
    QGraphicsView::resizeEvent(event);
}

void SquareGraphicsView::paintEvent(QPaintEvent* event)
{
    // Call the base class implementation to draw the view's content
    QGraphicsView::paintEvent(event);

    QPainter painter(viewport());
    
    // Draw a red border around the viewport
    {
        painter.setPen(Qt::red);
        painter.drawRect(viewport()->rect());
    }

    // Draw a green border around the scene
    {
        painter.setPen(Qt::green);
        Q_ASSERT(m_scene);
        painter.drawRect(m_scene->sceneRect());
    }
}