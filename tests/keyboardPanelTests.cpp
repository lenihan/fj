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

TEST_CASE("every letter sits at its real-QWERTY grid position, matching typing-mode muscle memory")
{
    // An earlier version of kLeftKeys/kRightKeys physically relocated
    // c/t/d/e/n (and, as a knock-on, q/w/a/s) so +card/+toc/del/edit/nav
    // would sit spatially close to cmd -- reverted (see kLeftKeys' own
    // comment): the user was explicit that only a key's *command-mode
    // function* should ever move, never the letter it produces, since
    // there's no separate "command identity" independent of that one
    // codepoint (Cursor dispatches command mode on the exact same
    // codepoint typing mode sends). This test locks each letter to its
    // real keyboard row/col so that mistake can't quietly come back --
    // checked by row/col position, not just "is it findable somewhere,"
    // since a findKey() lookup alone can't tell a relocated key from an
    // unmoved one.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    auto at = [](const std::vector<KeyRect>& keys, int row, int col) -> const KeyRect&
    { return keys.at(static_cast<std::size_t>(row * KeyboardPanel::kCols + col)); };

    // Left panel: row 1 "q w e r t", row 2 "a s d f g", row 3 "z x c v b"
    // (row 0 is ";1234 5", not a letter row).
    CHECK(at(leftKeys, 1, 1).label == U"q");
    CHECK(at(leftKeys, 1, 2).label == U"w");
    CHECK(at(leftKeys, 1, 3).label == U"e");
    CHECK(at(leftKeys, 2, 1).label == U"a");
    CHECK(at(leftKeys, 2, 2).label == U"s");
    CHECK(at(leftKeys, 2, 3).label == U"d");
    CHECK(at(leftKeys, 3, 1).label == U"z");
    CHECK(at(leftKeys, 3, 2).label == U"x");
    CHECK(at(leftKeys, 3, 3).label == U"c");

    // Right panel: row 1 "y u i o p", row 3 "n m , . /".
    CHECK(at(rightKeys, 1, 0).label == U"y");
    CHECK(at(rightKeys, 3, 0).label == U"n");
    CHECK(at(rightKeys, 3, 1).label == U"m");
}

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
    // i/k/j/l/u/o/m/./n/h/y/p live on the right panel (n included -- see
    // kRightKeys), the rest of the command keys on the left. q (+card)
    // sits at its real-QWERTY spot, not c's -- see kLeftKeys' own
    // comment on why only a key's *function* moved, never its letter.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& i = findKey(rightKeys, U"i");
    const KeyRect& q = findKey(leftKeys, U"q");
    const KeyRect& tab = findKey(leftKeys, U"tab");
    const KeyRect& c = findKey(leftKeys, U"c"); // an ordinary, wholly unmapped letter now

    CHECK(commandLegendFor(i, /*isLinkMode=*/false) == U"↑");
    CHECK(commandLegendFor(q, /*isLinkMode=*/false) == U"+card");
    CHECK_FALSE(commandLegendFor(tab, /*isLinkMode=*/false).has_value());
    CHECK_FALSE(commandLegendFor(c, /*isLinkMode=*/false).has_value());
}

TEST_CASE("commandLegendFor: navigation mode only describes its own live keys")
{
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& i = findKey(rightKeys, U"i");
    const KeyRect& a = findKey(leftKeys, U"a");   // no longer live in navigation mode -- cmd is the only way out
    const KeyRect& s = findKey(leftKeys, U"s");   // ditto
    const KeyRect& q = findKey(leftKeys, U"q");   // live in general command mode, blocked in navigation mode
    const KeyRect& u = findKey(rightKeys, U"u");  // ditto

    CHECK(commandLegendFor(i, /*isLinkMode=*/true) == U"prev");
    CHECK_FALSE(commandLegendFor(a, /*isLinkMode=*/true).has_value());
    CHECK_FALSE(commandLegendFor(s, /*isLinkMode=*/true).has_value());
    CHECK_FALSE(commandLegendFor(q, /*isLinkMode=*/true).has_value());
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
    // Unlike s/a, cmd never produces literal text, so it has no
    // "ordinary key" state to fall back to in typing mode.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& cmd = findKey(leftKeys, U"cmd");

    for (bool isTypingMode : {true, false})
        for (bool isLinkMode : {true, false})
            CHECK(modeColorFor(cmd, isTypingMode, isLinkMode) == ModeColor::Command);
}

