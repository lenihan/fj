#include "squareGraphicsView.h"

#include <QGraphicsRectItem>

SquareGraphicsView::SquareGraphicsView(QWidget* parent) : QGraphicsView(parent)
{
    // Create a scene
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setAlignment(Qt::AlignCenter);

    // Optional: Disable scrollbars
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Add a QGraphicsRectItem to outline the scene rectangle
    m_sceneBorder = new QGraphicsRectItem(m_scene->sceneRect());
    QPen pen(Qt::green, 2); // Green pen with 2-pixel width
    m_sceneBorder->setPen(pen);
    m_sceneBorder->setBrush(Qt::NoBrush); // No fill, just the outline
    m_scene->addItem(m_sceneBorder);
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    // Get the view's size
    int width = viewport()->width();
    int height = viewport()->height();

    // Use the smaller dimension to keep the scene square
    int side = qMin(width, height);

    // Set the scene's rectangle to be square and centered
    qreal offsetX = (width - side) / 2.0;
    qreal offsetY = (height - side) / 2.0;
    m_scene->setSceneRect(offsetX, offsetY, side, side);
    m_sceneBorder->setRect(m_scene->sceneRect());

    // Fit the view to the scene's rectangle
    fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);

    QGraphicsView::resizeEvent(event);
}

void SquareGraphicsView::paintEvent(QPaintEvent* event)
{
    // Call the base class implementation to draw the view's content
    QGraphicsView::paintEvent(event);

    // Draw a red border around the viewport
    QPainter painter(viewport());
    QPen pen(Qt::red, 1); // Red pen with 2-pixel width
    painter.setPen(pen);
    painter.drawRect(viewport()->rect().adjusted(
        1, 1, -1, -1)); // Slightly inset to avoid clipping
}