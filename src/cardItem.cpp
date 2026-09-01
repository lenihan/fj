#include "cardItem.h"
#include "hackAtlas.h"
#include "textUtil.h"

#include <cassert>
#include <cmath>

namespace
{
// Body-row height in inches, so the card's total rendered height actually
// matches Card::kHeight_in -- see cellHeight_px. Deriving it from the
// atlas's own glyph height instead (as an earlier version of this did)
// let the card's real shape drift wherever the font's natural glyph
// proportions happened to land (for Hack, that's ~2.6:1 wide:tall, not
// Card::kWidth_in/kHeight_in's 5:3), because nothing tied row height to
// the card's declared physical size at all. kRowUnits counts the title
// row as 2 (it renders at 2x a body row's height, same as its 2x width).
constexpr double kRowUnits = 2.0 + (Card::kNumRows - 1);
} // namespace

CardItem::CardItem(CardNumber cardNumber, Year year) : m_cardNum(cardNumber), m_year(year)
{
    for (Row row = 0; row < Card::kNumRows; ++row)
        m_rows[row].text.assign(colPerRow(row), U' ');

    setupLastRow();
}

bool CardItem::isTOC() const { return cardType() == Type::TOC; }
bool CardItem::isContent() const { return cardType() == Type::Content; }
bool CardItem::isThreadStart() const { return m_threadStart == this; }
bool CardItem::isThreadEnd() const { return m_threadNext == nullptr; }

void CardItem::setChar(char32_t c, Row row, Col col)
{
    assert(col < colPerRow(row));
    m_rows[row].text[col] = c;
}

void CardItem::setText(Row row, const std::u32string& text)
{
    assert(text.size() <= colPerRow(row));
    m_rows[row].text = text;
    m_rows[row].text.resize(colPerRow(row), U' ');
}

std::u32string CardItem::text(Row row) const
{
    return m_rows[row].text;
}

ColCount CardItem::colPerRow(Row row) const
{
    return row == 0 ? Title::kColsPerRow : Body::kColsPerRow;
}

CardNumber CardItem::cardNumber() const { return m_cardNum; }
Year CardItem::year() const { return m_year; }

int CardItem::cellWidth_px(Row row, const HackAtlas::Atlas& atlas) const
{
    return atlas.cellWidth * (row == 0 ? 2 : 1);
}

int CardItem::cellHeight_px(Row row, const HackAtlas::Atlas& atlas) const
{
    // Pixels-per-inch implied by this atlas -- back-deriving it from
    // cardWidth_px (the card's true rendered width, margins included,
    // which tools/offline/bakeFont's atlas sizing targets Card::kWidth_in
    // for) means this doesn't need a separate "current DPI" parameter of
    // its own, and it automatically tracks whichever atlas
    // Canvas::pickAtlas selects as the window resizes.
    double pixelsPerInch = cardWidth_px(atlas) / Card::kWidth_in;
    int bodyRowHeight_px = static_cast<int>(std::lround(pixelsPerInch * Card::kHeight_in / kRowUnits));
    return bodyRowHeight_px * (row == 0 ? 2 : 1);
}

int CardItem::sideMargin_px(const HackAtlas::Atlas& atlas) const
{
    return atlas.cellWidth * 2;
}

int CardItem::cardWidth_px(const HackAtlas::Atlas& atlas) const
{
    return Body::kColsPerRow * cellWidth_px(1, atlas) + 2 * sideMargin_px(atlas);
}

int CardItem::cardHeight_px(const HackAtlas::Atlas& atlas) const
{
    Row lastRow = Card::kNumRows - 1;
    return rowTop_px(lastRow, atlas) + cellHeight_px(lastRow, atlas);
}

int CardItem::rowTop_px(Row row, const HackAtlas::Atlas& atlas) const
{
    if (row == 0)
        return 0;
    // Every body row shares the same height, so any non-title row index
    // gives the right per-row height here.
    return cellHeight_px(0, atlas) + static_cast<int>(row - 1) * cellHeight_px(1, atlas);
}

Row CardItem::firstUserRow() const { return 1; }

Row CardItem::lastUserRow() const
{
    Row lastRow = Card::kNumRows - 1;
    return lastRow - 1;
}

Col CardItem::lastColAt(Row row) const { return colPerRow(row) - 1; }
Col CardItem::firstColAt(Row) const { return 0; }

CardItem* CardItem::tableOfContents()
{
    assert(m_threadStart);
    CardItem* toc = m_threadStart->isTOC() ? m_threadStart : m_threadStart->threadPrev();
    assert(toc);
    assert(toc->isTOC());
    return toc;
}

void CardItem::setThreadStart(CardItem* threadStart) { m_threadStart = threadStart; }
CardItem* CardItem::threadStart() const { return m_threadStart; }

void CardItem::setThreadPrev(CardItem* card)
{
    m_threadPrev = card;
    setupLastRow();
    m_links.clear();
    setupLinks();
}
CardItem* CardItem::threadPrev() const { return m_threadPrev; }

