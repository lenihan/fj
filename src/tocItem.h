#pragma once

#include "cardItem.h"
#include "common.h"
#include <vector>

class QGraphicsItem;

class TOCItem : public CardItem
{
  public:
    TOCItem(CardNumber cardNumber, Year year, QGraphicsItem* parent = nullptr);
    Type cardType() const override { return Type::TOC; }
    void addToTOC(CardItem* card);
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value);

  private:
    void setupRowAt(Row row);
    std::vector<CardItem*> m_cards;
};