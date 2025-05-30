#ifndef ANIMATEDGRAPHICSITEM_H
#define ANIMATEDGRAPHICSITEM_H

#include <QGraphicsObject>
#include <QPainter>

class AnimatedGraphicsItem : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos NOTIFY posChanged)

public:
    enum class ShapeType { Rectangle, Circle };

    AnimatedGraphicsItem(ShapeType shape, const QRectF &rect, const QBrush &brush, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    void posChanged();

private:
    ShapeType shapeType;
    QRectF rect;
    QBrush brush;
};

#endif // ANIMATEDGRAPHICSITEM_H