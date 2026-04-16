#include "topicItem.h"
#include <QPen>

TopicItem::TopicItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : CardItem(cardNum, year, parent)
{
}

void TopicItem::setupVisuals()
{
    setupBackground();
    setupLines();
}
