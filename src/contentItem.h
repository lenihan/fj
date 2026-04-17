#pragma once

#include "cardItem.h"
#include "common.h"

class ContentItem : public CardItem
{
  public:
    ContentItem(CardNumber cardNumber, Year year, QGraphicsItem* parent = nullptr);
    Type cardType() const override { return Type::Content; }
};