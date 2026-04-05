#pragma once

#include "constants.h"

#include <QGraphicsRectItem>

class RowItem;

class CardItem : public QGraphicsRectItem
{
  public:
    CardItem(uint16_t cardNum, uint16_t year, QGraphicsItem* parent = nullptr);
    void setChar(const QChar c, const uint8_t row, const uint8_t col);
    void setText(const uint8_t row, const QString& text);
    uint8_t colPerRow(uint8_t row) const;
    qreal rowLineY_scn(uint8_t row) const;
    const RowItem* rowItem(uint8_t row) const;
    RowItem* firstRow();
    RowItem* lastRow();
    void setThreadStart(bool threadStart);
    bool threadStart() const;
    uint16_t cardNum() const;
    uint16_t year() const;
    void setThreadPrev(uint16_t cardNum, uint16_t year = 0);
    void setThreadNext(uint16_t cardNum, uint16_t year = 0);
    uint8_t firstEditableRow() const;
    uint8_t lastEditableRow() const;
    uint8_t lastCol(uint8_t row) const;
    uint8_t firstCol(uint8_t row) const;

  private:
    struct ThreadRef
    {
        uint16_t m_year{0};    // 0000–9999
        uint16_t m_cardNum{0}; // 0–9999
    };
    ThreadRef m_threadPrev;
    ThreadRef m_threadNext;
    QList<RowItem*> m_rows;
    uint16_t m_cardNum{0};
    uint16_t m_year{0};
    bool m_threadStart{false};
    bool m_deleted{false};
    bool m_readOnly{false};
};