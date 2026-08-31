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
// gesture. isTypingMode is Cursor::isTypingMode() at the moment of the
// release -- see below.
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
//
// isTypingMode gates shift's case/symbol transform (see kShiftedSymbols
// below): applying it unconditionally would hand Cursor's command-mode
// switch an uppercase letter or a symbol it has no case for, silently
// breaking every command while shift happens to be latched -- a real bug
// found designing phase 3's typing-mode legend preview, not a
// hypothetical. Outside typing mode, a Fire key's codepoint always passes
// through unchanged regardless of shift.
GestureOutcome resolveKeyGesture(const KeyRect& pressedKey, bool pressedLeftSide,
                                  const std::optional<KeyRect>& releasedKey, bool releasedLeftSide,
                                  bool capsLatchedBefore, bool shiftLatchedBefore, bool isTypingMode);

// Picks whichever baked atlas best fits the longest single-width key
// label/legend within one key's pitch at panelSize_px -- see
// keyboardPanel.cpp. Exposed so main.cpp doesn't need to know that
// heuristic itself.
const HackAtlas::Atlas& pickPanelAtlas(int panelSize_px);

// What a key displays in typing mode: its physical label, unless it's a
// Fire+Char key and shiftEngaged, in which case the shifted form --
// letters uppercased, digits/punctuation mapped through the same
// kShiftedSymbols table resolveKeyGesture uses, matching a real keyboard
// (PLAN.md/the user: "capital letters and the numbers give the symbols
// instead").
std::u32string typingLabelFor(const KeyRect& key, bool shiftEngaged);

// What a key displays in command mode: a short description if it's live
// right now, nullopt if it isn't (drawKeyboardPanel renders those blank).
// cmd always describes itself, in either sub-state (it's how you back out
// one level -- see cursor.cpp's CapsLock-release branch). isLinkMode
// selects between the two command sub-states -- Navigation mode (Link)
// only considers i/k/j/l live (matching handleKey's own exclusive-
// navigation-mode gate: n/e are ordinary blocked keys there now, not
// exceptions -- see cursor.cpp); general command mode (Cursor) considers
// every key handleKey's switch implements live.
std::optional<std::u32string> commandLegendFor(const KeyRect& key, bool isLinkMode);

// Background tint a key should show, both for the three mode keys
// themselves (always, regardless of current mode) and for whichever
// other keys are live in the *current* mode -- see modeColorFor.
enum class ModeColor
{
    None,       // white/default -- no mode, or not live in the current one
    Edit,       // e -- red
    Command,    // cmd -- green (general command mode: i/k/j/l move the cursor)
    Navigation, // n -- blue (Navigation sub-mode: i/k/j/l select/follow links)
};

// cmd always shows its own fixed Command color regardless of mode -- it
// never produces literal text, so it has no "ordinary key" state to fall
// back to. In typing mode, every *other* key shows Edit, full stop --
// the user was explicit that typing mode should read as "everything is
// red except cmd," not just the keys that do something while typing --
// including n/e, which are ordinary letters here (this is where they
// type a literal 'n'/'e'), not mode keys. In general command mode, n/e
// switch to their own permanent color (Navigation/Edit) instead, since
// they're not producing text there; in Navigation mode they're blocked
// like any other non-i/k/j/l key (see cursor.cpp), so they fall through
// to the same live/blank check as everything else instead of keeping
// their color while dead. Every other key shows Command (general) or
// Navigation (isLinkMode) exactly when commandLegendFor returns a legend
// for it, None (the default background) otherwise. shift is deliberately
// excluded throughout -- it keeps its existing press/latch-only visual,
// not a mode color of its own.
ModeColor modeColorFor(const KeyRect& key, bool isTypingMode, bool isLinkMode);

// Fills canvas (expected to already be panelSize_px x panelSize_px) with
// the panel's background and every key face + label. A key draws color-
// inverted (face/label swapped) if its rect matches pressedKeyRect
// (currently mid-press, any key -- the caller only passes a non-null
// pointer to the side actually holding the pressed key), or it's
// ShiftToggle and shiftEngaged -- the latter needs no rect-matching since
// this function already knows each key's action, so both shift keys
// light up together automatically, for as long as the latch is on. cmd
// deliberately isn't part of that second case even though it's the same
// kind of toggle: it only inverts while pressedKeyRect matches it (a
// momentary flash on press and again on release), never for a lasting
// "the latch is on" indicator the way shift does -- cmd already has its
// own permanent Command-colored face (modeColorFor) for that, so a
// lasting invert on top would be redundant. The caller's own capsLatched
// state still matters for other things (e.g. resolveKeyGesture's chord
// logic); it just isn't a rendering input here anymore.
//
// isTypingMode/isLinkMode/shiftEngaged pick which of typingLabelFor/
// commandLegendFor supplies each key's text (see main.cpp's onClick/
// renderContent for why these are computed live, mid-press, rather than
// only once a gesture resolves): typingLabelFor while isTypingMode, else
// commandLegendFor -- a key with no legend at all (nullopt, command mode
// only) draws its face and border with no text, not even blank space
// reserved for it.
//
// hoveredKeyRect draws a brightened border around whichever key it
// matches, on top of everything else -- purely a "the pointer is here"
// affordance, independent of press/latch/mode state and applied to every
// key uniformly, dead ones included (the user was explicit: hovering a
// key that doesn't do anything should still show the hover, since it's
// about the pointer, not the key's function). Deliberately a border, not
// a face/text swap like pressedKeyRect/latching use -- those already mean
// "this key's state changed"; hover needing its own distinct visual
// keeps it from being confused with either.
void drawKeyboardPanel(Canvas& canvas, bool leftSide, const HackAtlas::Atlas& atlas,
                        const Rect* pressedKeyRect = nullptr, const Rect* hoveredKeyRect = nullptr,
                        bool shiftEngaged = false, bool isTypingMode = true, bool isLinkMode = false);
