// keyboardPanelTests.cpp -- automated regression tests for layoutKeys
// (see PLAN.md's "Emulator keyboard panels" note). layoutKeys is a pure
// function of (leftSide, panelSize_px) with no Cursor/window dependency
// (see keyboardPanel.h), so -- like cursorTests.cpp -- this needs no
// PlatformWindow, no window, no platform shell code at all.

#include "keyboardPanel.h"
#include "layout.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>
#include <vector>

namespace
{
constexpr int kPanelSize_px = 500; // an arbitrary, reasonably large panel resolution
}

TEST_CASE("layoutKeys returns the full grid plus one spacebar key")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    REQUIRE(keys.size() == KeyboardPanel::kCols * KeyboardPanel::kKeyRows + 1);
}

TEST_CASE("layoutKeys' spacebar key is double-width and aligned to the screen side")
{
    SECTION("left panel: spacebar sits at the panel's right edge")
    {
        auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
        const KeyRect& spacebar = keys.back();
        CHECK(spacebar.label == U"spacebar");

        // Double-width: noticeably wider than an ordinary single key.
        const KeyRect& ordinaryKey = keys.front();
        CHECK(spacebar.rect.w > ordinaryKey.rect.w * 1.5);

        // Right-aligned: its right edge should match the rightmost
        // ordinary key's right edge in the same grid.
        int rightmostOrdinaryEdge = 0;
        for (std::size_t i = 0; i + 1 < keys.size(); ++i)
            rightmostOrdinaryEdge = std::max(rightmostOrdinaryEdge, keys[i].rect.x + keys[i].rect.w);
        CHECK(spacebar.rect.x + spacebar.rect.w == rightmostOrdinaryEdge);
    }

    SECTION("right panel: spacebar sits at the panel's left edge")
    {
        auto keys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
        const KeyRect& spacebar = keys.back();
        CHECK(spacebar.label == U"spacebar");

        int leftmostOrdinaryEdge = kPanelSize_px;
        for (std::size_t i = 0; i + 1 < keys.size(); ++i)
            leftmostOrdinaryEdge = std::min(leftmostOrdinaryEdge, keys[i].rect.x);
        CHECK(spacebar.rect.x == leftmostOrdinaryEdge);
    }
}

TEST_CASE("layoutKeys keeps every key within the panel's bounds")
{
    for (bool leftSide : {true, false})
    {
        auto keys = layoutKeys(leftSide, kPanelSize_px);
        for (const KeyRect& key : keys)
        {
            CHECK(key.rect.x >= 0);
            CHECK(key.rect.y >= 0);
            CHECK(key.rect.x + key.rect.w <= kPanelSize_px);
            CHECK(key.rect.y + key.rect.h <= kPanelSize_px);
        }
    }
}

TEST_CASE("layoutKeys' left and right panels are mirror images with different labels")
{
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    REQUIRE(leftKeys.size() == rightKeys.size());

    // Same geometry (grid position/size), since both panels are the same
    // physical shape -- only the labels differ between the two sides.
    bool anyLabelDiffers = false;
    for (std::size_t i = 0; i < leftKeys.size(); ++i)
    {
        CHECK(leftKeys[i].rect.w == rightKeys[i].rect.w);
        CHECK(leftKeys[i].rect.h == rightKeys[i].rect.h);
        if (leftKeys[i].label != rightKeys[i].label)
            anyLabelDiffers = true;
    }
    CHECK(anyLabelDiffers);
}

namespace
{
const KeyRect& findKey(const std::vector<KeyRect>& keys, std::u32string_view label)
{
    auto it = std::ranges::find(keys, label, &KeyRect::label);
    REQUIRE(it != keys.end());
    return *it;
}
} // namespace

