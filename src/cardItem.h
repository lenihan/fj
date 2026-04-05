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
    void setThreadPrev(CardItem *card);
    CardItem* threadPrev();
    void setThreadNext(CardItem *card);
    CardItem* threadNext();
    uint8_t firstEditableRow() const;
    uint8_t lastEditableRow() const;
    uint8_t lastCol(uint8_t row) const;
    uint8_t firstCol(uint8_t row) const;

  private:
    CardItem* m_threadPrev{nullptr};
    CardItem* m_threadNext{nullptr};
    QList<RowItem*> m_rows;
    uint16_t m_cardNum{0};
    uint16_t m_year{0};
    bool m_threadStart{false};
    bool m_deleted{false};
    bool m_readOnly{false};
};