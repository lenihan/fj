#pragma once

#include "constants.h"

#include <QGraphicsRectItem>

class RowItem;

class CardItem : public QGraphicsRectItem
{
  public:
    CardItem(QGraphicsItem* parent = nullptr);
    void setChar(const QChar ch, const uint8_t row, const uint8_t col);
    void setText(QStringList text);

  private:
    QList<RowItem*> m_rows;
};