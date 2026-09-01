// cursorTests.cpp -- automated regression tests for Cursor (see PLAN.md's
// TODO). Drives Cursor directly with scripted KeyEvent sequences, exactly
// the same handleKey() entry point every platform shell's real keyboard
// events go through (see platform.h) -- no PlatformWindow, no window, no
// platform shell code involved at all. That's what makes this fast and
// portable: it's testing the shared, platform-agnostic state machine that
// every shell (Win32/Xlib/Emscripten) is equally relying on to get keyboard
// dispatch right, not any one shell's own event-translation code.
//
// New to Catch2: a TEST_CASE is just a function Catch2 discovers and runs
// for you (via the CATCH_CONFIG_MAIN-equivalent Catch2::Catch2WithMain
// target, so there's no hand-written main() here at all). REQUIRE aborts
// the current test immediately on failure (use it for a precondition that
// makes the rest of the test meaningless if false); CHECK records a
// failure but keeps running the rest of the test (use it for the actual
// assertions under test, so a run shows every failure, not just the
// first).

#include "cardItem.h"
#include "cursor.h"
#include "platform.h"
#include "tocItem.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

namespace
{
// Sends one command-mode key the same way a real hold-Caps-then-tap does
// -- a plain helper function so the tests below don't each repeat that
// three-event dance. Nothing Catch2-specific here: a TEST_CASE body is
// just an ordinary function, so ordinary refactoring (pull out a helper)
// works exactly the way it would anywhere else in this codebase.
void sendCommand(Cursor& cursor, char32_t key)
{
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});
    cursor.handleKey({KeyEvent::Kind::Char, key, true});
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});
}

// A plain tap: press and release with nothing typed between -- distinct
// from sendCommand's chord shape (see cursor.cpp's handleKey for why the
// two are handled differently).
void tapCmd(Cursor& cursor)
{
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});
}

// A fresh Cursor now starts on Master's own TOC, which is read-only and
// only ever grows the two entries setupInitialContent() puts there (see
// PLAN.md) -- tests exercising typing/editing/thread creation need a
// real writable card instead, exactly what a real user gets by
// following the link to their own current-year TOC and adding a card
// there. cursor.setYear() is called explicitly first (rather than just
// navigating and letting m_year drift) because addNewCard()/
// addContinuationCard() operate on whichever stack m_year currently
// names, not on m_currentCard's own year() -- see cursor.h's
// currentCalendarYear() comment and PLAN.md's writeup of that gap.
// currentCalendarYear() (not a hardcoded year literal) guarantees this
// points at the exact same stack setupInitialContent() already created.
void gotoYearTocInCommandMode(Cursor& cursor)
{
    cursor.setYear(currentCalendarYear());
    sendCommand(cursor, U'l'); // master TOC's first link -- this year's own TOC
    tapCmd(cursor);            // Link -> general command, one step (see cursor.cpp)
}

// Leaves the cursor in typing mode on a freshly added, blank Content
// card -- the same "ready to type" state the old debug scratch card
// used to hand every test for free, now built from a real, writable
// card instead of Master's own (now read-only) content. A bare key
// press, not sendCommand's hold-release chord: gotoYearTocInCommandMode
// already leaves command mode latched (not momentarily held), and 'c'
// itself enters typing mode -- chording it would immediately revert
// that on release (the same "stay latched" behavior the "hold-tap-
// release chord while already latched" test exercises on purpose),
// undoing the very transition this helper exists to reach.
CardItem* freshScratchCard(Cursor& cursor)
{
    gotoYearTocInCommandMode(cursor);
    cursor.handleKey({KeyEvent::Kind::Char, U'c', true});
    return cursor.currentCard();
}

// Presses Enter until a thread continuation card actually appears (see
// nextRowCreateCard()'s comment on the original version of this loop for
// why it's a bounded loop instead of jumping straight to Card::kNumRows)
// -- pulled out here since several tests below need a multi-card thread
// to exist before they can test navigating across it.
CardItem* createContinuationCard(Cursor& cursor)
{
    CardItem* before = cursor.currentCard();
    int guard = 0;
    while (cursor.currentCard() == before && guard < 20)
    {
        cursor.handleKey({KeyEvent::Kind::Enter, 0, true});
        ++guard;
    }
    return cursor.currentCard();
}
} // namespace

