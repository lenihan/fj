#include "cardItem.h"
#include "rowItem.h"

#include <QPen>

CardItem::CardItem(QGraphicsItem* parent) : QGraphicsRectItem(parent)
{
    // Card background
    setRect(Card::kRect_scn);
    setPen(Qt::NoPen);
    setBrush(QBrush(Card::kColor));

    // Row lines
    for (int i = 0; i < Card::kNumRows - 1; ++i)
    {
        auto* line = new QGraphicsLineItem(this);
        const qreal y =
            Card::kTop_scn + Title::kRowHeight_scn + (i * Body::kRowHeight_scn);
        line->setLine(Card::kLeft_scn, y, Card::kRight_scn, y);

        QPen pen(i == 0 ? Title::kLineColor : Body::kLineColor);
        pen.setWidthF(3.0);
        pen.setCosmetic(true);
        line->setPen(pen);
    }

    // Reserve space for rows
    m_rows.reserve(Card::kNumRows);
    for (int i = 0; i < Card::kNumRows; ++i)
    {
        m_rows.emplace_back(new RowItem(static_cast<uint8_t>(i), this));
    }
}

void CardItem::setText(QStringList text)
{
    for (int i = 0; i < Card::kNumRows; ++i)
    {
        m_rows[i]->setText(text[i]);
    }
}
