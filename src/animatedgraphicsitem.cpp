#include "animatedgraphicsitem.h"

AnimatedGraphicsItem::AnimatedGraphicsItem(ShapeType shape, const QRectF &rect, const QBrush &brush, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , shapeType(shape)
    , rect(rect)
    , brush(brush)
{
}

QRectF AnimatedGraphicsItem::boundingRect() const
{
    return rect;
}

void AnimatedGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setBrush(brush);
    painter->setPen(Qt::NoPen);
    if (shapeType == ShapeType::Rectangle)
    {
        painter->drawRect(rect);
    }
    else
    {
        painter->drawEllipse(rect);
    }
}