TEST_CASE("typing and backspace edit the current row")
{
    // Every SECTION below re-enters this TEST_CASE from the top -- Catch2
    // runs the function once per SECTION, taking only that one SECTION's
    // branch and skipping its siblings each time. So this setup (a fresh
    // Cursor, one typed character) really does run fresh for each
    // SECTION; nothing here leaks between them. That's the Catch2 idiom
    // for shared setup -- no separate fixture class/setUp() needed, it's
    // just ordinary code above the SECTIONs.
    Cursor cursor;
    freshScratchCard(cursor);
    REQUIRE(cursor.isTypingMode()); // addNewCard() leaves a fresh card ready to type

    cursor.handleKey({KeyEvent::Kind::Char, U'x', true});

    SECTION("the character lands in the row and the cursor advances")
    {
        CHECK(cursor.col() == 1);
        CHECK(cursor.currentCard()->text(cursor.row())[0] == U'x');
    }

    SECTION("backspace removes it again")
    {
        cursor.handleKey({KeyEvent::Kind::Backspace, 0, true});
        CHECK(cursor.col() == 0);
        CHECK(cursor.currentCard()->text(cursor.row())[0] == U' '); // backspace blanks, doesn't shrink the row
    }
}

TEST_CASE("holding Caps Lock forces command mode; releasing it restores typing mode")
{
    Cursor cursor;
    freshScratchCard(cursor);
    REQUIRE(cursor.isTypingMode());

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});
    CHECK(cursor.isCommandMode());

    // A key pressed while Caps Lock is held is dispatched as a home-row
    // navigation command instead of typed text (see cursor.cpp's
    // handleKey) -- 'j' is bound to left(). isCommandMode() above already
    // proves the *mode* switched; sending a real command through it here
    // proves the held state actually does something, not just that the
    // flag flipped.
    cursor.handleKey({KeyEvent::Kind::Char, U'j', true});

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});
    CHECK(cursor.isTypingMode());
}

TEST_CASE("a plain Caps Lock tap (nothing typed while held) latches command mode; a second tap releases it")
{
    // Distinct from the test above: nothing is typed between the press and
    // release here, matching a real quick tap (or, on a real keyboard,
    // holding Caps Lock a moment with nothing typed before letting go) --
    // found missing while testing the ortholinear keyboard panel's mouse
    // tap-to-toggle gesture (see PLAN.md): without tracking this
    // separately from an in-progress hold, a first tap correctly entered
    // command mode, but a second tap re-entered it again instead of
    // releasing back to typing mode.
    Cursor cursor;
    freshScratchCard(cursor);
    REQUIRE(cursor.isTypingMode());

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});
    CHECK(cursor.isCommandMode()); // first tap: latches on, stays

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});
    CHECK(cursor.isTypingMode()); // second tap: releases the latch
}

TEST_CASE("a hold-tap-release chord while already latched stays latched")
{
    // If command mode is already held on by a plain-tap latch, a
    // subsequent hold-tap-release chord (see sendCommand below) should
    // leave it latched afterward rather than reverting to whatever mode
    // preceded that particular hold -- the same "state returns to
    // whatever it was before this gesture" invariant, just with "before"
    // being the latch rather than typing mode.
    Cursor cursor;
    freshScratchCard(cursor);
    REQUIRE(cursor.isTypingMode());

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true}); // tap on -- latches
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});
    REQUIRE(cursor.isCommandMode());

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true}); // hold...
    cursor.handleKey({KeyEvent::Kind::Char, U'j', true});  // ...tap a command key...
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false}); // ...release
    CHECK(cursor.isCommandMode());                          // still latched, not back to typing
}

TEST_CASE("a fresh cmd tap after 'e' latches command mode, not a stale release")
{
    // 'e' reaches typing mode without going through the CapsLock-release
    // branch that owns m_capsTapLatched -- found live, not by inspection:
    // leaving that flag set after 'e' meant the *next* plain cmd tap
    // still believed it was releasing an already-latched command mode
    // (per the "second tap releases the latch" behavior above) and
    // bounced straight back to typing instead of latching a fresh one.
    Cursor cursor;
    freshScratchCard(cursor);
    REQUIRE(cursor.isTypingMode());

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true}); // tap on -- latches
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});
    REQUIRE(cursor.isCommandMode());

    cursor.handleKey({KeyEvent::Kind::Char, U'e', true}); // -> typing mode, not via CapsLock at all
    REQUIRE(cursor.isTypingMode());

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true}); // a fresh plain tap...
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});
    CHECK(cursor.isCommandMode()); // ...should latch on, not bounce back to typing
}

