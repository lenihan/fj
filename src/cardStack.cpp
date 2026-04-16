#include "cardStack.h"
#include "tocItem.h"
#include "topicItem.h"
#include "rowItem.h"

#include <QGraphicsScene>

CardStack::CardStack(Year year, QGraphicsScene* scene) : m_year(year), m_scene(scene)
{
    Q_ASSERT(m_scene);
    TOCItem* toc = new TOCItem(0, m_year);
    QString title = m_year == Master::kYear ? "Master TOC" : QString::number(m_year) = " TOC";
    toc->setText(0, title);
    toc->setReadOnly(true);
    m_cards.append(toc);
    toc->setThreadStart(toc);
    toc->setThreadPrev(nullptr);
}

CardItem* CardStack::card(CardNum cardNum)
{
    Q_ASSERT(cardNum <= lastCardNum());
    return m_cards[cardNum];
}

TOCItem* CardStack::toc()
{
    CardItem* first = m_cards.at(0);
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

void CardStack::addTopic(CardItem* currentCard)
{
    bool addTOC = false;
    add(currentCard, addTOC);
}

void CardStack::addTOC(CardItem* currentCard)
{
    bool addTOC = true;
    add(currentCard, addTOC);
}

void CardStack::add(CardItem* card)
{
    m_cards.append(card);
}

void CardStack::add(CardItem* currentCard, bool addTOC)
{
    Q_ASSERT(currentCard);
    const CardNum nextCardNum = lastCardNum() + 1;
    CardItem* newCard = nullptr;
    if (addTOC)
        newCard = new TOCItem(nextCardNum, m_year);
    else
        newCard = new TopicItem(nextCardNum, m_year);
    m_cards.append(newCard);

    newCard->setThreadStart(newCard);
    
    auto* toc = dynamic_cast<TOCItem*>(currentCard->toc());
    newCard->setThreadPrev(toc);
    toc->addToTOC(newCard);

    m_scene->addItem(newCard);
}