TEST_CASE("each key's label maps to the action a physical keypress of it would trigger")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);

    SECTION("cmd toggles/chords -- see resolveKeyGesture")
    {
        const KeyRect& cmd = findKey(keys, U"cmd");
        CHECK(cmd.action == KeyRect::Action::CapsToggle);
        CHECK(cmd.kind == KeyEvent::Kind::CapsLock);
    }

    SECTION("shift toggles/chords too, but sends no KeyEvent of its own")
    {
        CHECK(findKey(keys, U"shift").action == KeyRect::Action::ShiftToggle);
    }

    SECTION("tab has no mapped action yet -- a tap is a no-op")
    {
        CHECK(findKey(keys, U"tab").action == KeyRect::Action::None);
    }

    SECTION("a single-codepoint label fires exactly that character")
    {
        const KeyRect& q = findKey(keys, U"q");
        CHECK(q.action == KeyRect::Action::Fire);
        CHECK(q.kind == KeyEvent::Kind::Char);
        CHECK(q.codepoint == U'q');
    }

    SECTION("spacebar fires a literal space character")
    {
        const KeyRect& spacebar = keys.back();
        CHECK(spacebar.label == U"spacebar");
        CHECK(spacebar.action == KeyRect::Action::Fire);
        CHECK(spacebar.kind == KeyEvent::Kind::Char);
        CHECK(spacebar.codepoint == U' ');
    }

    SECTION("enter and bs (right panel) fire Enter/Backspace")
    {
        auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
        CHECK(findKey(rightKeys, U"enter").kind == KeyEvent::Kind::Enter);
        CHECK(findKey(rightKeys, U"bs").kind == KeyEvent::Kind::Backspace);
    }
}

TEST_CASE("hitTestPanel finds the key under a point, and nothing between keys")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& someKey = keys[7]; // an arbitrary ordinary (non-spacebar) key
    Point center{someKey.rect.x + someKey.rect.w / 2, someKey.rect.y + someKey.rect.h / 2};

    auto hit = hitTestPanel(/*leftSide=*/true, kPanelSize_px, center);
    REQUIRE(hit.has_value());
    CHECK(hit->label == someKey.label);

    // Outside the whole grid entirely -- panelSize_px is well beyond the
    // margin layoutKeys centers the grid within.
    CHECK_FALSE(hitTestPanel(/*leftSide=*/true, kPanelSize_px, {0, 0}).has_value());
}

// resolveKeyGesture: cmd's two gestures (tap-toggle, press-drag-release
// chord) and both shift keys' two gestures -- see PLAN.md's "Emulator
// keyboard panels" write-up for why the user asked these be validated
// individually rather than assumed identical/symmetric.
TEST_CASE("resolveKeyGesture: cmd tap toggles the persistent latch")
{
    // A real press+release pair every time, regardless of whether the tap
    // is turning the latch on or off -- Cursor::handleKey's tap detection
    // (m_capsTapLatched) only recognizes a plain tap by seeing a press
    // immediately followed by a release with nothing typed between, the
    // same shape tests/cursorTests.cpp already sends directly. A single
    // collapsed event (this used to send only one, whose `pressed`
    // mirrored the new latch state) meant Cursor never actually saw a
    // press *and* a release, so a second plain click could never reach
    // the branch that releases the latch -- found live, not by
    // inspection (see keyboardPanel.cpp's own comment on this).
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& cmd = findKey(keys, U"cmd");

    auto turnOn = resolveKeyGesture(cmd, true, cmd, true, /*capsLatchedBefore=*/false, false, true);
    REQUIRE(turnOn.events.size() == 2);
    CHECK(turnOn.events[0].kind == KeyEvent::Kind::CapsLock);
    CHECK(turnOn.events[0].pressed);
    CHECK(turnOn.events[1].kind == KeyEvent::Kind::CapsLock);
    CHECK_FALSE(turnOn.events[1].pressed);
    CHECK(turnOn.capsLatched);

    auto turnOff = resolveKeyGesture(cmd, true, cmd, true, /*capsLatchedBefore=*/true, false, true);
    REQUIRE(turnOff.events.size() == 2);
    CHECK(turnOff.events[0].kind == KeyEvent::Kind::CapsLock);
    CHECK(turnOff.events[0].pressed);
    CHECK(turnOff.events[1].kind == KeyEvent::Kind::CapsLock);
    CHECK_FALSE(turnOff.events[1].pressed);
    CHECK_FALSE(turnOff.capsLatched);
}

