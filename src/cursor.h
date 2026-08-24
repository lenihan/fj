// cursor.h -- navigation/edit state machine, and the cursor's own draw
// pass. Ported off Qt: no QGraphicsScene (no scene graph at all anymore --
// see PLAN.md's show()/hide() removal), no QPen/QBrush (Canvas
// calls take a Pixel color directly), std:: containers and char32_t
// instead of Qt types.
//
// handleKey() absorbs the key-dispatch switch that used to live in
// SquareGraphicsView::keyPressEvent/keyReleaseEvent (see
// squareGraphicsView.cpp): home-row navigation (i/k/j/l -- there are no
// arrow keys in this app, see platform.h) and the
// caps-lock-forces-command-mode-while-held behavior. That dispatch needs
// Cursor's own mode state to interpret a KeyEvent, so it belongs here
// rather than in a platform shell.

#pragma once

#include "cardItem.h"
#include "cardStack.h"
#include "platform.h"
#include "types.h"

#include <map>
#include <memory>
#include <vector>

class Canvas;
namespace HackAtlas
{
struct Atlas;
}

class Cursor
{
  public:
    enum class KeyboardMode
    {
        Command,
        Typing
    };

    Cursor();

    CardNumber lastCardNumber() const;

    Year year() const;
    void setYear(Year year);

    Row row() const;
    void setRow(Row row);

    Col col() const;
    void setCol(Col col);

    CardItem* currentCard();
    void setCurrentCard(CardItem* card);

    bool isTypingMode() const;
    bool isCommandMode() const;
    void enterTypingMode();
    void enterCommandMode();

    void toggleNavigationMode();

    void up();
    void down();
    void left();
    void right();

    void enter();
    void backspace();

    void charTyped(char32_t c);

    void nextRow();
    void nextRowCreateCard();
    void prevRow();

    void nextCard();
    void prevCard();

    void prevThreadCard();
    void nextThreadCard();
    void nextThreadCardCreateCard();

    void addNewCard(CardItem::Type type);
    void addContinuationCard(CardItem::Type type);

    void moveToTOCForNewCard();

    void toggleDeleteCard();

    // Replaces SquareGraphicsView::keyPressEvent/keyReleaseEvent's switch.
    void handleKey(const KeyEvent& event);

    // Draws the current card (background/lines/text) and the cursor
    // itself, using whichever baked atlas is active for this frame (see
    // PLAN.md's "Coordinate system (core)"). Unlike the old
    // draw(QPainter*, QRectF, bool capsDown), caps state isn't a
    // parameter -- handleKey() already tracks it.
    void draw(Canvas& canvas, const HackAtlas::Atlas& atlas) const;

  private:
    enum class NavigationMode
    {
        Link,
        Cursor
    };

    void showCard(CardItem* card);
    void tocCurrent();
    void addCard(CardItem::Type type, CardStack::ThreadMode threadMode);
    void shakeCardNo() const;

    Year m_year{0};
    Row m_row{0};
    Col m_col{0};
    CardItem* m_currentCard{nullptr};

    KeyboardMode m_keyboardMode{KeyboardMode::Command};
    NavigationMode m_navigationMode{NavigationMode::Link};

    std::vector<CardItem*> m_linkHistory;

    // Ordered (not unordered_map) so a future "list of year card stacks"
    // UI (see PLAN.md's master-TOC glossary entry) can iterate
    // chronologically for free.
    std::map<Year, std::unique_ptr<CardStack>> m_yearToCardStack;

    // Key-dispatch state, absorbed from SquareGraphicsView.
    bool m_capsDown{false};
    bool m_wasTypingMode{false};
    KeyEvent::Kind m_lastKeyKind{KeyEvent::Kind::Enter}; // arbitrary non-CapsLock init
};
