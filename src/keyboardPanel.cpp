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

// What clicking a key labeled `label` sends -- see KeyRect's comment.
// "tab"/"shift" have no corresponding Cursor::handleKey case at all today
// (PLAN.md's Keyboard Mapping table specs them, but nothing implements
// them yet -- see PLAN.md's Ortholinear Keyboard section), so a click on
// either is a deliberate no-op rather than guessing a mapping that isn't
// real. Every other key already has a well-defined physical meaning:
// "caps" is the one key whose press AND release both matter (mirroring
// KeyEvent::Kind::CapsLock -- see platform.h), "enter"/"bs" fire once on
// press only (matching every real keyboard's Kind::Enter/Backspace, which
// only ever arrive as a single press-only event -- see win32Window.cpp's
// WM_KEYDOWN handling), "spacebar" sends the literal space character, and
// every remaining single-codepoint label is exactly the character it
// shows.
void fillKeyEvent(KeyRect& key)
{
    if (key.label == U"tab" || key.label == U"shift")
    {
        key.clickable = false;
    }
    else if (key.label == U"caps")
    {
        key.kind = KeyEvent::Kind::CapsLock;
    }
    else if (key.label == U"enter")
    {
        key.kind = KeyEvent::Kind::Enter;
    }
    else if (key.label == U"bs")
    {
        key.kind = KeyEvent::Kind::Backspace;
    }
    else if (key.label == U"spacebar")
    {
        key.kind = KeyEvent::Kind::Char;
        key.codepoint = U' ';
    }
    else
    {
        key.kind = KeyEvent::Kind::Char;
        key.codepoint = key.label.front(); // every other label is exactly one codepoint
    }
}

bool contains(Rect r, Point p)
{
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

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
            KeyRect keyRect{key, labels[row][col]};
            fillKeyEvent(keyRect);
            keys.push_back(std::move(keyRect));
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
    KeyRect spacebar{spacebarKey, U"spacebar"};
    fillKeyEvent(spacebar);
    keys.push_back(std::move(spacebar));

    return keys;
}

std::optional<KeyRect> hitTestPanel(bool leftSide, int panelSize_px, Point pos)
{
    for (const KeyRect& key : layoutKeys(leftSide, panelSize_px))
    {
        if (contains(key.rect, pos))
            return key;
    }
    return std::nullopt;
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
