// tocItem.h -- a card that lists other cards ("content") instead of
// holding freeform text in its body rows.
//
// Content-row text (rows 1..lastUserRow: "Title .... ->5") is computed on
// demand in text() rather than cached. The old Qt version materialized it
// once via setupRowAt() and re-materialized all of it whenever the TOC
// became visible again (QGraphicsItem::itemChange), to pick up title
// edits made to a referenced card elsewhere. With show()/hide() gone (see
// PLAN_addendum.md), there's no "about to be shown" moment left to hook a
// resync into, so text() just recomputes from m_content every call --
// simpler, and can't go stale. Not a meaningful cost at 11 rows max.

#pragma once

#include "cardItem.h"

#include <vector>

class TOCItem : public CardItem
{
  public:
    TOCItem(CardNumber cardNumber, Year year);

    Type cardType() const override { return Type::TOC; }
    std::u32string text(Row row) const override;

    void addToTOC(CardItem* card);
    RowCount numberContent() const;
    CardItem* cardAtRow(Row row);
    Row rowAtCard(CardItem* card) const;
    bool isEmpty() const;
    bool isFull() const;
    void setupLinks() override;

  private:
    std::u32string rowDisplayText(Row row) const;

    std::vector<CardItem*> m_content; // non-owning; CardStack owns via unique_ptr
};