TEST_CASE("modeColorFor: typing mode colors every key some shade of red, cmd excepted -- s/a included")
{
    // The user was explicit: typing mode should read as "everything is
    // some shade of red except cmd" -- including shift/tab, which aren't
    // Fire keys and used to stay white -- with four tiers: letters
    // brightest (EditLetter), digits a shade less (EditNumber),
    // punctuation/spacebar less still (EditOther), and tab/shift/enter/bs
    // least of all (EditControl -- control keys, not text you're
    // producing). s/a (nav/edit's own keys) are ordinary letters here too
    // (this is where they type a literal 's'/'a') -- found live, not by
    // inspection (against an earlier version of this mapping, where nav
    // lived on 'n'): showing its command-mode blue while typing read as
    // wrong the instant it was seen on screen, since it's just a letter
    // like any other in this mode.
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& q = findKey(keys, U"q");
    const KeyRect& a = findKey(keys, U"a");
    const KeyRect& one = findKey(keys, U"1");
    const KeyRect& spacebar = keys.back();
    const KeyRect& shift = findKey(keys, U"shift");
    const KeyRect& tab = findKey(keys, U"tab");
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& enter = findKey(rightKeys, U"enter");
    const KeyRect& bs = findKey(rightKeys, U"bs");
    const KeyRect& comma = findKey(rightKeys, U",");

    CHECK(modeColorFor(q, /*isTypingMode=*/true, false) == ModeColor::EditLetter);
    CHECK(modeColorFor(a, /*isTypingMode=*/true, false) == ModeColor::EditLetter);
    CHECK(modeColorFor(one, /*isTypingMode=*/true, false) == ModeColor::EditNumber);
    CHECK(modeColorFor(spacebar, /*isTypingMode=*/true, false) == ModeColor::EditOther);
    CHECK(modeColorFor(comma, /*isTypingMode=*/true, false) == ModeColor::EditOther);
    CHECK(modeColorFor(enter, /*isTypingMode=*/true, false) == ModeColor::EditControl);
    CHECK(modeColorFor(bs, /*isTypingMode=*/true, false) == ModeColor::EditControl);
    CHECK(modeColorFor(shift, /*isTypingMode=*/true, false) == ModeColor::EditControl);
    CHECK(modeColorFor(tab, /*isTypingMode=*/true, false) == ModeColor::EditControl);
}

