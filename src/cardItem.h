#pragma once

#include "constants.h"

#include <QGraphicsRectItem>

class CardItem : public QGraphicsRectItem
{
  public:
    CardItem(QGraphicsItem* parent = nullptr);
};