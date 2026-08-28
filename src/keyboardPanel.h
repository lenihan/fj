// keyboardPanel.h -- one 5"x5" ortholinear keyboard half of the emulated
// device (see layout.h's KeyboardPanel/Device namespaces and PLAN.md's
// "Ortholinear Keyboard" section). Phase 1: static physical key labels
// (what's silkscreened on a real keycap). Phase 2: hitTestPanel resolves a
// click to a key, and resolveKeyGesture turns a press/release pair into
// the KeyEvents Cursor should see -- main.cpp owns turning raw window-
// pixel clicks into panelSize_px-relative positions and replaying the
// result through cursor.handleKey (see main.cpp's onClick). Mode-
// dependent dynamic legends are still a planned follow-up (see PLAN.md).
//
// Deliberately core, not platform code: same layer as cardItem.h, pure
// functions with no Cursor/window dependency, so headlessly testable (see
// tests/keyboardPanelTests.cpp).

#pragma once

#include "canvas.h"

#include <optional>
#include <string>
#include <vector>

namespace HackAtlas
{
struct Atlas;
}

struct KeyRect
{
    Rect rect;
    std::u32string label;

    enum class Action
    {
        // A plain tap (press+release on this same key) sends
        // {kind, codepoint, true} directly, matching a physical keypress.
        Fire,

        // caps: a plain tap toggles a persistent latch (mirroring a real
        // Caps Lock's own latch), shown inverted the whole time it's on.
        // A press-drag-release chord that starts here and ends on a Fire
        // key applies command mode to that one key only, then reverts --
        // mirroring "hold caps, tap a command key, release caps" on a
        // real keyboard, which a single mouse/touch point can't do by
        // holding (see resolveKeyGesture's comment for why a drag is the
        // mouse/touch equivalent).
        CapsToggle,

        // shift: same two gestures as caps (tap toggles a latch, chord
        // applies to one key), but has no KeyEvent of its own -- real
        // Shift is a pure per-keypress case modifier, not a Cursor mode,
        // so resolveKeyGesture case-transforms the chorded/typed key
        // directly instead of sending anything shift-specific. Appears on
        // both panels (mirroring a real keyboard's two shift keys),
        // sharing one latch.
        ShiftToggle,

        // No mapped action at all yet (tab -- see PLAN.md's Keyboard
        // Mapping table: Cursor::handleKey doesn't implement it). Still
        // visually pressable, just does nothing.
        None,
    };
    Action action{Action::Fire};
    KeyEvent::Kind kind{KeyEvent::Kind::Char}; // valid when action == Fire
    char32_t codepoint{0};                     // valid when action == Fire and kind == Char
};

// One entry per key: 4 rows x KeyboardPanel::kCols single keys, plus one
// double-wide spacebar key aligned to the panel's screen-side edge.
// panelSize_px is the panel's rendered size in pixels (always square --
// KeyboardPanel::kWidth_in == kHeight_in).
std::vector<KeyRect> layoutKeys(bool leftSide, int panelSize_px);

// Which key (if any) contains pos, in the same panelSize_px-relative
// coordinate space layoutKeys itself uses -- nullopt for the gaps between
// keycaps or outside the grid entirely. Recomputes layoutKeys internally;
// cheap (a couple dozen rects), not worth caching.
std::optional<KeyRect> hitTestPanel(bool leftSide, int panelSize_px, Point pos);

struct GestureOutcome
{
    std::vector<KeyEvent> events; // replay through Cursor::handleKey, in order
    bool capsLatched;             // caps latch state *after* this gesture
    bool shiftLatched;            // shift latch state *after* this gesture
};

// Resolves one press/release pair into what Cursor should see. pressedKey/
// pressedLeftSide is the key a press landed on -- callers only invoke this
// when a press actually landed on a key, never for a press on empty space.
// releasedKey/releasedLeftSide is the key the matching release landed on,
// if any (none, the same key as the press, or a different key).
// capsLatchedBefore/shiftLatchedBefore is the latch state going into this
// gesture.
//
// Same key released as pressed -> a plain tap: Fire keys send their own
// event; caps/shift toggle their latch. A *different* key released is a
// press-drag-release chord -- the mouse/touch equivalent of physically
// holding one key while tapping another, which a single pointer can't do
// by holding (see KeyRect::Action's comment): caps/shift apply their
// effect to the released key for that one keypress only, then the latch
// reverts to whatever it was before the gesture (the gesture itself never
// changes the persistent latch -- see keyboardPanel.cpp for exactly why
// caps's chord is handled differently depending on whether it was already
// latched, and shift's isn't).
GestureOutcome resolveKeyGesture(const KeyRect& pressedKey, bool pressedLeftSide,
                                  const std::optional<KeyRect>& releasedKey, bool releasedLeftSide,
                                  bool capsLatchedBefore, bool shiftLatchedBefore);

// Picks whichever baked atlas best fits the longest single-width key
// label (e.g. "shift", "enter") within one key's pitch at panelSize_px --
// see keyboardPanel.cpp. Exposed so main.cpp doesn't need to know that
// heuristic itself.
const HackAtlas::Atlas& pickPanelAtlas(int panelSize_px);

// Fills canvas (expected to already be panelSize_px x panelSize_px) with
// the panel's background and every key face + centered label. A key draws
// color-inverted (face/label swapped) if its rect matches pressedKeyRect
// (currently mid-press, any key -- the caller only passes a non-null
// pointer to the side actually holding the pressed key), or it's
// CapsToggle/ShiftToggle and the matching latch is on -- the latter needs
// no rect-matching since this function already knows each key's action,
// so both shift keys light up together automatically.
void drawKeyboardPanel(Canvas& canvas, bool leftSide, const HackAtlas::Atlas& atlas,
                        const Rect* pressedKeyRect = nullptr, bool capsLatched = false,
                        bool shiftLatched = false);