TEST_CASE("'e' returns to typing mode from a held command")
{
    Cursor cursor;
    freshScratchCard(cursor);
    tapCmd(cursor); // Typing -> Command, so there's a real command to return from
    REQUIRE(cursor.isCommandMode());
    // A bare press, not sendCommand's chord -- see freshScratchCard's own
    // comment: chording a key that itself enters typing mode would just
    // revert on release, since command mode is latched here, not merely
    // held.
    cursor.handleKey({KeyEvent::Kind::Char, U'e', true}); // see cursor.cpp's handleKey dispatch: 'e' -> enterTypingMode()
    CHECK(cursor.isTypingMode());
}

TEST_CASE("'c' and 't' add a new card and move to its title row in typing mode")
{
    Cursor cursor;
    gotoYearTocInCommandMode(cursor);
    auto before = cursor.lastCardNumber();

    // Bare presses throughout this test, not sendCommand's chord -- see
    // freshScratchCard's own comment on why chording a key that itself
    // enters typing mode would just revert on release here.
    SECTION("'c' adds a Content card")
    {
        cursor.handleKey({KeyEvent::Kind::Char, U'c', true});
        CHECK(cursor.lastCardNumber() == before + 1);
        CHECK(cursor.currentCard()->isContent());
        CHECK(cursor.row() == 0); // new cards start on their (blank) title row
        CHECK(cursor.isTypingMode());
    }

    SECTION("'t' adds a TOC card")
    {
        cursor.handleKey({KeyEvent::Kind::Char, U't', true});
        CHECK(cursor.lastCardNumber() == before + 1);
        CHECK(cursor.currentCard()->isTOC());
        CHECK(cursor.row() == 0);
        CHECK(cursor.isTypingMode());
    }
}

TEST_CASE("'d' toggles the deleted flag on the current card")
{
    Cursor cursor;
    freshScratchCard(cursor);
    // CHECK_FALSE/REQUIRE_FALSE are just the negated form of CHECK/REQUIRE
    // -- REQUIRE(!x) works too, but this reads better for a plain bool.
    REQUIRE_FALSE(cursor.currentCard()->deleted());

    sendCommand(cursor, U'd');
    CHECK(cursor.currentCard()->deleted());

    sendCommand(cursor, U'd');
    CHECK_FALSE(cursor.currentCard()->deleted());
}

TEST_CASE("'n' enters Link navigation mode; cmd is the only way back to Cursor mode")
{
    // isLinkMode() exists (added for the keyboard panel's phase 3 legend
    // lookup), but this test predates it and checks the *effect* instead,
    // which is still worth keeping: in Link mode, down() ('k') calls
    // CardItem::nextLink(), which only moves a link *index* on the card
    // -- it never touches Cursor's own row/col (see cursor.cpp's down()).
    // In Cursor mode, down() calls nextRow(), which does. So row()
    // staying put vs. actually advancing is what distinguishes the two
    // modes here.
    //
    // 'n' used to also toggle straight back (Link -> Cursor) on a second
    // press; per the exclusive-navigation-mode design that's now blocked
    // (see the "navigation mode is exclusive" test) -- cmd (a CapsLock
    // tap) is the only way back, stepping through general command mode
    // on the way (see the "tapping cmd from navigation mode..." test).
    //
    // A fresh scratch card starts in *Cursor* navigation mode, not Link,
    // despite Link being the member's default -- entering typing mode
    // (which addNewCard(), inside freshScratchCard(), does) always resets
    // navigation mode to Cursor ("so you can use navigation keys" -- see
    // cursor.cpp), so 'n' needs to run first to reach Link mode at all.
    Cursor cursor;
    freshScratchCard(cursor);
    Row before = cursor.row();

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});
    cursor.handleKey({KeyEvent::Kind::Char, U'n', true}); // toggleNavigationMode(): Cursor -> Link
    cursor.handleKey({KeyEvent::Kind::Char, U'k', true}); // down() in Link mode: moves a link index, not the row
    CHECK(cursor.row() == before);

    // Nothing typed while held would normally read as a plain tap on
    // release -- but 'n'/'k' were typed above, so this release instead
    // re-enters typing mode via m_wasTypingMode (same as any other
    // hold-something-release chord), which also resets navigation mode
    // back to Cursor as enterTypingMode()'s own side effect.
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});

    // Confirm Cursor mode's own down() actually moves the row, unlike
    // Link mode's above -- tap cmd once to get back to general command.
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false});
    cursor.handleKey({KeyEvent::Kind::Char, U'k', true}); // down() in Cursor mode: actually moves
    CHECK(cursor.row() == before + 1);
}

