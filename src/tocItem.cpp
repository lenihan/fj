#include "tocItem.h"

TOCItem::TOCItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : CardItem(cardNum, year, parent)
{
    m_topics.reserve(Card::kNumUserBodyRows);
}

void TOCItem::addToTOC(CardItem* card)
{
    Q_ASSERT(card);
    Q_ASSERT(m_topics.size() < Card::kNumUserBodyRows);
    m_topics.push_back(card);

    // TODO: Update rows
}

void TOCItem::setupVisuals()
{
    setupBackground();
}

