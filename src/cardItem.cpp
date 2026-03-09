#include "CardItem.h"
#include <QPen>

CardItem::CardItem(QGraphicsItem* parent)
    : QGraphicsRectItem(parent),
      kCardTopLeftPt_scn(kCardLeft_scn, kCardTop_scn),
      kCardBottomRightPt_scn(kCardRight_scn, kCardBottom_scn),
      kCardRect_scn(kCardTopLeftPt_scn, kCardBottomRightPt_scn)
{
    // Card background
    setRect(kCardRect_scn);
    setPen(Qt::NoPen);
    setBrush(QBrush(kCardColor));

    // Title line
    auto* titleLine = new QGraphicsLineItem(this);
    {
        const qreal y = kCardTop_scn + kTitleRowHeight_scn;
        titleLine->setLine(kCardLeft_scn, y, kCardRight_scn, y);

        QPen pen(kTitleLineColor);
        pen.setWidthF(3.0);
        pen.setCosmetic(true);
        titleLine->setPen(pen);
    }

    // Body lines
    for (int i = 0; i < 9; ++i)
    {
        // Body line
        auto* bodyLine = new QGraphicsLineItem(this);
        {
            // TODO: Combine title/body with single setLine, single y calc
            const qreal y = (kBodyRowHeight_scn + kTitleRowHeight_scn) +
                            (i * kBodyRowHeight_scn);
            bodyLine->setLine(kCardLeft_scn, y, kCardRight_scn, y);

            QPen pen(kBodyLineColor);
            pen.setWidthF(3.0);
            pen.setCosmetic(true);
            bodyLine->setPen(pen);
        }
    }
}