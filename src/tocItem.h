#pragma once

#include "cardItem.h"
#include "common.h"
#include <vector>

class QGraphicsItem;

class TOCItem : public CardItem
{
  public:
    TOCItem(CardNum cardNum, Year year, QGraphicsItem* parent = nullptr);
    void addToTOC(CardItem* card);

  protected:
    void drawLines() override;

  private:
    std::vector<CardItem*> m_entries;
};