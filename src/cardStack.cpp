#include "cardStack.h"
#include "tocItem.h"
#include "cardItem.h"
#include "rowItem.h"

#include <QGraphicsScene>

CardStack::CardStack(Year year, QGraphicsScene* scene) : m_year(year), m_scene(scene)
{
    Q_ASSERT(m_scene);
    TOCItem* toc = new TOCItem(0, m_year);
    m_cards.append(toc);
    toc->setThreadStart(toc);
    toc->setThreadPrev(toc);
}

CardItem* CardStack::card(CardNum cardNum)
{
    Q_ASSERT(cardNum <= lastCardNum());
    return m_cards[cardNum];
}

TOCItem* CardStack::toc()
{
    CardItem* first = m_cards.at(0);
    Q_ASSERT(first->isIndex());
    TOCItem* toc = dynamic_cast<TOCItem*>(first);
    Q_ASSERT(toc);
    return toc;
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
        newCard = new TOCItem(nextCardNum, m_year);
    else
        newCard = new CardItem(nextCardNum, m_year);
    m_cards.append(newCard);

    newCard->setThreadStart(newCard);
    
    auto* toc = dynamic_cast<TOCItem*>(currentCard->toc());
    newCard->setThreadPrev(toc);
    toc->addToTOC(newCard);

    m_scene->addItem(newCard);
}
