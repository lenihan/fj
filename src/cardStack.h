// cardStack.h -- owns every CardItem in one year's stack (or the master
// stack). Ownership used to live implicitly in QGraphicsScene (CardItem
// was `new`'d and handed to scene->addItem(), which owned it from then
// on); with no scene, CardStack -- which already creates every card via
// add() -- is the natural owner, via unique_ptr for real RAII lifetime
// instead of a borrowed scene-graph one.

#pragma once

#include "cardItem.h"
#include "types.h"

#include <memory>
#include <vector>

class TOCItem;

class CardStack
{
  public:
    enum class ThreadMode
    {
        New,
        Continue
    };

    explicit CardStack(Year year);

    CardItem* cardItemAt(CardNumber cardNumber);
    TOCItem* tableOfContents();
    CardItem* lastCardItem();
    CardNumber lastCardNumber() const;

    void setReadOnly(bool readOnly);
    bool readOnly() const;

    // Returns a non-owning observer pointer; CardStack retains ownership.
    CardItem* add(CardItem::Type type, ThreadMode threadMode, CardItem* currentCard = nullptr);

  private:
    Year m_year;
    std::vector<std::unique_ptr<CardItem>> m_cards;
    bool m_readOnly{false};
};
