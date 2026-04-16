#pragma once

#include "cardItem.h"
#include "common.h"
#include <vector>

class QGraphicsItem;

class TOCItem : public CardItem
{
  public:
    TOCItem(CardNum cardNum, Year year, QGraphicsItem* parent = nullptr);
    Type cardType() const override { return Type::TOC; }
    void addToTOC(CardItem* card);

  private:
    void setupVisuals() override;

  private:
    std::vector<CardItem*> m_content;
};