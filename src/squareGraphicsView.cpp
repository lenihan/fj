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
    blueRect->setRect(100, 100, 100, 100);
    m_scene->addItem(blueRect);

    m_text =
        new QGraphicsTextItem("FJ: Keep your hands on the keyboard!\n0O 1l 5S");
    QFont font("Hack", 12);
    m_text->setFont(font);

    m_scene->addItem(m_text);
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    QScreen* screen = QApplication::primaryScreen();
    Q_ASSERT(screen);
    const qreal dpiX = screen->physicalDotsPerInchX();
    const qreal dpiY = screen->physicalDotsPerInchY();

    // qreal dpiY = 2.0;
    // qreal dpiX = 1.0;
    resetTransform();
    scale(dpiX / dpiX, dpiY / dpiX);

    // Get the view's size
    const int width_px = viewport()->width();
    const int height_px = viewport()->height();

    // Use the smaller dimension to keep the scene square
    const int side_px = width_px <= height_px ? width_px : height_px;
    const qreal dpi = width_px <= height_px ? dpiX : dpiY;

    // Set the scene's rectangle to be square and centered
    const qreal offsetX_px = (width_px - side_px) / 2.0;
    const qreal offsetY_px = (height_px - side_px) / 2.0;
    m_scene->setSceneRect(offsetX_px, offsetY_px, side_px, side_px);

    // TODO: Need to update items in scene for new scene settings
    // Translate all items by the negative offset (-dx, -dy)
    {
        const QRectF sceneRect = m_scene->sceneRect();
        for (QGraphicsItem* item : m_scene->items())
        {
            item->setPos(sceneRect.topLeft());
        }
    }

    // Set title
    {
        QWidget* mainWindow = window();
        Q_ASSERT(mainWindow);
        const qreal desired_in = 8.0;
        const qreal side_in = side_px / dpi;
        const int percent = side_in / desired_in * 100.0;
        const QString title =
            QString("FJ - %1\"x%1\" %2%").arg(desired_in).arg(percent);
        mainWindow->setWindowTitle(title);
    }
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