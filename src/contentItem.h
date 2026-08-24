// contentItem.h -- a card holding freeform user text in its body rows.
//
// Trivial: unlike the old Qt version, there's no setupBackground()/
// setupLines() to call here anymore -- background fill and separator
// lines aren't per-instance item state, they're draw calls computed fresh
// from constants each frame by whatever draws a CardItem (see cursor.h).

#pragma once

#include "cardItem.h"

class ContentItem : public CardItem
{
  public:
    ContentItem(CardNumber cardNumber, Year year);

    Type cardType() const override { return Type::Content; }
};
