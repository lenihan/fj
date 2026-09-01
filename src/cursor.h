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

// The real current calendar year, via std::chrono's C++20/23 calendar
// API. Free function, not a Cursor member -- it seeds Cursor's own
// initial content (see setupInitialContent()) but has no Cursor state
// of its own, and tests need the exact same value to point additional
// scratch content at the same year stack setupInitialContent() already
// created (see cursorTests.cpp) rather than re-deriving it and risking
// a mismatch.
Year currentCalendarYear();

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
    // Whether command mode's navigation sub-state is currently Link
    // (reached via 'n', displayed as "Navigation mode" -- see
    // keyboardPanel.cpp's commandLegendFor/modeColorFor) rather than
    // Cursor. Needed by the keyboard panel's legend lookup to know which
    // command sub-state applies; also what handleKey's own dispatch gates
    // command keys on, now that Link mode is exclusive: only i/k/j/l stay
    // live there (see handleKey's own comment) -- n/e are ordinary
    // blocked keys like every other command key, not permanent
    // exceptions. The only way out is the cmd key stepping up one level
    // at a time (Link -> general Command -> Typing -- see handleKey's
    // CapsLock-release branch), never straight to Typing from Link.
    bool isLinkMode() const;

    // Whether left() (Navigation mode's "back") has anywhere to go --
    // exposed so the keyboard panel's disabled-key styling can know in
    // advance, the same way CardItem::canEdit()/canDelete() do for e/d.
    bool hasLinkHistory() const { return !m_linkHistory.empty(); }

    // Whether prevCard()/nextCard() (u/o) would actually go anywhere from
    // here -- the same "would this be a no-op" check those two already
    // make before calling shakeCardNo(), exposed so the keyboard panel's
    // disabled-key styling can know in advance.
    bool isAtFirstCard() const;
    bool isAtLastCard() const;

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

    // Whether prevThreadCard()/nextThreadCard() (m/. -- "prevT"/"nextT")
    // would actually go anywhere from here -- exposed so the keyboard
    // panel's disabled-key styling can know in advance, the same way
    // hasLinkHistory() does for j. Mirrors those methods' own "skip
    // deleted cards" walk (see findLivePrevThreadCard()/
    // findLiveNextThreadCard()) rather than just checking
    // CardItem::threadPrev()/threadNext() directly, so a thread whose
    // only neighbor is deleted correctly reads as having none.
    bool hasPrevThreadCard() const { return findLivePrevThreadCard() != nullptr; }
    bool hasNextThreadCard() const { return findLiveNextThreadCard() != nullptr; }

    void addNewCard(CardItem::Type type);
    void addContinuationCard(CardItem::Type type);

    void moveToTOCForNewCard();

    void toggleDeleteCard();

    // Replaces SquareGraphicsView::keyPressEvent/keyReleaseEvent's switch.
    void handleKey(const KeyEvent& event);

    // Draws the current card (background/lines/text) and the cursor
    // itself. atlas drives all layout (row heights, margins, cell
    // positions -- see cardItem.h) and Body row glyphs; titleAtlas is a
    // separately-picked, larger-baked atlas Title rows render their
    // glyphs from instead of upscaling atlas's own (see cursor.cpp's
    // drawCard and PLAN.md's Font atlas section) -- layout still assumes
    // Title is exactly 2x atlas's cell size regardless of titleAtlas's
    // own exact dimensions, so text may not perfectly fill to the row's
    // nominal right edge; a deliberate trade for real per-size AA over
    // pixel-exact column alignment. Unlike the old draw(QPainter*,
    // QRectF, bool capsDown), caps state isn't a parameter -- handleKey()
    // already tracks it.
    void draw(Canvas& canvas, const HackAtlas::Atlas& atlas, const HackAtlas::Atlas& titleAtlas) const;

  private:
    enum class NavigationMode
    {
        Link,
        Cursor
    };

    void showCard(CardItem* card);
    void tocCurrent();
    // The live (non-deleted) card prevThreadCard()/nextThreadCard() would
    // actually land on, or nullptr if there isn't one -- shared by those
    // two action methods and by hasPrevThreadCard()/hasNextThreadCard()
    // above, rather than duplicating the same skip-deleted walk twice.
    CardItem* findLivePrevThreadCard() const;
    CardItem* findLiveNextThreadCard() const;
    void addCard(CardItem::Type type, CardStack::ThreadMode threadMode);
    void shakeCardNo() const;

    // Builds the app's real starting content -- Master's TOC (read-only,
    // pointing at the current year's stack and a Help TOC of topics) and
    // the current year's own empty stack (its TOC linking back to
    // Master) -- in place of a single blank scratch card. Called once
    // from the constructor. See PLAN.md for the full design.
    void setupInitialContent();

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

    // Whether command mode is currently held on by a plain Caps Lock tap
    // (press+release with nothing typed in between) rather than by an
    // in-progress hold -- see handleKey's CapsLock-release branch. A
    // second plain tap releases this latch back to typing mode; distinct
    // from m_capsDown, which is already false again by the time a tap's
    // release is even being handled.
    bool m_capsTapLatched{false};

    // Whether a chorded command (hold cmd, tap a key, release -- 'c'/'t'/
    // 'e' specifically, via enterTypingMode()) already explicitly changed
    // the keyboard mode *during* the current hold, before cmd itself has
    // even been released yet. handleKey's CapsLock-release branch checks
    // this first and, if set, leaves the mode exactly as that command set
    // it -- its usual m_capsTapLatched/m_wasTypingMode "revert to
    // whatever preceded this hold" bookkeeping is stale by definition
    // once something mid-hold has already moved the mode on. Reset when
    // a fresh hold begins (CapsLock press) and consumed (reset again) the
    // moment a release actually checks it.
    bool m_modeChangedDuringHold{false};
};
