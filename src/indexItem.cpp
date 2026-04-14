#include "indexItem.h"
#include "common.h"
#include "cardStack.h"

IndexItem::IndexItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : CardItem(cardNum, year, parent)
{
    m_isIndex = true;
    m_entries.reserve(Card::kNumUserBodyRows);
}

void IndexItem::addToIndex(CardItem* card)
{
    Q_ASSERT(card);
    Q_ASSERT(m_entries.size() < Card::kNumUserBodyRows);
    m_entries.push_back(card);
}