void CardItem::setThreadNext(CardItem* card)
{
    m_threadNext = card;
    setupLastRow();
    m_links.clear();
    setupLinks();
}
CardItem* CardItem::threadNext() const { return m_threadNext; }

bool CardItem::canDelete() const
{
    // Card 0 of a stack's own thread (its TOC) can never be deleted, and
    // neither can anything read-only (every card of Master's, most days
    // -- see setupInitialContent()) -- read-only used to only block
    // *editing*, not deletion, which was really the same "can't touch
    // this card at all" intent left half-enforced.
    return m_threadStart->cardNumber() != 0 && !m_readOnly;
}

void CardItem::setDeleted(bool deleted)
{
    if (!canDelete())
        return;
    m_deleted = deleted;
}
bool CardItem::deleted() const { return m_deleted; }

void CardItem::setReadOnly(bool readOnly) { m_readOnly = readOnly; }
bool CardItem::readOnly() const { return m_readOnly; }

bool CardItem::canEdit() const
{
    if (isTOC() && m_threadStart != this)
        return false; // a TOC continuation page's title can't be edited, only its own thread-start's
    return !m_readOnly && !m_deleted;
}

void CardItem::setRowReadOnly(Row row, bool readOnly) { m_rows[row].readOnly = readOnly; }
bool CardItem::rowReadOnly(Row row) const { return m_rows[row].readOnly; }

bool CardItem::hasLinks() const { return !m_links.empty(); }
std::size_t CardItem::linkCount() const { return m_links.size(); }

bool CardItem::isAtFirstLink() const { return m_links.empty() || m_currentLinkIndex <= 0; }
bool CardItem::isAtLastLink() const
{
    return m_links.empty() || static_cast<std::size_t>(m_currentLinkIndex) >= m_links.size() - 1;
}

CardItem::CardLink CardItem::currentLink() const
{
    assert(m_currentLinkIndex >= 0);
    assert(static_cast<size_t>(m_currentLinkIndex) < m_links.size());
    return m_links[m_currentLinkIndex];
}

void CardItem::nextLink()
{
    assert(!m_links.empty());
    if (static_cast<size_t>(m_currentLinkIndex) < m_links.size() - 1)
        ++m_currentLinkIndex;
}

void CardItem::prevLink()
{
    assert(!m_links.empty());
    if (m_currentLinkIndex != 0)
        --m_currentLinkIndex;
}

void CardItem::setCurrentLink(CardItem* card)
{
    assert(card);
    m_currentLinkIndex = -1;
    for (size_t i = 0; i < m_links.size(); ++i)
    {
        if (m_links[i].targetCard == card)
        {
            m_currentLinkIndex = static_cast<int>(i);
            return;
        }
    }
    assert(false); // should have found the card in the links
}

void CardItem::setupPrevLink()
{
    if (m_threadPrev)
    {
        std::u32string prev = linkStr(m_threadPrev);
        Row lastRow = Card::kNumRows - 1;
        m_links.push_back({lastRow, 0, static_cast<ColCount>(prev.size()), m_threadPrev});
    }
}

void CardItem::setupNextLink()
{
    if (m_threadNext)
    {
        std::u32string next = linkStr(m_threadNext);
        Row lastRow = Card::kNumRows - 1;
        Col col = static_cast<Col>(colPerRow(lastRow) - next.size());
        m_links.push_back({lastRow, col, static_cast<ColCount>(next.size()), m_threadNext});
    }
}

void CardItem::setupLinks()
{
    setupPrevLink();
    setupNextLink();
    m_currentLinkIndex = static_cast<int>(m_links.size()) - 1;
}

// Examples: up-arrow 4, right-arrow 2026-42
std::u32string CardItem::linkStr(CardItem* card) const
{
    if (!card)
        return {};

    std::u32string str = (m_threadStart == this && card == m_threadPrev) ? U"↑" : U"→";

    if (m_year != card->year())
    {
        str += (card->year() == Master::kYear) ? U"Master" : toU32(card->year());
        str += U"-";
    }
    str += toU32(card->cardNumber() + 1);
    return str;
}

void CardItem::setupLastRow()
{
    Row lastRow = Card::kNumRows - 1;
    setRowReadOnly(lastRow, true);

    // Last line: prev thread       card num       next thread
    ColCount colCount = colPerRow(lastRow);
    std::u32string text(colCount, U' ');

    if (m_threadPrev)
    {
        std::u32string prev = linkStr(m_threadPrev);
        text.replace(0, prev.size(), prev);
    }

    std::u32string cardNumStr = toU32(m_cardNum + 1);
    size_t pos = (colCount - cardNumStr.size()) / 2;
    text.replace(pos, cardNumStr.size(), cardNumStr);

    if (m_threadNext)
    {
        std::u32string next = linkStr(m_threadNext);
        text.replace(colCount - next.size(), next.size(), next);
    }

    setText(lastRow, text);
}
