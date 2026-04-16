#pragma once

#include "cardItem.h"
#include "common.h"

class TopicItem : public CardItem
{
  public:
    TopicItem(CardNum cardNum, Year year, QGraphicsItem* parent = nullptr);

  private:
    void setupVisuals() override;
};