TEST_CASE("resolveKeyGesture: cmd chord (drag to a letter), latch initially off")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& cmd = findKey(keys, U"cmd");
    const KeyRect& q = findKey(keys, U"q");

    auto outcome = resolveKeyGesture(cmd, true, q, true, /*capsLatchedBefore=*/false, false, true);

    REQUIRE(outcome.events.size() == 3);
    CHECK(outcome.events[0].kind == KeyEvent::Kind::CapsLock);
    CHECK(outcome.events[0].pressed);
    CHECK(outcome.events[1].kind == KeyEvent::Kind::Char);
    CHECK(outcome.events[1].codepoint == U'q');
    CHECK(outcome.events[2].kind == KeyEvent::Kind::CapsLock);
    CHECK_FALSE(outcome.events[2].pressed);
    CHECK_FALSE(outcome.capsLatched); // the gesture never touches the persistent latch
}

TEST_CASE("resolveKeyGesture: cmd chord (drag to a letter), latch initially on")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& cmd = findKey(keys, U"cmd");
    const KeyRect& q = findKey(keys, U"q");

    auto outcome = resolveKeyGesture(cmd, true, q, true, /*capsLatchedBefore=*/true, false, true);

    REQUIRE(outcome.events.size() == 1); // already in command mode -- no redundant CapsLock pair
    CHECK(outcome.events[0].kind == KeyEvent::Kind::Char);
    CHECK(outcome.events[0].codepoint == U'q');
    CHECK(outcome.capsLatched); // unchanged
}

TEST_CASE("resolveKeyGesture: cmd released off any key cancels")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& cmd = findKey(keys, U"cmd");

    auto outcome = resolveKeyGesture(cmd, true, std::nullopt, true, /*capsLatchedBefore=*/false, false, true);
    CHECK(outcome.events.empty());
    CHECK_FALSE(outcome.capsLatched);
}

TEST_CASE("resolveKeyGesture: left shift tap toggles its latch")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& leftShift = findKey(keys, U"shift");

    auto turnOn = resolveKeyGesture(leftShift, true, leftShift, true, false, /*shiftLatchedBefore=*/false, true);
    CHECK(turnOn.events.empty()); // shift has no KeyEvent of its own
    CHECK(turnOn.shiftLatched);

    auto turnOff = resolveKeyGesture(leftShift, true, leftShift, true, false, /*shiftLatchedBefore=*/true, true);
    CHECK(turnOff.events.empty());
    CHECK_FALSE(turnOff.shiftLatched);
}

TEST_CASE("resolveKeyGesture: right shift tap toggles its latch too")
{
    // Same behavior as left shift, verified against the right panel's own
    // instance specifically -- not just assumed identical.
    auto keys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& rightShift = findKey(keys, U"shift");

    auto turnOn = resolveKeyGesture(rightShift, false, rightShift, false, false, /*shiftLatchedBefore=*/false, true);
    CHECK(turnOn.events.empty());
    CHECK(turnOn.shiftLatched);

    auto turnOff = resolveKeyGesture(rightShift, false, rightShift, false, false, /*shiftLatchedBefore=*/true, true);
    CHECK(turnOff.events.empty());
    CHECK_FALSE(turnOff.shiftLatched);
}

TEST_CASE("resolveKeyGesture: left shift chord (drag to a letter) capitalizes it")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& leftShift = findKey(keys, U"shift");
    const KeyRect& q = findKey(keys, U"q");

    auto outcome = resolveKeyGesture(leftShift, true, q, true, false, /*shiftLatchedBefore=*/false, true);

    REQUIRE(outcome.events.size() == 1);
    CHECK(outcome.events[0].kind == KeyEvent::Kind::Char);
    CHECK(outcome.events[0].codepoint == U'Q');
    CHECK_FALSE(outcome.shiftLatched); // unchanged
}

