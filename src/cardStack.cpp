#include "cardStack.h"
#include "tocItem.h"
#include "contentItem.h"
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
    m_scene->addItem(toc);
}

CardItem* CardStack::cardItemAt(CardNumber cardNumber)
{
    Q_ASSERT(cardNumber <= lastCard());
    return m_cards[cardNumber];
}

TOCItem* CardStack::tableOfContents()
{
    CardItem* first = m_cards.at(0);
    TOCItem* toc = dynamic_cast<TOCItem*>(first);
    Q_ASSERT(toc);
    return toc;
}

CardItem* CardStack::lastCardItem()
{
    return m_cards.last();
}

CardNumber CardStack::lastCard() const
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

void CardStack::addNewContent(CardItem* currentCard)
{
    auto* newCard = new TOCItem(lastCard() + 1, m_year);
    addNewCard(currentCard, newCard);
}

void CardStack::addNewTOC(CardItem* currentCard)
{
    auto* newCard = new ContentItem(lastCard() + 1, m_year);
    addNewCard(currentCard, newCard);
}

void CardStack::addNewCard(CardItem* currentCard, CardItem* newCard)
{
    Q_ASSERT(currentCard);
    Q_ASSERT(newCard);
    m_cards.append(newCard);
    m_scene->addItem(newCard);

    newCard->setThreadStart(newCard);
    
    auto* toc = dynamic_cast<TOCItem*>(currentCard->tableOfContents());
    Q_ASSERT(toc);
    newCard->setThreadPrev(toc);
    toc->addToTOC(newCard);
}
