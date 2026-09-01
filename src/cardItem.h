// cardItem.h -- fj's card model, ported off QGraphicsRectItem.
//
// RowItem is gone: once painting and QFont metrics left, there wasn't
// enough left in it (just per-row text + a read-only flag) to justify a
// separate class, so CardItem owns that data directly (see RowData below).
//
// Row text is std::u32string, not std::string: linkStr() below builds
// strings containing the link arrows U+2191/U+2192, and every column
// calculation in this class and Cursor (lastColAt, dot-fill widths, link
// positions, ...) assumes one string element == one glyph cell. A
// std::string holding UTF-8 would count 3 bytes for each arrow and quietly
// break that arithmetic; std::u32string keeps 1 element == 1 codepoint ==
// 1 cell, and hands off to Canvas::drawText's char32_t span with no
// conversion.
//
// Ownership: CardStack owns CardItems (see cardStack.h) via
// unique_ptr<CardItem>, so the destructor must be virtual -- deleting a
// TOCItem/ContentItem through a CardItem* base pointer without one is UB.
// cardType()/setupLinks() stay virtual too: unlike PlatformWindow (only
// ever one implementation per binary), a single CardStack genuinely mixes
// TOCItem and ContentItem instances together at runtime, so this is a case
// where dynamic dispatch is actually earning its keep.

#pragma once

#include "layout.h"
#include "types.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace HackAtlas
{
struct Atlas;
}

class CardItem
{
  public:
    enum class Type
    {
        Unknown,
        TOC,
        Content
    };

    struct CardLink
    {
        Row row{0};
        Col col{0};
        ColCount charCount{0};
        CardItem* targetCard{nullptr};
    };

    CardItem(CardNumber cardNumber, Year year);
    virtual ~CardItem() = default;

    virtual Type cardType() const { return Type::Unknown; }
    bool isTOC() const;
    bool isContent() const;
    bool isThreadStart() const;
    bool isThreadEnd() const;

    void setChar(char32_t c, Row row, Col col);

    // Pads `text` with trailing spaces out to the row's full width if it's
    // shorter (matches the old RowItem::setText, which only ever
    // overwrote a prefix and left the rest as the row's initial spaces --
    // every real caller either passes a full-width string already or sets
    // a short title on a still-blank row, so padding is equivalent and
    // simpler than replicating "leave the old suffix alone" exactly).
    void setText(Row row, const std::u32string& text);

    // Virtual, returned by value (not const&): TOCItem overrides this to
    // compute content-row text on demand from the cards it lists, rather
    // than caching it -- see tocItem.h.
    virtual std::u32string text(Row row) const;

    ColCount colPerRow(Row row) const;
    CardNumber cardNumber() const;
    Year year() const;

    // Pixel geometry for row `row`, given whichever baked atlas is active
    // for this frame (picked by Canvas::pickAtlas against the current
    // window size -- see PLAN.md's "Coordinate system (core)"). This is
    // layout math, not a statement about which atlas's glyph bitmaps
    // actually get drawn -- Title rows are laid out as exactly 2x a body
    // row's cell size here regardless of which atlas Cursor::draw's
    // drawCard actually renders their glyphs from (see cursor.h's draw
    // comment and PLAN.md's Font atlas section for why Title uses its
    // own separately-picked atlas rather than a scaled-up Body one).
    // cellWidth_px comes directly from the atlas; cellHeight_px is instead
    // anchored to Card::kHeight_in (see cardItem.cpp) so the card's total
    // rendered shape actually matches its declared physical size, rather
    // than whatever the font's own glyph proportions happen to produce --
    // rows are generally taller than a glyph's own pixel height as a
    // result, with the glyph vertically centered inside (see Cursor::
    // draw's drawCard).
    int cellWidth_px(Row row, const HackAtlas::Atlas& atlas) const;
    int cellHeight_px(Row row, const HackAtlas::Atlas& atlas) const;
    int rowTop_px(Row row, const HackAtlas::Atlas& atlas) const;

