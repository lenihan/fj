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
    CardItem* cardItemAt(CardNumber cardNumber);
    TOCItem* tableOfContents();
    CardItem* lastCardItem();
    CardNumber lastCard() const;
    
    void setReadOnly(bool readOnly);
    bool readOnly() const;    
    
    void addNewContent(CardItem* currentCard);
    void addNewTOC(CardItem* currentCard);


  private:
    void addNewCard(CardItem* currentCard, CardItem* newCard);
    Year m_year;
    CardList m_cards;
    QGraphicsScene* m_scene;
    bool m_readOnly{false};
};