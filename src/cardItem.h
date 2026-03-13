#pragma once

#include "constants.h"

#include <QGraphicsRectItem>

class RowItem;

class CardItem : public QGraphicsRectItem
{
  public:
    CardItem(QGraphicsItem* parent = nullptr);
    void setText(QStringList text);

  private:
    QList<RowItem*> m_rows;
};