TEST_CASE("resolveKeyGesture: right shift chord (drag to a letter) capitalizes it too")
{
    // Dragging from the right panel's own shift key to a letter on the
    // right panel -- confirms it isn't only the left instance wired up.
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& rightShift = findKey(rightKeys, U"shift");
    const KeyRect& u = findKey(rightKeys, U"u");

    auto outcome = resolveKeyGesture(rightShift, false, u, false, false, /*shiftLatchedBefore=*/false, true);

    REQUIRE(outcome.events.size() == 1);
    CHECK(outcome.events[0].kind == KeyEvent::Kind::Char);
    CHECK(outcome.events[0].codepoint == U'U');
    CHECK_FALSE(outcome.shiftLatched);
}

TEST_CASE("resolveKeyGesture: shift chord still capitalizes even while already latched")
{
    // The bug caught while designing these tests: shift has no Cursor-side
    // state to preserve the way cmd does, so the chord's outcome should
    // never be conditional on shiftLatchedBefore -- always capital.
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& leftShift = findKey(keys, U"shift");
    const KeyRect& q = findKey(keys, U"q");

    auto outcome = resolveKeyGesture(leftShift, true, q, true, false, /*shiftLatchedBefore=*/true, true);

    REQUIRE(outcome.events.size() == 1);
    CHECK(outcome.events[0].codepoint == U'Q');
    CHECK(outcome.shiftLatched); // unchanged
}

TEST_CASE("resolveKeyGesture: shift released off any key cancels")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& leftShift = findKey(keys, U"shift");

    auto outcome = resolveKeyGesture(leftShift, true, std::nullopt, true, false, /*shiftLatchedBefore=*/false, true);
    CHECK(outcome.events.empty());
    CHECK_FALSE(outcome.shiftLatched);
}

TEST_CASE("resolveKeyGesture: a plain letter tap stays capitalized while shift is latched")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& q = findKey(keys, U"q");

    auto outcome = resolveKeyGesture(q, true, q, true, false, /*shiftLatchedBefore=*/true, true);

    REQUIRE(outcome.events.size() == 1);
    CHECK(outcome.events[0].codepoint == U'Q');
}

TEST_CASE("resolveKeyGesture: shift transform is skipped outside typing mode")
{
    // The bug found designing phase 3: shift-latching while in command
    // mode must not hand Cursor's command switch an uppercase codepoint
    // it can't match.
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& q = findKey(keys, U"q");

    auto plainTap = resolveKeyGesture(q, true, q, true, false, /*shiftLatchedBefore=*/true, /*isTypingMode=*/false);
    REQUIRE(plainTap.events.size() == 1);
    CHECK(plainTap.events[0].codepoint == U'q'); // not 'Q'

    auto keys2 = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& shift = findKey(keys2, U"shift");
    auto chord = resolveKeyGesture(shift, true, q, true, false, false, /*isTypingMode=*/false);
    REQUIRE(chord.events.size() == 1);
    CHECK(chord.events[0].codepoint == U'q'); // not 'Q'
}

TEST_CASE("resolveKeyGesture: shift-symbol table matches a real keyboard")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& one = findKey(keys, U"1");
    const KeyRect& semicolon = findKey(keys, U";");
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& slash = findKey(rightKeys, U"/");

    auto bang = resolveKeyGesture(one, true, one, true, false, /*shiftLatchedBefore=*/true, /*isTypingMode=*/true);
    REQUIRE(bang.events.size() == 1);
    CHECK(bang.events[0].codepoint == U'!');

    auto colon =
        resolveKeyGesture(semicolon, true, semicolon, true, false, /*shiftLatchedBefore=*/true, /*isTypingMode=*/true);
    REQUIRE(colon.events.size() == 1);
    CHECK(colon.events[0].codepoint == U':');

    auto question =
        resolveKeyGesture(slash, false, slash, false, false, /*shiftLatchedBefore=*/true, /*isTypingMode=*/true);
    REQUIRE(question.events.size() == 1);
    CHECK(question.events[0].codepoint == U'?');
}