    // Blank horizontal margin on each side of every row's text, so the
    // first/last character isn't flush against the card's own edge --
    // see cardItem.cpp. Same value regardless of row, so title and body
    // text line up on the left.
    int sideMargin_px(const HackAtlas::Atlas& atlas) const;

    // Total rendered card size in pixels, atlas included -- unlike
    // cellWidth_px/cellHeight_px/rowTop_px, which describe one row's
    // cell, these describe the whole card (width including
    // sideMargin_px on both sides). The only two callers needing this
    // (Cursor::draw and main.cpp) used to each keep their own private
    // copy of this arithmetic; promoted here once cellHeight_px's own
    // physical-accuracy math (above) needed the true total width too.
    int cardWidth_px(const HackAtlas::Atlas& atlas) const;
    int cardHeight_px(const HackAtlas::Atlas& atlas) const;

    Row firstUserRow() const;
    Row lastUserRow() const;
    Col lastColAt(Row row) const;
    Col firstColAt(Row row) const;

    CardItem* tableOfContents();

    void setThreadStart(CardItem* threadStart);
    CardItem* threadStart() const;
    void setThreadPrev(CardItem* card);
    CardItem* threadPrev() const;
    void setThreadNext(CardItem* card);
    CardItem* threadNext() const;

    void setDeleted(bool deleted);
    bool deleted() const;
    // Whether setDeleted() would actually take effect right now -- the
    // exact predicate it already enforces internally, exposed so callers
    // (the keyboard panel's disabled-key styling, see keyboardPanel.h)
    // can know in advance rather than finding out by trying. A stack's
    // primary TOC (card 0 of its own thread) can never be deleted, and
    // neither can any read-only card (see setReadOnly) -- Master's, most
    // days, since setupInitialContent() marks every card there read-only.
    bool canDelete() const;

    void setReadOnly(bool readOnly);
    bool readOnly() const;
    void setRowReadOnly(Row row, bool readOnly);
    bool rowReadOnly(Row row) const;
    // Whether entering typing mode on this card would actually succeed
    // right now -- the exact predicate Cursor::enterTypingMode() already
    // enforces internally, exposed for the same reason as canDelete()
    // above. A TOC that isn't its own thread's start (a continuation
    // page) can't have its title edited, and neither can a read-only or
    // deleted card.
    bool canEdit() const;

    bool hasLinks() const;
    // How many links this card has -- distinct from hasLinks() (>=1),
    // used where "any at all" isn't specific enough.
    std::size_t linkCount() const;
    // Whether prevLink()/nextLink() would actually move anywhere from
    // here (both no-op at their respective end -- see their own
    // comments) -- true too when there are no links at all, so callers
    // don't need their own hasLinks() check first. The keyboard panel's
    // disabled-key styling uses these to gray out prev/next
    // independently: a card with several links doesn't disable either
    // one until you're actually sitting at one end.
    bool isAtFirstLink() const;
    bool isAtLastLink() const;
    CardLink currentLink() const;
    void nextLink();
    void prevLink();
    void setCurrentLink(CardItem* card);
    void setupPrevLink();
    void setupNextLink();
    virtual void setupLinks();

  protected:
    void setupLastRow();
    std::u32string linkStr(CardItem* card) const;

    std::vector<CardLink> m_links;
    int m_currentLinkIndex{-1};

  private:
    struct RowData
    {
        std::u32string text;
        bool readOnly{false};
    };

    CardItem* m_threadPrev{nullptr};
    CardItem* m_threadNext{nullptr};
    CardItem* m_threadStart{nullptr};
    std::array<RowData, Card::kNumRows> m_rows;
    CardNumber m_cardNum{0};
    Year m_year{0};
    bool m_deleted{false};
    bool m_readOnly{false};
};
