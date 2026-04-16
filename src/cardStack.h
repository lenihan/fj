#pragma once

#include "common.h"
#include <cstdint>
#include <QList>
#include <QMap>

class CardItem;
class TOCItem;
class QGraphicsScene;

using CardList = QList<CardItem*>;

class CardStack
{
  public:
    CardStack() = default;
    CardStack(Year year, QGraphicsScene* scene);
    CardItem* card(CardNum cardNum);
    TOCItem* toc();
    CardItem* lastCard();
    CardNum lastCardNum() const;
    void setReadOnly(bool readOnly);
    bool readOnly() const;    void add(CardItem *card);
    void addCollection(CardItem* currentCard);
    void addEntry(CardItem* currentCard);


  private:
    void add(CardItem* currentCard, bool addIndex);
    Year m_year;
    CardList m_cards;
    QGraphicsScene* m_scene;
    bool m_readOnly{false};
};