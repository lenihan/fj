#include "tocItem.h"

TOCItem::TOCItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : CardItem(cardNum, year, parent)
{
    m_isTOC = true;
    m_entries.reserve(Card::kNumUserBodyRows);
}

void TOCItem::addToTOC(CardItem* card)
{
    Q_ASSERT(card);
    Q_ASSERT(m_entries.size() < Card::kNumUserBodyRows);
    m_entries.push_back(card);

    // TODO: Update rows
}

void TOCItem::drawLines()
{
    ; // TOC is a blank card (no lines)
}
