#pragma once

#include "constants.h"

#include <QGraphicsRectItem>

class RowItem;

class CardItem : public QGraphicsRectItem
{
  public:
    CardItem(uint16_t page, QGraphicsItem* parent = nullptr);
    void setChar(const QChar ch, const uint8_t row, const uint8_t col);
    QString text(const uint8_t row) const;
    void setText(const uint8_t row, const QString& text);
    void setText(QStringList text);
    uint8_t userRowsPerCard() const;
    uint8_t colPerRow(uint8_t row) const;

  private:
    QList<RowItem*> m_rows;
    uint16_t m_page;
    uint8_t m_currentRow;
    uint8_t m_currentCol;
};