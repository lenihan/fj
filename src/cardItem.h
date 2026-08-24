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
#include <string>
#include <vector>

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

    // Pixel geometry for row `row`, at render scale `scale` (the best-fit
    // integer scale chosen once at window-creation time -- see
    // PLAN_addendum.md's "Coordinate system (core)"). Title rows render at
    // 2x whatever scale Body rows use, same as the atlas reuse trick.
    int cellWidth_px(Row row, int scale) const;
    int cellHeight_px(Row row, int scale) const;
    int rowTop_px(Row row, int scale) const;

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

    void setReadOnly(bool readOnly);
    bool readOnly() const;
    void setRowReadOnly(Row row, bool readOnly);
    bool rowReadOnly(Row row) const;

    bool hasLinks() const;
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
