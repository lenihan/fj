#include "keyboardPanel.h"

#include "hackAtlas.h"
#include "layout.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace
{

// PLAN.md's "Ortholinear Keyboard" section's "Possible layout" sketch,
// transcribed one row array per side. Row 4 (the spacebar) isn't in here --
// it's handled separately in layoutKeys/drawKeyboardPanel since it spans
// two columns instead of one.
const std::array<std::array<const char32_t*, KeyboardPanel::kCols>, KeyboardPanel::kKeyRows> kLeftKeys = {{
    {U";", U"1", U"2", U"3", U"4", U"5"},
    {U"tab", U"q", U"w", U"e", U"r", U"t"},
    {U"caps", U"a", U"s", U"d", U"f", U"g"},
    {U"shift", U"z", U"x", U"c", U"v", U"b"},
}};

const std::array<std::array<const char32_t*, KeyboardPanel::kCols>, KeyboardPanel::kKeyRows> kRightKeys = {{
    {U"6", U"7", U"8", U"9", U"0", U"bs"},
    {U"y", U"u", U"i", U"o", U"p", U"-"},
    {U"h", U"j", U"k", U"l", U"'", U"enter"},
    {U"n", U"m", U",", U".", U"/", U"shift"},
}};

constexpr Pixel kPanelColor = 0x00202020;
constexpr Pixel kKeyColor = 0x00E8E4DA; // close to Card::kColor, so the whole device reads as one object
constexpr Pixel kKeyBorderColor = 0x00000000;
constexpr Pixel kKeyLabelColor = 0x00202020;

void drawBoxOutline(Canvas& canvas, Rect r, Pixel color, int thickness)
{
    canvas.line({r.x, r.y}, {r.x + r.w, r.y}, color, thickness);
    canvas.line({r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, color, thickness);
    canvas.line({r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}, color, thickness);
    canvas.line({r.x, r.y + r.h}, {r.x, r.y}, color, thickness);
}

// The longest single-width key label ("shift"/"enter", 5 codepoints) --
// pickPanelAtlas sizes text to fit this within one key's pitch, not the
// double-wide spacebar's more generous width.
constexpr std::size_t kLongestSingleWidthLabel = 5;

} // namespace

std::vector<KeyRect> layoutKeys(bool leftSide, int panelSize_px)
{
    std::vector<KeyRect> keys;
    keys.reserve(KeyboardPanel::kCols * KeyboardPanel::kKeyRows + 1);

    double pitchIn = KeyboardPanel::kKeyPitch_in;
    int pitch_px = static_cast<int>(pitchIn / KeyboardPanel::kWidth_in * panelSize_px);
    int gap_px = static_cast<int>(KeyboardPanel::kKeyGap_in / KeyboardPanel::kWidth_in * panelSize_px);

    int gridWidth_px = pitch_px * KeyboardPanel::kCols;
    int gridHeight_px = pitch_px * (KeyboardPanel::kKeyRows + 1); // +1 for the spacebar row
    int originX = (panelSize_px - gridWidth_px) / 2;
    int originY = (panelSize_px - gridHeight_px) / 2;

    const auto& labels = leftSide ? kLeftKeys : kRightKeys;
    for (int row = 0; row < KeyboardPanel::kKeyRows; ++row)
    {
        for (int col = 0; col < KeyboardPanel::kCols; ++col)
        {
            Rect cell{originX + col * pitch_px, originY + row * pitch_px, pitch_px, pitch_px};
            Rect key{cell.x + gap_px / 2, cell.y + gap_px / 2, cell.w - gap_px, cell.h - gap_px};
            keys.push_back({key, labels[row][col]});
        }
    }

    // Spacebar: double-wide, aligned to the panel's screen-side edge --
    // rightmost two columns on the left panel (nearest the screen to its
    // right), leftmost two columns on the right panel (nearest the screen
    // to its left).
    int spacebarCol = leftSide ? KeyboardPanel::kCols - 2 : 0;
    Rect spacebarCell{originX + spacebarCol * pitch_px, originY + KeyboardPanel::kKeyRows * pitch_px, pitch_px * 2,
                       pitch_px};
    Rect spacebarKey{spacebarCell.x + gap_px / 2, spacebarCell.y + gap_px / 2, spacebarCell.w - gap_px,
                      spacebarCell.h - gap_px};
    keys.push_back({spacebarKey, U"spacebar"});

    return keys;
}

const HackAtlas::Atlas& pickPanelAtlas(int panelSize_px)
{
    int pitch_px = static_cast<int>(KeyboardPanel::kKeyPitch_in / KeyboardPanel::kWidth_in * panelSize_px);
    // A bit of margin (kLongestSingleWidthLabel + 1) so "shift"/"enter"
    // don't render edge-to-edge against the key's own border.
    int desiredCellWidth_px = pitch_px / static_cast<int>(kLongestSingleWidthLabel + 1);
    return pickAtlas(desiredCellWidth_px);
}

void drawKeyboardPanel(Canvas& canvas, bool leftSide, const HackAtlas::Atlas& atlas)
{
    canvas.fillRect({0, 0, canvas.width(), canvas.height()}, kPanelColor);

    for (const KeyRect& key : layoutKeys(leftSide, canvas.width()))
    {
        canvas.fillRect(key.rect, kKeyColor);
        drawBoxOutline(canvas, key.rect, kKeyBorderColor, 1);

        int textWidth = static_cast<int>(key.label.size()) * atlas.cellWidth;
        Point textPos{key.rect.x + std::max(0, (key.rect.w - textWidth) / 2),
                      key.rect.y + std::max(0, (key.rect.h - atlas.cellHeight) / 2)};
        canvas.drawText(key.label, textPos, kKeyLabelColor, atlas);
    }
}
