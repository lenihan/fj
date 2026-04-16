#pragma once

#include "cardItem.h"
#include "common.h"

class ContentItem : public CardItem
{
  public:
    ContentItem(CardNum cardNum, Year year, QGraphicsItem* parent = nullptr);
    Type cardType() const override { return Type::Content; }

  private:
    void setupVisuals() override;
};