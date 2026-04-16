#include "contentItem.h"
#include <QPen>

ContentItem::ContentItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : CardItem(cardNum, year, parent)
{
}

void ContentItem::setupVisuals()
{
    setupBackground();
    setupLines();
}
