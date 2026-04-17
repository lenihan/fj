#include "contentItem.h"
#include <QPen>

ContentItem::ContentItem(CardNumber cardNumber, Year year, QGraphicsItem* parent)
    : CardItem(cardNumber, year, parent)
{
    setupBackground();
    setupLines();
}