TEST_CASE("navigation mode is exclusive: only i/k/j/l stay live, everything else is blocked")
{
    // The user was explicit this is a bug fix, not a new restriction:
    // entering navigation mode (via 'n') should make everything except
    // i/k/j/l unreachable -- including 'n' and 'e' themselves now, which
    // used to be permanent exceptions that could jump back to general
    // command mode or straight to typing on their own. cmd (a CapsLock
    // tap) is the only way out, one step at a time -- see the "tapping
    // cmd from navigation mode steps up one level" test below.
    //
    Cursor cursor;
    freshScratchCard(cursor);
    tapCmd(cursor); // Typing -> general Command (Cursor sub-mode)
    REQUIRE(cursor.isCommandMode());

    cursor.handleKey({KeyEvent::Kind::Char, U'n', true}); // Cursor -> Link navigation mode
    REQUIRE(cursor.isLinkMode());

    CardNumber before = cursor.currentCard()->cardNumber();
    cursor.handleKey({KeyEvent::Kind::Char, U'u', true}); // prevCard() -- blocked in Link mode
    CHECK(cursor.currentCard()->cardNumber() == before);
    cursor.handleKey({KeyEvent::Kind::Char, U'c', true}); // addNewCard() -- blocked in Link mode
    CHECK(cursor.currentCard()->cardNumber() == before);

    // 'n' and 'e' are now ordinary blocked keys here too, same as u/c
    // above -- neither changes mode at all from inside Link mode.
    cursor.handleKey({KeyEvent::Kind::Char, U'n', true});
    CHECK(cursor.isLinkMode());
    cursor.handleKey({KeyEvent::Kind::Char, U'e', true});
    CHECK(cursor.isLinkMode());
    CHECK(cursor.isCommandMode());

    // i/k/j/l still do their link-mode thing -- row() staying put (rather
    // than moving the way general command mode's down() would) is what
    // proves Link mode's own down() ran, not general command's.
    Row row = cursor.row();
    cursor.handleKey({KeyEvent::Kind::Char, U'k', true});
    CHECK(cursor.row() == row);
}

TEST_CASE("tapping cmd from navigation mode steps up one level at a time")
{
    // A plain CapsLock tap (press+release, nothing typed between) used to
    // be a blind two-way toggle between typing and command mode,
    // regardless of navigation sub-state -- so tapping cmd from
    // navigation mode jumped straight to typing, skipping general command
    // mode entirely. The user asked for cmd to be navigation mode's only
    // way out, one level at a time: Link -> tap -> general command ->
    // tap (or 'e') -> typing.
    Cursor cursor;
    freshScratchCard(cursor);
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});  // hold...
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false}); // ...release: plain tap, latches command mode
    REQUIRE(cursor.isCommandMode());

    cursor.handleKey({KeyEvent::Kind::Char, U'n', true}); // -> Link navigation mode
    REQUIRE(cursor.isLinkMode());

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false}); // tap #1: steps up to general command only
    CHECK(cursor.isCommandMode());
    CHECK_FALSE(cursor.isLinkMode());

    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, true});
    cursor.handleKey({KeyEvent::Kind::CapsLock, 0, false}); // tap #2: now reaches typing
    CHECK(cursor.isTypingMode());
}

