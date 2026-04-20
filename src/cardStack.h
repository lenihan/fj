#pragma once

#include "cardItem.h"
#include "common.h"
#include <QList>
#include <QMap>
#include <cstdint>

class Cursor;
class TOCItem;

using CardList = QList<CardItem*>;

class CardStack
{
  public:
    enum class ThreadMode { New, Continue };

    CardStack(Year year, Cursor* cursor);
    CardItem* cardItemAt(CardNumber cardNumber);
    TOCItem* tableOfContents();
    CardItem* lastCardItem();
    CardNumber lastCardNumber() const;

    void setReadOnly(bool readOnly);
    bool readOnly() const;

    void add(CardItem::Type type, ThreadMode threadMode);

  private:
    Year m_year;
    CardList m_cards;
    Cursor *m_cursor;
    bool m_readOnly{false};
};