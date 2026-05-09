#pragma once

#include "cardItem.h"
#include "common.h"
#include <QList>
#include <QMap>
#include <cstdint>

class QGraphicsScene;
class TOCItem;

using CardList = QList<CardItem*>;

class CardStack
{
  public:
    enum class ThreadMode { New, Continue };

    CardStack(Year year, QGraphicsScene *scene);
    CardItem* cardItemAt(CardNumber cardNumber);
    TOCItem* tableOfContents();
    CardItem* lastCardItem();
    CardNumber lastCardNumber() const;

    void setReadOnly(bool readOnly);
    bool readOnly() const;

    CardItem* add(CardItem::Type type, ThreadMode threadMode, CardItem* currentCard);

  private:
    Year m_year;
    CardList m_cards;
    QGraphicsScene *m_scene;
    bool m_readOnly{false};
};