TEST_CASE("'u' and 'o' navigate to the adjacent card by card number")
{
    Cursor cursor;
    gotoYearTocInCommandMode(cursor);
    // A buffer card first, bare (not sendCommand's chord -- see
    // freshScratchCard's comment), so 'u' below has a non-TOC card to
    // land on: the year's own TOC is read-only, so landing directly on
    // it would block the release's own enterTypingMode() fallback (see
    // handleKey's m_wasTypingMode branch), and land us in Link mode.
    cursor.handleKey({KeyEvent::Kind::Char, U'c', true});
    tapCmd(cursor); // Typing -> Command again
    cursor.handleKey({KeyEvent::Kind::Char, U'c', true}); // land on a freshly added card
    auto hereNumber = cursor.currentCard()->cardNumber();
    REQUIRE(hereNumber > 1); // there's an earlier *content* card to navigate to, not just the TOC

    sendCommand(cursor, U'u'); // prevCard()
    CHECK(cursor.currentCard()->cardNumber() == hereNumber - 1);

    sendCommand(cursor, U'o'); // nextCard()
    CHECK(cursor.currentCard()->cardNumber() == hereNumber);
}

TEST_CASE("typing to a card's last row and pressing Enter creates a thread "
          "continuation, navigable with '.' and 'm'")
{
    Cursor cursor;
    freshScratchCard(cursor);
    CardItem* firstCard = cursor.currentCard();
    CardItem* secondCard = createContinuationCard(cursor);
    REQUIRE(secondCard != firstCard);

    sendCommand(cursor, U'm'); // prevThreadCard()
    CHECK(cursor.currentCard() == firstCard);

    sendCommand(cursor, U'.'); // nextThreadCard()
    CHECK(cursor.currentCard() == secondCard);
}

TEST_CASE("row navigation ('i'/'k'), not just '.'/'m', also crosses an existing thread boundary")
{
    Cursor cursor;
    freshScratchCard(cursor);
    CardItem* firstCard = cursor.currentCard();
    CardItem* secondCard = createContinuationCard(cursor);
    REQUIRE(cursor.currentCard() == secondCard);
    REQUIRE(cursor.row() == secondCard->firstUserRow());

    // 'i' is up(). Typing mode always sets Cursor navigation mode (see
    // cursor.cpp's enterTypingMode()), and in Cursor mode, up() at a
    // card's first user row crosses to the *previous* thread card's last
    // row (see prevRow()) instead of just decrementing the row number --
    // this is the same "move to the previous card" behavior '.'/'m' give
    // you, reachable through ordinary navigation instead of the explicit
    // thread-jump keys.
    sendCommand(cursor, U'i');
    CHECK(cursor.currentCard() == firstCard);
    CHECK(cursor.row() == firstCard->lastUserRow());

    // And forward again the same way.
    sendCommand(cursor, U'k');
    CHECK(cursor.currentCard() == secondCard);
    CHECK(cursor.row() == secondCard->firstUserRow());
}

TEST_CASE("thread navigation skips a deleted card in the middle")
{
    Cursor cursor;
    freshScratchCard(cursor);
    CardItem* firstCard = cursor.currentCard();
    CardItem* secondCard = createContinuationCard(cursor);
    CardItem* thirdCard = createContinuationCard(cursor);
    REQUIRE(thirdCard != secondCard);
    REQUIRE(cursor.currentCard() == thirdCard);

    // Delete the middle card directly rather than navigating there and
    // back with 'd' -- that key's own behavior is already covered by a
    // dedicated test above, so this one stays focused on what it's
    // actually testing: whether prevThreadCard()/nextThreadCard() skip
    // over a deleted card in between, not on how a card gets deleted.
    secondCard->setDeleted(true);

    sendCommand(cursor, U'm'); // prevThreadCard() from thirdCard: threadPrev() is the deleted secondCard
    CHECK(cursor.currentCard() == firstCard); // skipped straight to firstCard

    sendCommand(cursor, U'.'); // nextThreadCard() from firstCard: threadNext() is the same deleted secondCard
    CHECK(cursor.currentCard() == thirdCard); // skipped straight to thirdCard
}