TEST_CASE("typingLabelFor previews shift's effect: capitals and symbols, matching a real keyboard")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& q = findKey(keys, U"q");
    const KeyRect& one = findKey(keys, U"1");
    const KeyRect& spacebar = keys.back();
    const KeyRect& tab = findKey(keys, U"tab");
    const KeyRect& shift = findKey(keys, U"shift");

    CHECK(typingLabelFor(q, /*shiftEngaged=*/false) == U"q");
    CHECK(typingLabelFor(q, /*shiftEngaged=*/true) == U"Q");
    CHECK(typingLabelFor(one, /*shiftEngaged=*/false) == U"1");
    CHECK(typingLabelFor(one, /*shiftEngaged=*/true) == U"!");

    // Keys with no shifted form of their own are unaffected.
    CHECK(typingLabelFor(spacebar, /*shiftEngaged=*/true) == U"spacebar");
    CHECK(typingLabelFor(tab, /*shiftEngaged=*/true) == U"tab");
    CHECK(typingLabelFor(shift, /*shiftEngaged=*/true) == U"shift");
}

TEST_CASE("commandLegendFor: general command mode describes every implemented key, blank otherwise")
{
    // i/k/j/l/u/o/m/./h/y/p live on the right panel, the rest of the
    // command keys on the left -- see kLeftKeys/kRightKeys. n moved to
    // the left panel (next to cmd) and w to the right in the mode-key
    // relocation, so they're the two exceptions to that split.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& i = findKey(rightKeys, U"i");
    const KeyRect& c = findKey(leftKeys, U"c");
    const KeyRect& tab = findKey(leftKeys, U"tab");
    const KeyRect& w = findKey(rightKeys, U"w"); // an ordinary, wholly unmapped letter

    CHECK(commandLegendFor(i, /*isLinkMode=*/false) == U"up");
    CHECK(commandLegendFor(c, /*isLinkMode=*/false) == U"+card");
    CHECK_FALSE(commandLegendFor(tab, /*isLinkMode=*/false).has_value());
    CHECK_FALSE(commandLegendFor(w, /*isLinkMode=*/false).has_value());
}

TEST_CASE("commandLegendFor: navigation mode only describes its own live keys")
{
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& i = findKey(rightKeys, U"i");
    const KeyRect& e = findKey(leftKeys, U"e");   // no longer live in navigation mode -- cmd is the only way out
    const KeyRect& n = findKey(leftKeys, U"n");   // ditto
    const KeyRect& c = findKey(leftKeys, U"c");   // live in general command mode, blocked in navigation mode
    const KeyRect& u = findKey(rightKeys, U"u");  // ditto

    CHECK(commandLegendFor(i, /*isLinkMode=*/true) == U"prev");
    CHECK_FALSE(commandLegendFor(e, /*isLinkMode=*/true).has_value());
    CHECK_FALSE(commandLegendFor(n, /*isLinkMode=*/true).has_value());
    CHECK_FALSE(commandLegendFor(c, /*isLinkMode=*/true).has_value());
    CHECK_FALSE(commandLegendFor(u, /*isLinkMode=*/true).has_value());
}

TEST_CASE("commandLegendFor: cmd always describes itself, in either command sub-state")
{
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& cmd = findKey(leftKeys, U"cmd");

    CHECK(commandLegendFor(cmd, /*isLinkMode=*/false) == U"cmd");
    CHECK(commandLegendFor(cmd, /*isLinkMode=*/true) == U"cmd");
}

TEST_CASE("modeColorFor: cmd always shows its own color, in any mode")
{
    // Unlike n/e, cmd never produces literal text, so it has no
    // "ordinary key" state to fall back to in typing mode.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& cmd = findKey(leftKeys, U"cmd");

    for (bool isTypingMode : {true, false})
        for (bool isLinkMode : {true, false})
            CHECK(modeColorFor(cmd, isTypingMode, isLinkMode) == ModeColor::Command);
}

