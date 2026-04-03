#pragma once

#include "constants.h"

#include <QGraphicsRectItem>

class RowItem;

class CardItem : public QGraphicsRectItem
{
  public:
    CardItem(uint16_t page, QGraphicsItem* parent = nullptr);
    void setChar(const QChar c, const uint8_t row, const uint8_t col);
    void setText(const uint8_t row, const QString& text);
    uint8_t userRowsPerCard() const;
    uint8_t colPerRow(uint8_t row) const;
    qreal rowLineY_scn(uint8_t row) const;
    const RowItem* rowItem(uint8_t row) const;

  private:
    struct ThreadRef
    {
        uint16_t m_year;    // 0000–9999
        uint16_t m_cardNum; // 0–9999
    };
    ThreadRef m_threadPrev;
    ThreadRef m_threadNext;
    QList<RowItem*> m_rows;
    uint16_t m_cardNum;
    bool m_threadStart;
    bool m_deleted;
    bool m_readOnly;
};