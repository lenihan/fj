#pragma once

#include "constants.h"

#include <QFont>
#include <QGraphicsSimpleTextItem>

class RowItem : public QGraphicsSimpleTextItem
{
  public:
    RowItem(uint8_t row, QGraphicsItem* parent = nullptr);
    virtual void paint(QPainter* painter,
                       const QStyleOptionGraphicsItem* option,
                       QWidget* widget) override;

  private:
    const QFont kFont;
    const qreal kFontCharHeight_fnt;
    const qreal kFontCharWidth_fnt;
    static QFont getFont();
    static qreal getFontCharHeight_fnt();
    static qreal getFontCharWidth_fnt();

    const qreal kCharsPerRow;
    const qreal kRowHeight_scn;

    uint8_t m_row;
};