TEST_CASE("modeColorFor: typing mode colors every key Edit, cmd excepted -- n/e included")
{
    // The user was explicit: typing mode should read as "everything is
    // red except cmd" -- including shift/tab, which aren't Fire keys and
    // used to stay white. n/e are ordinary letters here too (this is
    // where they type a literal 'n'/'e') -- found live, not by
    // inspection: n showing its command-mode blue while typing read as
    // wrong the instant it was seen on screen, since it's just a letter
    // like any other in this mode.
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& q = findKey(keys, U"q");
    const KeyRect& e = findKey(keys, U"e");
    const KeyRect& spacebar = keys.back();
    const KeyRect& shift = findKey(keys, U"shift");
    const KeyRect& tab = findKey(keys, U"tab");
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& n = findKey(keys, U"n");
    const KeyRect& enter = findKey(rightKeys, U"enter");
    const KeyRect& bs = findKey(rightKeys, U"bs");

    CHECK(modeColorFor(q, /*isTypingMode=*/true, false) == ModeColor::Edit);
    CHECK(modeColorFor(e, /*isTypingMode=*/true, false) == ModeColor::Edit);
    CHECK(modeColorFor(n, /*isTypingMode=*/true, false) == ModeColor::Edit);
    CHECK(modeColorFor(spacebar, /*isTypingMode=*/true, false) == ModeColor::Edit);
    CHECK(modeColorFor(enter, /*isTypingMode=*/true, false) == ModeColor::Edit);
    CHECK(modeColorFor(bs, /*isTypingMode=*/true, false) == ModeColor::Edit);
    CHECK(modeColorFor(shift, /*isTypingMode=*/true, false) == ModeColor::Edit);
    CHECK(modeColorFor(tab, /*isTypingMode=*/true, false) == ModeColor::Edit);
}

TEST_CASE("modeColorFor: general command mode colors n/e their own permanent color")
{
    // In general command mode, n/e are mode-transition keys, not literal
    // text -- this is where they show their permanent identity color
    // instead.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& e = findKey(leftKeys, U"e");
    const KeyRect& n = findKey(leftKeys, U"n");

    CHECK(modeColorFor(e, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Edit);
    CHECK(modeColorFor(n, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Navigation);
}

TEST_CASE("modeColorFor: navigation mode blocks n/e -- neither keeps its color while dead")
{
    // Unlike cmd (always live, always its own color), n/e are ordinary
    // blocked keys in Navigation mode -- see cursor.cpp's exclusive-
    // navigation-mode gate -- so they show None like any other dead key,
    // not their permanent identity color. Found live, not by inspection:
    // a blank blue 'n' and a blank red 'e' both read as live when they
    // weren't.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& e = findKey(leftKeys, U"e");
    const KeyRect& n = findKey(leftKeys, U"n");

    CHECK(modeColorFor(e, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::None);
    CHECK(modeColorFor(n, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::None);
}

TEST_CASE("modeColorFor: command mode colors exactly the keys commandLegendFor lives for")
{
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& i = findKey(rightKeys, U"i");
    const KeyRect& u = findKey(rightKeys, U"u");
    const KeyRect& c = findKey(leftKeys, U"c");
    const KeyRect& tab = findKey(leftKeys, U"tab");

    // General command mode: Command for every live key.
    CHECK(modeColorFor(i, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Command);
    CHECK(modeColorFor(u, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Command);
    CHECK(modeColorFor(tab, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::None);

    // Navigation mode: Navigation for the keys still live there, None for
    // the ones blocked (see the exclusive-navigation-mode gate in
    // cursor.cpp) -- 'e'/'n' included now (see the "navigation mode
    // blocks n/e" test above), not permanent exceptions.
    CHECK(modeColorFor(i, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::Navigation);
    CHECK(modeColorFor(u, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::None);
    CHECK(modeColorFor(c, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::None);
}
