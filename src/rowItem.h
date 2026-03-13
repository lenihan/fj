#pragma once

#include "constants.h"

#include <QFont>
#include <QGraphicsSimpleTextItem>

class RowItem : public QGraphicsSimpleTextItem
{
  public:
    explicit RowItem(uint8_t row, QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

  private:
    static QFont getFont();
    static qreal getFontCharHeight_fnt();
    static qreal getFontCharWidth_fnt();

    const QFont kFont;
    const qreal kFontCharHeight_fnt;
    const qreal kFontCharWidth_fnt;

    const qreal kCharsPerRow;
    const qreal kRowHeight_scn;

    uint8_t m_row;
    QString m_text;
};