#pragma once

#include "constants.h"
#include <QFont>
#include <QGraphicsSimpleTextItem>
#include <QPointF>

class RowItem : public QGraphicsSimpleTextItem
{
  public:
    RowItem(uint8_t row, QGraphicsItem* parent = nullptr);
    virtual void paint(QPainter* painter,
                       const QStyleOptionGraphicsItem* option,
                       QWidget* widget) override;

  private:
    static QFont getFont();
    static qreal getCharWidth();
    static qreal getCharHeight();

    const QFont kFont;
    const qreal kCharsPerRow;
    const qreal kRowHeight_scn;
    const qreal kCharWidth_fnt;
    const qreal kCharHeight_fnt;

    // static constexpr qreal Title:kCharsPerRow = 30.0;
    // static constexpr qreal Body::kCharsPerRow = 60.0;
    // static constexpr qreal Title::kRowHeight_scn = 0.5;
    // static constexpr qreal Body::kRowHeight_scn = 0.25;
    // static constexpr qreal kCardLeft_scn = 0.0;
    // static constexpr qreal kCardRight_scn = 5.0;
    // static constexpr qreal kCardTop_scn = 0.0;
    // static constexpr qreal kCardBottom_scn = 3.0;
    // static constexpr qreal kCardBorder_scn = 0.1;

    const QPointF kCardTopLeftPt_scn;
    const QPointF kCardBottomRightPt_scn;
    const QRectF kCardRect_scn;
    const qreal kUseableCardWidth_scn =
        kCardRect_scn.width() - (2 * Card::kBorder_scn);

    uint8_t m_row;
};