// keyboardPanel.h -- one 5"x5" ortholinear keyboard half of the emulated
// device (see layout.h's KeyboardPanel/Device namespaces and PLAN.md's
// "Ortholinear Keyboard" section). Phase 1 only: static physical key
// labels (what's silkscreened on a real keycap), no click handling, no
// mode-dependent legends yet -- both are planned follow-ups (see PLAN.md).
//
// Deliberately core, not platform code: same layer as cardItem.h, a pure
// function of (leftSide, panelSize_px) with no Cursor/window dependency,
// so it's headlessly testable (see tests/keyboardPanelTests.cpp).

#pragma once

#include "canvas.h"

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
};

// One entry per key: 4 rows x KeyboardPanel::kCols single keys, plus one
// double-wide spacebar key aligned to the panel's screen-side edge.
// panelSize_px is the panel's rendered size in pixels (always square --
// KeyboardPanel::kWidth_in == kHeight_in).
std::vector<KeyRect> layoutKeys(bool leftSide, int panelSize_px);

// Picks whichever baked atlas best fits the longest single-width key
// label (e.g. "shift", "enter") within one key's pitch at panelSize_px --
// see keyboardPanel.cpp. Exposed so main.cpp doesn't need to know that
// heuristic itself.
const HackAtlas::Atlas& pickPanelAtlas(int panelSize_px);

// Fills canvas (expected to already be panelSize_px x panelSize_px) with
// the panel's background and every key face + centered label.
void drawKeyboardPanel(Canvas& canvas, bool leftSide, const HackAtlas::Atlas& atlas);
