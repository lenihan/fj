#include "cardStack.h"
#include "contentItem.h"
#include "rowItem.h"
#include "tocItem.h"

#include <QGraphicsScene>

CardStack::CardStack(Year year, QGraphicsScene* scene) : m_year(year), m_scene(scene)
{
    Q_ASSERT(m_scene);
    TOCItem* toc = new TOCItem(0, m_year);
    QString title = m_year == Master::kYear ? "Master TOC" : QString::number(m_year) = " TOC";
    toc->setText(0, title);
    toc->setReadOnly(true);
    if (m_year == Master::kYear)
        toc->firstRowItem()->setReadOnly(true);
    m_cards.append(toc);
    toc->setThreadStart(toc);
    toc->setThreadPrev(nullptr);
    m_scene->addItem(toc);
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
    Q_ASSERT(currentCard);

    // Create
    CardNumber newCardNumber = lastCardNumber() + 1;
    CardItem* newCard = nullptr;
    if (type == CardItem::Type::Content)
        newCard = new ContentItem(newCardNumber, m_year);
    else if (type == CardItem::Type::TOC)
        newCard = new TOCItem(newCardNumber, m_year);
    
    // Add to card stack
    m_cards.append(newCard);

    // Thread
    newCard->setThreadStart(newCard);

    // Update TOC connections
    auto* toc = dynamic_cast<TOCItem*>(currentCard->tableOfContents());
    Q_ASSERT(toc);
    newCard->setThreadPrev(toc);
    toc->addToTOC(newCard);

    // Show
    m_scene->addItem(newCard);
    currentCard->hide();
    currentCard = newCard;
    newCard->show();

    return newCard;
}
