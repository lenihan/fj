#pragma once

#include "constants.h"
#include <QFont>
#include <QGraphicsSimpleTextItem>

class RowItem : public QGraphicsSimpleTextItem
{
  public:
    explicit RowItem(uint8_t row, QGraphicsItem* parent = nullptr);
    uint8_t colPerRow() const;
    void setText(const QString& text); // Not virtual, hides base class setText

  private:
    static QFont getFont();
    static qreal getFontCharHeight_fnt();
    static qreal getFontCharWidth_fnt();

    const QFont kFont;
    const qreal kFontCharHeight_fnt;
    const qreal kFontCharWidth_fnt;

    const uint8_t kCharsPerRow;
    const qreal kRowHeight_scn;

    uint8_t m_row;
};