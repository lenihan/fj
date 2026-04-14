#pragma once

#include "cardItem.h"
#include "cardStack.h"
#include <vector>

class IndexItem : public CardItem
{
  public:
    IndexItem(CardNum cardNum, Year year, QGraphicsItem* parent = nullptr);
    void addToIndex(CardItem* card);

  private:
    std::vector<CardItem*> m_entries;
};