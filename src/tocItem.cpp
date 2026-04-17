#include "tocItem.h"
#include "rowItem.h"

TOCItem::TOCItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : CardItem(cardNum, year, parent)
{
    m_content.reserve(Card::kNumUserBodyRows);
    setupBackground();
}

void TOCItem::addToTOC(CardItem* card)
{
    Q_ASSERT(card);
    Q_ASSERT(m_content.size() <= Card::kNumUserBodyRows);
    if (m_content.size() == Card::kNumUserBodyRows)
    {
        auto* newTOC = new TOCItem(cardNum(), year());
        newTOC->addToTOC(card);
        return;
    }
    m_content.push_back(card);

    RowNum row = m_content.size();
    RowItem* rowItem = card->rowItem(row);
    ColNum cols = rowItem->colPerRow();
    QString text = "HOWDY HOWDY!";
    
    Q_ASSERT(text.length() == cols);
    rowItem->setText(text);
}
