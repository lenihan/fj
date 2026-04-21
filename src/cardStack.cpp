#include "cardStack.h"
#include "contentItem.h"
#include "cursor.h"
#include "rowItem.h"
#include "tocItem.h"

#include <QGraphicsScene>

CardStack::CardStack(Year year, Cursor* cursor) : m_year(year), m_cursor(cursor)
{
    Q_ASSERT(m_cursor);
    Q_ASSERT(m_cursor->scene());
    
    // Create cardstack's TOC
    TOCItem* toc = new TOCItem(0, m_year);
    toc->setReadOnly(true);
    
    // Title
    QString title = m_year == Master::kYear ? "Master TOC" : QString::number(m_year) = " TOC";
    toc->firstRowItem()->setText(title);
    toc->firstRowItem()->setReadOnly(true);
    
    // Threads
    toc->setThreadStart(toc);
    toc->setThreadPrev(nullptr);
    
    // Add to cardstack
    m_cards.append(toc);
    
    // Add to scene
    m_cursor->scene()->addItem(toc);

    // Current
    m_cursor->setYear(m_year);
    m_cursor->setRow(1);
    m_cursor->setCol(0);
    m_cursor->setCurrentCard(toc);
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

void CardStack::add(CardItem::Type type, ThreadMode threadMode)
{
    // Create
    CardNumber newCardNumber = lastCardNumber() + 1;
    CardItem* newCard = nullptr;
    if (type == CardItem::Type::Content)
        newCard = new ContentItem(newCardNumber, m_year);
    else if (type == CardItem::Type::TOC)
        newCard = new TOCItem(newCardNumber, m_year);
    
    // Add to card stack
    m_cards.append(newCard);

    // Connections
    CardItem* currentCard = m_cursor->currentCard();
    if (threadMode == ThreadMode::New)
    {
        // newCard
        newCard->setThreadStart(newCard);
        auto* toc = dynamic_cast<TOCItem*>(currentCard->tableOfContents());
        Q_ASSERT(toc);
        newCard->setThreadPrev(toc);
        m_cursor->setRow(0);
        m_cursor->setCol(0);        

        // TOC
        toc->addToTOC(newCard);
    }
    else if (threadMode == ThreadMode::Continue)
    {
        // newCard
        newCard->setThreadStart(currentCard->threadStart());
        newCard->setThreadPrev(currentCard);
        QString title = currentCard->firstRowItem()->text();
        newCard->firstRowItem()->setText(title);
        newCard->firstRowItem()->setReadOnly(true);
        m_cursor->setRow(newCard->firstUserRow());
        m_cursor->setCol(0);
        
        // currentCard
        currentCard->setThreadNext(newCard);
    }

    // Show
    m_cursor->scene()->addItem(newCard);
    currentCard->hide();
    newCard->show();

    // Current
    m_cursor->setYear(newCard->year());

    m_cursor->setCurrentCard(newCard);
}
