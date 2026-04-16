#include "tocItem.h"

TOCItem::TOCItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : CardItem(cardNum, year, parent)
{
    m_content.reserve(Card::kNumUserBodyRows);
    setupBackground();
}

void TOCItem::addToTOC(CardItem* card)
{
    Q_ASSERT(card);
    Q_ASSERT(m_content.size() < Card::kNumUserBodyRows);
    m_content.push_back(card);

    // TODO: Update rows
}
