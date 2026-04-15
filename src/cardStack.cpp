#include "cardStack.h"
#include "indexItem.h"
#include "cardItem.h"
#include "rowItem.h"

#include <QGraphicsScene>

CardStack::CardStack(Year year, QGraphicsScene* scene) : m_year(year), m_scene(scene)
{
    Q_ASSERT(m_scene);
    IndexItem* index = new IndexItem(0, m_year);
    m_cards.append(index);
    index->setThreadStart(index);
    index->setThreadPrev(index);
}

CardItem* CardStack::card(CardNum cardNum)
{
    Q_ASSERT(cardNum <= lastCardNum());
    return m_cards[cardNum];
}

IndexItem* CardStack::index()
{
    CardItem* first = m_cards.at(0);
    Q_ASSERT(first->isIndex());
    IndexItem* index = dynamic_cast<IndexItem*>(first);
    Q_ASSERT(index);
    return index;
}

CardItem* CardStack::lastCard()
{
    return m_cards.last();
}

CardNum CardStack::lastCardNum() const
{
    return m_cards.size() - 1;
}

void CardStack::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
}

bool CardStack::readOnly() const
{
    return m_readOnly;
}

void CardStack::addCollection(CardItem* currentCard)
{
    bool addIndex = false;
    add(currentCard, addIndex);
}

void CardStack::addEntry(CardItem* currentCard)
{
    bool addIndex = true;
    add(currentCard, addIndex);
}

void CardStack::add(CardItem* card)
{
    m_cards.append(card);
}

void CardStack::add(CardItem* currentCard, bool addIndex)
{
    Q_ASSERT(currentCard);
    const CardNum nextCardNum = lastCardNum() + 1;
    CardItem* newCard = nullptr;
    if (addIndex)
        newCard = new IndexItem(nextCardNum, m_year);
    else
        newCard = new CardItem(nextCardNum, m_year);
    m_cards.append(newCard);

    newCard->setThreadStart(newCard);
    
    auto* index = dynamic_cast<IndexItem*>(currentCard->index());
    newCard->setThreadPrev(index);
    index->addToIndex(newCard);

    m_scene->addItem(newCard);
}