TEST_CASE("modeColorFor: general command mode colors s/a their own permanent color")
{
    // In general command mode, s/a are mode-transition keys, not literal
    // text -- this is where they show their permanent identity color
    // instead.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& a = findKey(leftKeys, U"a");
    const KeyRect& s = findKey(leftKeys, U"s");

    CHECK(modeColorFor(a, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Edit);
    CHECK(modeColorFor(s, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Navigation);
}

TEST_CASE("modeColorFor: KeyDisabledState grays out a/e/m/./u/o in general command mode")
{
    // Driven by CardItem::canEdit()/canDelete() and Cursor::
    // hasPrevThreadCard()/hasNextThreadCard()/isAtFirstCard()/
    // isAtLastCard() (main.cpp's job to query) -- each key still shows a
    // legend (commandLegendFor doesn't care), just not the color
    // suggesting they'd do something right now.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& a = findKey(leftKeys, U"a");
    const KeyRect& e = findKey(leftKeys, U"e");
    const KeyRect& m = findKey(rightKeys, U"m"); // right panel -- see kRightKeys
    const KeyRect& dot = findKey(rightKeys, U".");
    const KeyRect& u = findKey(rightKeys, U"u");
    const KeyRect& o = findKey(rightKeys, U"o");

    CHECK(modeColorFor(a, false, false, KeyDisabledState{.editDisabled = true}) == ModeColor::Disabled);
    CHECK(modeColorFor(e, false, false, KeyDisabledState{.deleteDisabled = true}) == ModeColor::Disabled);
    CHECK(modeColorFor(m, false, false, KeyDisabledState{.prevThreadDisabled = true}) == ModeColor::Disabled);
    CHECK(modeColorFor(dot, false, false, KeyDisabledState{.nextThreadDisabled = true}) == ModeColor::Disabled);
    CHECK(modeColorFor(u, false, false, KeyDisabledState{.prevCardDisabled = true}) == ModeColor::Disabled);
    CHECK(modeColorFor(o, false, false, KeyDisabledState{.nextCardDisabled = true}) == ModeColor::Disabled);

    // Not disabled -- back to their ordinary colors (`{}` -- every field
    // false -- matching every other call site in this file).
    CHECK(modeColorFor(a, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Edit);
    CHECK(modeColorFor(e, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Command);
    CHECK(modeColorFor(u, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Command);

    // Irrelevant in typing mode -- a is an ordinary red letter there
    // regardless.
    CHECK(modeColorFor(a, true, false, KeyDisabledState{.editDisabled = true}) == ModeColor::EditLetter);
}

TEST_CASE("modeColorFor: KeyDisabledState grays out the arrows in general command mode when read-only")
{
    // A deliberate choice, not an oversight -- see KeyDisabledState's own
    // comment in keyboardPanel.h: general command mode's arrows exist to
    // position the cursor for editing, which is meaningless on a card you
    // can't edit anyway.
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& i = findKey(rightKeys, U"i");
    const KeyRect& k = findKey(rightKeys, U"k");
    const KeyRect& j = findKey(rightKeys, U"j");
    const KeyRect& l = findKey(rightKeys, U"l");
    KeyDisabledState readOnly{.editDisabled = true};

    CHECK(modeColorFor(i, false, /*isLinkMode=*/false, readOnly) == ModeColor::Disabled);
    CHECK(modeColorFor(k, false, /*isLinkMode=*/false, readOnly) == ModeColor::Disabled);
    CHECK(modeColorFor(j, false, /*isLinkMode=*/false, readOnly) == ModeColor::Disabled);
    CHECK(modeColorFor(l, false, /*isLinkMode=*/false, readOnly) == ModeColor::Disabled);

    // Not read-only -- back to Command.
    CHECK(modeColorFor(i, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Command);

    // Irrelevant in Navigation mode -- i/k/j mean link navigation there,
    // gated by prevDisabled/nextDisabled/backDisabled instead (see the
    // next test), not editDisabled.
    CHECK(modeColorFor(i, false, /*isLinkMode=*/true, readOnly) == ModeColor::Navigation);
}

TEST_CASE("modeColorFor: backDisabled/prevDisabled/nextDisabled independently gray out j/i/k in navigation mode")
{
    // Driven by Cursor::hasLinkHistory()/CardItem::isAtFirstLink()/
    // isAtLastLink() (main.cpp's job to query) -- each key still shows a
    // legend (commandLegendFor doesn't care), just not the color
    // suggesting they'd do something right now. i/k are independent: a
    // card with several links only disables whichever end you're
    // actually sitting at, not both just because there's more than one.
    // Irrelevant outside Navigation mode, same as the general-command
    // disabled states above.
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& i = findKey(rightKeys, U"i");
    const KeyRect& k = findKey(rightKeys, U"k");
    const KeyRect& j = findKey(rightKeys, U"j");

    CHECK(modeColorFor(j, false, /*isLinkMode=*/true, KeyDisabledState{.backDisabled = true}) ==
          ModeColor::Disabled);
    CHECK(modeColorFor(i, false, /*isLinkMode=*/true, KeyDisabledState{.prevDisabled = true}) ==
          ModeColor::Disabled);
    CHECK(modeColorFor(k, false, /*isLinkMode=*/true, KeyDisabledState{.nextDisabled = true}) ==
          ModeColor::Disabled);

    // i's own disabled state doesn't leak onto k, or vice versa.
    CHECK(modeColorFor(k, false, /*isLinkMode=*/true, KeyDisabledState{.prevDisabled = true}) ==
          ModeColor::Navigation);
    CHECK(modeColorFor(i, false, /*isLinkMode=*/true, KeyDisabledState{.nextDisabled = true}) ==
          ModeColor::Navigation);

    // Not disabled -- back to Navigation's ordinary color.
    CHECK(modeColorFor(j, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::Navigation);
    CHECK(modeColorFor(i, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::Navigation);

    // Irrelevant in general command mode -- i/k/j mean cursor movement
    // there, not link navigation, regardless of any of these flags.
    CHECK(modeColorFor(j, false, /*isLinkMode=*/false, KeyDisabledState{.backDisabled = true}) ==
          ModeColor::Command);
}

TEST_CASE("modeColorFor: navigation mode blocks s/a -- neither keeps its color while dead")
{
    // Unlike cmd (always live, always its own color), s/a are ordinary
    // blocked keys in Navigation mode -- see cursor.cpp's exclusive-
    // navigation-mode gate -- so they show None like any other dead key,
    // not their permanent identity color. Found live, not by inspection
    // (against an earlier version of this mapping, where nav lived on
    // 'n'): a blank blue 'n' and a blank red 'e' both read as live when
    // they weren't.
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& a = findKey(leftKeys, U"a");
    const KeyRect& s = findKey(leftKeys, U"s");

    CHECK(modeColorFor(a, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::None);
    CHECK(modeColorFor(s, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::None);
}

TEST_CASE("modeColorFor: command mode colors exactly the keys commandLegendFor lives for")
{
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    const KeyRect& i = findKey(rightKeys, U"i");
    const KeyRect& u = findKey(rightKeys, U"u");
    const KeyRect& q = findKey(leftKeys, U"q");
    const KeyRect& tab = findKey(leftKeys, U"tab");

    // General command mode: Command for every live key.
    CHECK(modeColorFor(i, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Command);
    CHECK(modeColorFor(u, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::Command);
    CHECK(modeColorFor(tab, /*isTypingMode=*/false, /*isLinkMode=*/false) == ModeColor::None);

    // Navigation mode: Navigation for the keys still live there, None for
    // the ones blocked (see the exclusive-navigation-mode gate in
    // cursor.cpp) -- 's'/'a' included now (see the "navigation mode
    // blocks s/a" test above), not permanent exceptions.
    CHECK(modeColorFor(i, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::Navigation);
    CHECK(modeColorFor(u, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::None);
    CHECK(modeColorFor(q, /*isTypingMode=*/false, /*isLinkMode=*/true) == ModeColor::None);
}