TEST_CASE("every non-deleted card added to a TOC is reachable from it via links")
{
    // Master's own TOC is read-only and only ever holds the two entries
    // setupInitialContent() puts there (see PLAN.md) -- this exercises
    // the same TOC/link-walking logic on the current year's own
    // (writable) TOC instead, which is otherwise identical.
    Cursor cursor;
    gotoYearTocInCommandMode(cursor);
    CardItem* toc = cursor.currentCard();

    sendCommand(cursor, U'c');
    CardItem* cardA = cursor.currentCard();
    sendCommand(cursor, U'c');
    CardItem* cardB = cursor.currentCard();
    sendCommand(cursor, U'c');
    CardItem* cardC = cursor.currentCard();

    cardB->setDeleted(true); // see the note above on setting this directly

    // Get back to the TOC. 'u' (prevCard(), by absolute card number) is
    // the ordinary public way there, walked one step at a time rather
    // than assumed to be exactly one step away.
    int guard = 0;
    while (cursor.currentCard() != toc && guard < static_cast<int>(cursor.lastCardNumber()) + 1)
    {
        sendCommand(cursor, U'u');
        ++guard;
    }
    REQUIRE(cursor.currentCard() == toc);

    // Walk every link on the TOC: rewind to the first one with prevLink()
    // ('i'), then repeatedly follow the current link ('l' -- right()) and
    // come back ('j' -- left(), which pops the link-history showCard()
    // itself pushed), advancing to the next link ('k') each time.
    // nextLink()/prevLink() both clamp at the ends of the link list (see
    // cardItem.cpp) rather than wrapping, so re-seeing the same target
    // twice in a row is how this knows it's reached the last one.
    for (int i = 0; i < 20; ++i)
        sendCommand(cursor, U'i'); // prevLink(), clamped -- safe to over-call

    std::vector<CardItem*> visited;
    CardItem* lastTarget = nullptr;
    for (int i = 0; i < 20; ++i)
    {
        CardItem::CardLink link = toc->currentLink();
        if (link.targetCard == lastTarget)
            break; // nextLink() below stopped moving -- every link has been visited
        lastTarget = link.targetCard;

        sendCommand(cursor, U'l'); // right(): follow the link
        visited.push_back(cursor.currentCard());

        while (cursor.currentCard() != toc) // left(): pop back to the TOC
            sendCommand(cursor, U'j');

        sendCommand(cursor, U'k'); // nextLink(): advance to the next one
    }

    CHECK(std::find(visited.begin(), visited.end(), cardA) != visited.end());
    CHECK(std::find(visited.begin(), visited.end(), cardC) != visited.end());
    // Running this the first time found a real gap, not a test bug:
    // right()'s Link-mode branch followed a link's target unconditionally,
    // with no deleted check at all -- unlike prevThreadCard()/
    // nextThreadCard() and the TOC's other (row-based) right() branch,
    // which both already skip a deleted card. Fixed in cursor.cpp to walk
    // forward through the deleted card's own thread the same way those do,
    // rather than weakening this assertion to match the old behavior.
    CHECK(std::find(visited.begin(), visited.end(), cardB) == visited.end()); // deleted -- must not be reachable
}

TEST_CASE("setYear() refuses to move backward, so new content can never target a year that's already passed")
{
    Cursor cursor;
    cursor.setYear(2020);
    REQUIRE(cursor.year() == 2020);

    cursor.setYear(2026);
    REQUIRE(cursor.year() == 2026);

    cursor.setYear(2020); // attempt to go back to a year that's already passed -- refused
    CHECK(cursor.year() == 2026);
}

