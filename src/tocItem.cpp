#include "tocItem.h"

#include <algorithm>
#include <cassert>
#include <iterator>

namespace
{
std::u32string rtrim(const std::u32string& str)
{
    size_t end = str.find_last_not_of(U' ');
    return (end == std::u32string::npos) ? std::u32string() : str.substr(0, end + 1);
}
} // namespace

TOCItem::TOCItem(CardNumber cardNumber, Year year) : CardItem(cardNumber, year)
{
    m_content.reserve(Card::kNumUserBodyRows);
}

std::u32string TOCItem::text(Row row) const
{
    // Content rows are computed fresh here rather than cached -- see the
    // header comment for why. Everything else (title, unused content
    // rows, the nav row) falls through to the base class's stored text,
    // which is correct as-is: rows never touched by addToTOC() stay
    // whatever CardItem's constructor initialized them to (spaces).
    if (row >= 1 && row <= m_content.size())
        return rowDisplayText(row);
    return CardItem::text(row);
}

void TOCItem::addToTOC(CardItem* card)
{
    assert(card);
    assert(m_content.size() < Card::kNumUserBodyRows);
    m_content.push_back(card);

    m_links.clear();
    assert(m_links.empty());
    setupLinks();
    assert(!m_links.empty());
    m_currentLinkIndex = 0;
}

RowCount TOCItem::numberContent() const { return static_cast<RowCount>(m_content.size()); }

CardItem* TOCItem::cardAtRow(Row row)
{
    assert(row >= 1);
    assert(row <= m_content.size());
    return m_content[row - 1];
}

Row TOCItem::rowAtCard(CardItem* card) const
{
    assert(card);
    auto it = std::find(m_content.begin(), m_content.end(), card);
    assert(it != m_content.end());
    return static_cast<Row>(std::distance(m_content.begin(), it) + 1);
}

bool TOCItem::isEmpty() const { return m_content.empty(); }

bool TOCItem::isFull() const
{
    assert(m_content.size() <= Card::kNumUserBodyRows);
    return m_content.size() == Card::kNumUserBodyRows;
}

void TOCItem::setupLinks()
{
    setupPrevLink();
    for (CardItem* card : m_content)
    {
        std::u32string cardLinkStr = linkStr(card);
        Row row = rowAtCard(card);
        Col col = static_cast<Col>(colPerRow(row) - cardLinkStr.size());
        m_links.push_back({row, col, static_cast<ColCount>(cardLinkStr.size()), card});
    }
    setupNextLink();

    if (threadPrev() == nullptr)
    {
        if (m_content.empty())
        {
            m_currentLinkIndex = -1;
            assert(threadNext() == nullptr);
        }
        else
            m_currentLinkIndex = 0;
    }
    else
    {
        m_currentLinkIndex = m_content.empty() ? 0 : 1;
    }
}

std::u32string TOCItem::rowDisplayText(Row row) const
{
    assert(row >= 1);
    assert(row <= m_content.size());
    CardItem* card = m_content[row - 1];
    int totalCol = colPerRow(row);

    std::u32string title = rtrim(card->text(0));
    std::u32string cardLinkStr = linkStr(card);
    constexpr int kTwoSpaces = 2;
    int dotsNeeded = totalCol - static_cast<int>(title.size()) - static_cast<int>(cardLinkStr.size()) - kTwoSpaces;
    std::u32string dots(dotsNeeded > 0 ? static_cast<size_t>(dotsNeeded) : 0, U'.');

    std::u32string text = title + U" " + dots + U" " + cardLinkStr;
    assert(text.size() == static_cast<size_t>(totalCol));
    return text;
}
