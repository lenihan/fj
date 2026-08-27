// keyboardPanel.h -- one 5"x5" ortholinear keyboard half of the emulated
// device (see layout.h's KeyboardPanel/Device namespaces and PLAN.md's
// "Ortholinear Keyboard" section). Phase 1: static physical key labels
// (what's silkscreened on a real keycap). Phase 2: hitTestPanel resolves a
// click to the KeyEvent that key sends -- main.cpp owns turning a raw
// window-pixel click into a panelSize_px-relative position and calling
// cursor.handleKey with the result (see main.cpp's onClick). Mode-
// dependent dynamic legends are still a planned follow-up (see PLAN.md).
//
// Deliberately core, not platform code: same layer as cardItem.h, pure
// functions of (leftSide, panelSize_px[, pos]) with no Cursor/window
// dependency, so headlessly testable (see tests/keyboardPanelTests.cpp).

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

    // What clicking this key sends -- kind/codepoint match KeyEvent's own
    // fields exactly (main.cpp just copies them in, plus the click's
    // pressed edge). clickable is false for keys with no mapped action at
    // all yet (tab/shift -- see PLAN.md's Keyboard Mapping table: neither
    // is bound to anything in Cursor::handleKey today), so a click on one
    // is a deliberate no-op rather than e.g. typing a stray 't'/'s'.
    bool clickable{true};
    KeyEvent::Kind kind{KeyEvent::Kind::Char};
    char32_t codepoint{0}; // valid only when kind == Char
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

// Picks whichever baked atlas best fits the longest single-width key
// label (e.g. "shift", "enter") within one key's pitch at panelSize_px --
// see keyboardPanel.cpp. Exposed so main.cpp doesn't need to know that
// heuristic itself.
const HackAtlas::Atlas& pickPanelAtlas(int panelSize_px);

// Fills canvas (expected to already be panelSize_px x panelSize_px) with
// the panel's background and every key face + centered label.
void drawKeyboardPanel(Canvas& canvas, bool leftSide, const HackAtlas::Atlas& atlas);