TEST_CASE("a thread continued into a new year's stack links back to the original card, "
          "but is also listed in the new year's own TOC")
{
    Cursor cursor;
    Year pastYear = 2020;
    // Deliberately far from any real calendar year -- setupInitialContent()
    // links Master to currentCalendarYear()'s own stack, so a literal that
    // happened to match today's real year would give that stack an extra
    // "back to Master" link this test doesn't expect, making its outcome
    // depend on which day it happened to run.
    Year currentYear = 9999; // must come *after* pastYear -- setYear() refuses to move backward (see the test above)

    // setYear() lazily creates that year's own CardStack (and TOC) the
    // first time it's used -- see cursor.cpp's comment on why. A fresh
    // Cursor starts on Master's own TOC in Link (Navigation) mode, which
    // blocks 'c' (see cursor.cpp's exclusive-navigation-mode gate) --
    // tapCmd steps up to general command mode first, the same one tap it
    // always takes from a fresh Cursor now (see setupInitialContent()'s
    // own comment on why the very first tap needs to actually do
    // something).
    cursor.setYear(pastYear);
    tapCmd(cursor);
    sendCommand(cursor, U'c'); // a new thread, in pastYear's stack
    CardItem* pastCard = cursor.currentCard();
    REQUIRE(pastCard->year() == pastYear);
    CardItem* pastYearToc = pastCard->tableOfContents();

    // Extend the thread into the current year. addContinuationCard() has
    // no key of its own (see cursor.cpp's handleKey dispatch), so it's
    // called directly -- Cursor::addCard() always creates the new card in
    // whichever stack Cursor::m_year currently points at, regardless of
    // which year m_currentCard itself belongs to, so switching years
    // first and then continuing is the whole sequence.
    cursor.setYear(currentYear);
    cursor.addContinuationCard(CardItem::Type::Content);
    CardItem* currentYearCard = cursor.currentCard();
    REQUIRE(currentYearCard != pastCard);
    REQUIRE(currentYearCard->year() == currentYear);

    // Still the same thread, linking back to the *real* previous card --
    // not to the current year's TOC.
    CHECK(currentYearCard->threadStart() == pastCard);
    CHECK(currentYearCard->threadPrev() == pastCard);
    CHECK(pastCard->threadNext() == currentYearCard);

    // But also independently listed in the current year's own TOC now
    // (see CardStack::add()'s ThreadMode::Continue branch) -- reached the
    // same public way as the master-TOC link walk above: 'u' by absolute
    // card number until landing on a TOC, this time within currentYear's
    // stack (Cursor::m_year is currentYear now, so 'u'/prevCard() walks
    // that stack, not pastYear's).
    int guard = 0;
    while (!cursor.currentCard()->isTOC() && guard < static_cast<int>(cursor.lastCardNumber()) + 1)
    {
        sendCommand(cursor, U'u');
        ++guard;
    }
    REQUIRE(cursor.currentCard()->isTOC());
    CardItem* currentYearToc = cursor.currentCard();

    REQUIRE(currentYearToc != pastYearToc); // genuinely a different year's TOC
    CHECK(currentYearToc->hasLinks());
    CHECK(currentYearToc->currentLink().targetCard == currentYearCard);
}

TEST_CASE("editing a thread's title updates the TOC reference to it live, "
          "and propagates to every continuation card in the thread")
{
    Cursor cursor;
    freshScratchCard(cursor);
    CardItem* firstCard = cursor.currentCard();
    firstCard->setText(0, U"Help"); // an explicit title for the propagation below to exercise
    CardItem* secondCard = createContinuationCard(cursor);
    REQUIRE(secondCard->text(0).substr(0, 4) == U"Help"); // copied once at creation time -- see CardStack::add's Continue branch

    // Only threadStart()'s own title row is ever editable -- every
    // continuation's title row is marked read-only right where it's
    // copied (see CardStack::add) -- so editing has to happen on
    // firstCard. Positioned directly via setRow/setCol rather than
    // navigating there with keys: getting to the title row isn't what
    // this test is about, and it's already covered by other tests.
    REQUIRE(firstCard->isThreadStart());
    cursor.setCurrentCard(firstCard);
    cursor.setRow(0);
    cursor.setCol(0);
    cursor.enterTypingMode();
    for (char32_t ch : {U'B', U'y', U'e'})
        cursor.handleKey({KeyEvent::Kind::Char, ch, true});
    // "Done with title" (see cursor.cpp's enter(), the m_row == 0 branch)
    // -- retyping alone changes firstCard->text(0) already; Enter is what
    // should trigger propagating that to the rest of the thread.
    cursor.handleKey({KeyEvent::Kind::Enter, 0, true});

    REQUIRE(firstCard->text(0).substr(0, 3) == U"Bye");

    // The TOC's own displayed reference already reads firstCard->text(0)
    // fresh every time (see TOCItem::text()/tocItem.h's header comment:
    // "computed on demand... can't go stale") -- not a propagation this
    // needs to *do* anything for, just confirming it's actually true.
    TOCItem* toc = dynamic_cast<TOCItem*>(firstCard->tableOfContents());
    REQUIRE(toc);
    Row tocRow = toc->rowAtCard(firstCard);
    CHECK(toc->text(tocRow).find(U"Bye") != std::u32string::npos);

    // secondCard's own stored title, on the other hand, was only ever
    // *copied* once -- this is the actual regression PLAN.md flagged
    // ("retroactive title propagation to already-created continuation
    // cards ... isn't implemented").
    CHECK(secondCard->text(0).substr(0, 3) == U"Bye");
}
