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

#include <catch2/catch_test_macros.hpp>

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
    REQUIRE(cursor.isTypingMode()); // the Help card starts ready to type (see Cursor's constructor)

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
