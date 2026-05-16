#include "cardStack.h"
#include "contentItem.h"
#include "cursor.h"
#include "rowItem.h"
#include "tocItem.h"

#include <QGraphicsScene>

CardStack::CardStack(Year year, QGraphicsScene* scene) : m_year(year), m_scene(scene)
{
    Q_ASSERT(m_scene);
    
    // Create cardstack's TOC
    CardItem* newCard = add(CardItem::Type::TOC, ThreadMode::New);
    
    newCard->setReadOnly(true);
    
    // Title
    QString title = m_year == Master::kYear ? "Master TOC" : QString::number(m_year) = " TOC";
    newCard->firstRowItem()->setText(title);
    newCard->firstRowItem()->setReadOnly(true);
}

CardItem* CardStack::cardItemAt(CardNumber cardNumber)
{
    Q_ASSERT(cardNumber <= lastCardNumber());
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

CardNumber CardStack::lastCardNumber() const
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

CardItem* CardStack::add(CardItem::Type type, ThreadMode threadMode, CardItem* currentCard)
{
    // Create
    CardNumber newCardNumber = lastCardNumber() + 1;
    CardItem* newCard = nullptr;
    if (type == CardItem::Type::Content)
        newCard = new ContentItem(newCardNumber, m_year);
    else if (type == CardItem::Type::TOC)
        newCard = new TOCItem(newCardNumber, m_year);
    
    // Add to scene
    m_scene->addItem(newCard);
    
    // Add to card stack
    m_cards.append(newCard);

    // Connections
    if (threadMode == ThreadMode::New)
    {
        // newCard
        newCard->setThreadStart(newCard);
        newCard->setThreadPrev(currentCard);
        
        // TOC
        if (currentCard)
        {
            auto* toc = dynamic_cast<TOCItem*>(currentCard);
            Q_ASSERT(toc);
            toc->addToTOC(newCard);
        }
    }
    else if (threadMode == ThreadMode::Continue)
    {
        // newCard
        newCard->setThreadStart(currentCard->threadStart());
        newCard->setThreadPrev(currentCard);
        QString title = currentCard->firstRowItem()->text();
        newCard->firstRowItem()->setText(title);
        newCard->firstRowItem()->setReadOnly(true);
        
        // currentCard
        currentCard->setThreadNext(newCard);
    }
    return newCard;
}
