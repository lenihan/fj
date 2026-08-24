#include "cardStack.h"
#include "contentItem.h"
#include "textUtil.h"
#include "tocItem.h"

#include <cassert>

CardStack::CardStack(Year year) : m_year(year)
{
    CardItem* newCard = add(CardItem::Type::TOC, ThreadMode::New);
    newCard->setReadOnly(true);

    // Old Qt code had `QString::number(m_year) = " TOC"` here -- an `=`
    // where a `+` was clearly meant, so every non-master year's TOC title
    // silently rendered as just " TOC" with the year missing. Fixed here.
    std::u32string title = (m_year == Master::kYear) ? U"Master TOC" : (toU32(m_year) + U" TOC");
    newCard->setText(0, title); // setText pads out to the full title row width
    newCard->setRowReadOnly(0, true);
}

CardItem* CardStack::cardItemAt(CardNumber cardNumber)
{
    assert(cardNumber < m_cards.size());
    return m_cards[cardNumber].get();
}

TOCItem* CardStack::tableOfContents()
{
    TOCItem* toc = dynamic_cast<TOCItem*>(m_cards.at(0).get());
    assert(toc);
    return toc;
}

CardItem* CardStack::lastCardItem() { return m_cards.back().get(); }

CardNumber CardStack::lastCardNumber() const { return static_cast<CardNumber>(m_cards.size() - 1); }

void CardStack::setReadOnly(bool readOnly) { m_readOnly = readOnly; }
bool CardStack::readOnly() const { return m_readOnly; }

CardItem* CardStack::add(CardItem::Type type, ThreadMode threadMode, CardItem* currentCard)
{
    CardNumber newCardNumber = m_cards.empty() ? 0 : static_cast<CardNumber>(lastCardNumber() + 1);

    std::unique_ptr<CardItem> owned;
    if (type == CardItem::Type::Content)
        owned = std::make_unique<ContentItem>(newCardNumber, m_year);
    else if (type == CardItem::Type::TOC)
        owned = std::make_unique<TOCItem>(newCardNumber, m_year);

    CardItem* newCard = owned.get();
    m_cards.push_back(std::move(owned));

    if (threadMode == ThreadMode::New)
    {
        newCard->setThreadStart(newCard);
        newCard->setThreadPrev(currentCard);

        if (currentCard)
        {
            auto* toc = dynamic_cast<TOCItem*>(currentCard);
            assert(toc);
            toc->addToTOC(newCard);
        }
    }
    else if (threadMode == ThreadMode::Continue)
    {
        newCard->setThreadStart(currentCard->threadStart());
        newCard->setThreadPrev(currentCard);
        newCard->setText(0, currentCard->text(0));
        newCard->setRowReadOnly(0, true);

        currentCard->setThreadNext(newCard);
    }
    return newCard;
}
