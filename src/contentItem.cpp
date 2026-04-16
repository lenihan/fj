#include "contentItem.h"
#include <QPen>

ContentItem::ContentItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : CardItem(cardNum, year, parent)
{
    setupBackground();
    setupLines();
}
