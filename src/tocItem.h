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
    RowCount numberContent() const;
    CardItem* cardAtRow(Row row);
    bool empty() const;

  private:
    void setupRowAt(Row row);
    QString rtrim(const QString& str) const;
    std::vector<CardItem*> m_content;
};