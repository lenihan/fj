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

// The longest single-width key label/legend ("shift"/"enter"/"prevT"/
// "nextT", 5-6 codepoints) -- pickPanelAtlas sizes text to fit this
// within one key's pitch, not the double-wide spacebar's more generous
// width.
constexpr std::size_t kLongestSingleWidthLabel = 6;

// What a key labeled `label` does -- see KeyRect::Action's comment.
// "tab" has no corresponding Cursor::handleKey case at all today (PLAN.md's
// Keyboard Mapping table specs it, but nothing implements it yet -- see
// PLAN.md's Ortholinear Keyboard section), so it's a deliberate no-op
// rather than guessing a mapping that isn't real. Every other key already
// has a well-defined physical meaning: "caps"/"shift" get the two-gesture
// toggle-or-chord treatment (resolveKeyGesture), "enter"/"bs" fire once on
// a plain tap (matching every real keyboard's Kind::Enter/Backspace, which
// only ever arrive as a single press-only event -- see win32Window.cpp's
// WM_KEYDOWN handling), "spacebar" sends the literal space character, and
// every remaining single-codepoint label is exactly the character it
// shows.
void fillKeyEvent(KeyRect& key)
{
    if (key.label == U"tab")
    {
        key.action = KeyRect::Action::None;
    }
    else if (key.label == U"shift")
    {
        key.action = KeyRect::Action::ShiftToggle;
    }
    else if (key.label == U"caps")
    {
        key.action = KeyRect::Action::CapsToggle;
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

bool sameRect(Rect a, Rect b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

// Standard US-QWERTY shift mapping: letters uppercase, every digit/
// punctuation key this layout actually has gets its real shifted symbol
// (matches a normal keyboard -- the user was explicit that shift+number
// should give the symbol, not just capitalize letters). Keys with no
// shifted form of their own (space, and anything not looked up here)
// pass through unchanged.
char32_t shiftTransform(char32_t c)
{
    if (c >= U'a' && c <= U'z')
        return c - (U'a' - U'A');
    switch (c)
    {
    case U';': return U':';
    case U'1': return U'!';
    case U'2': return U'@';
    case U'3': return U'#';
    case U'4': return U'$';
    case U'5': return U'%';
    case U'6': return U'^';
    case U'7': return U'&';
    case U'8': return U'*';
    case U'9': return U'(';
    case U'0': return U')';
    case U'-': return U'_';
    case U'\'': return U'"';
    case U',': return U'<';
    case U'.': return U'>';
    case U'/': return U'?';
    default: return c;
    }
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

GestureOutcome resolveKeyGesture(const KeyRect& pressedKey, bool pressedLeftSide,
                                  const std::optional<KeyRect>& releasedKey, bool releasedLeftSide,
                                  bool capsLatchedBefore, bool shiftLatchedBefore, bool isTypingMode)
{
    GestureOutcome outcome{{}, capsLatchedBefore, shiftLatchedBefore};

    bool samePress = releasedKey.has_value() && releasedLeftSide == pressedLeftSide &&
                      sameRect(releasedKey->rect, pressedKey.rect);

    switch (pressedKey.action)
    {
    case KeyRect::Action::Fire:
        if (samePress)
        {
            char32_t codepoint = (pressedKey.kind == KeyEvent::Kind::Char && shiftLatchedBefore && isTypingMode)
                                      ? shiftTransform(pressedKey.codepoint)
                                      : pressedKey.codepoint;
            outcome.events.push_back({pressedKey.kind, codepoint, true});
        }
        // Dragged off before releasing: cancel, no events.
        break;

    case KeyRect::Action::CapsToggle:
        if (samePress)
        {
            outcome.capsLatched = !capsLatchedBefore;
            outcome.events.push_back({KeyEvent::Kind::CapsLock, 0, outcome.capsLatched});
        }
        else if (releasedKey && releasedKey->action == KeyRect::Action::Fire)
        {
            // Always the chorded key's own codepoint, unchanged -- a caps
            // chord always results in command-mode dispatch
            // (Cursor::handleKey's switch), which only matches lowercase.
            // Shift is meaningless here regardless of isTypingMode: it
            // only ever describes what a *typed* character would be.
            if (capsLatchedBefore)
            {
                // Already in command mode via the persistent latch --
                // fire the chorded key directly. Sending a redundant
                // CapsLock press/release pair here would desync Cursor's
                // own m_capsDown from our tracked latch (its release
                // branch unconditionally clears m_capsDown -- see
                // cursor.cpp), even though the latch itself should stay
                // on.
                outcome.events.push_back({releasedKey->kind, releasedKey->codepoint, true});
            }
            else
            {
                // "Hold caps, tap a command key, release caps" in one
                // drag -- verified sequence (phase 2's caps -> c -> caps
                // -> q testing), just triggered by one gesture instead of
                // two separate taps. capsLatched stays false: this
                // gesture never touches the persistent latch, only
                // Cursor's momentary mode.
                outcome.events.push_back({KeyEvent::Kind::CapsLock, 0, true});
                outcome.events.push_back({releasedKey->kind, releasedKey->codepoint, true});
                outcome.events.push_back({KeyEvent::Kind::CapsLock, 0, false});
            }
        }
        // Released off any key, or on None/ShiftToggle: cancel.
        break;

    case KeyRect::Action::ShiftToggle:
        if (samePress)
        {
            outcome.shiftLatched = !shiftLatchedBefore;
        }
        else if (releasedKey && releasedKey->action == KeyRect::Action::Fire &&
                 releasedKey->kind == KeyEvent::Kind::Char)
        {
            // Always shift-transformed when isTypingMode, regardless of
            // shiftLatchedBefore -- shift has no Cursor-side state to
            // preserve the way caps does, so there's nothing to make
            // conditional on the latch: the outcome of chording onto a
            // Char key is simply "shifted," full stop. Gated on
            // isTypingMode for the same reason as the Fire case above:
            // outside typing mode this key will be dispatched as a
            // command, which only matches its plain lowercase codepoint.
            char32_t codepoint = isTypingMode ? shiftTransform(releasedKey->codepoint) : releasedKey->codepoint;
            outcome.events.push_back({KeyEvent::Kind::Char, codepoint, true});
        }
        // Non-Char Fire keys, or released off any key: cancel.
        break;

    case KeyRect::Action::None:
        break;
    }

    return outcome;
}

const HackAtlas::Atlas& pickPanelAtlas(int panelSize_px)
{
    int pitch_px = static_cast<int>(KeyboardPanel::kKeyPitch_in / KeyboardPanel::kWidth_in * panelSize_px);
    // A bit of margin (kLongestSingleWidthLabel + 1) so "shift"/"enter"/a
    // command legend like "prevT" don't render edge-to-edge against the
    // key's own border.
    int desiredCellWidth_px = pitch_px / static_cast<int>(kLongestSingleWidthLabel + 1);
    return pickAtlas(desiredCellWidth_px);
}

std::u32string typingLabelFor(const KeyRect& key, bool shiftEngaged)
{
    if (shiftEngaged && key.action == KeyRect::Action::Fire && key.kind == KeyEvent::Kind::Char)
    {
        char32_t shifted = shiftTransform(key.codepoint);
        if (shifted != key.codepoint)
            return std::u32string(1, shifted);
    }
    return key.label;
}

std::optional<std::u32string> commandLegendFor(const KeyRect& key, bool isLinkMode)
{
    // Navigation mode (Link): only the keys that mean something here at
    // all -- see cursor.cpp's matching exclusive-navigation-mode gate in
    // Cursor::handleKey, which this must stay in sync with.
    if (isLinkMode)
    {
        if (key.label == U"i") return std::u32string(U"prev");
        if (key.label == U"k") return std::u32string(U"next");
        if (key.label == U"j") return std::u32string(U"back");
        if (key.label == U"l") return std::u32string(U"go");
        if (key.label == U"e") return std::u32string(U"edit");
        if (key.label == U"n") return std::u32string(U"cmd");
        return std::nullopt;
    }

    // General command mode (Cursor navigation sub-state): every key
    // Cursor::handleKey's switch implements.
    if (key.label == U"i") return std::u32string(U"up");
    if (key.label == U"k") return std::u32string(U"down");
    if (key.label == U"j") return std::u32string(U"left");
    if (key.label == U"l") return std::u32string(U"right");
    if (key.label == U"e") return std::u32string(U"edit");
    if (key.label == U"u") return std::u32string(U"prev");
    if (key.label == U"o") return std::u32string(U"next");
    if (key.label == U"d") return std::u32string(U"del");
    if (key.label == U"c") return std::u32string(U"+card");
    if (key.label == U"t") return std::u32string(U"+toc");
    if (key.label == U"n") return std::u32string(U"link");
    if (key.label == U"m") return std::u32string(U"prevT");
    if (key.label == U".") return std::u32string(U"nextT");
    return std::nullopt;
}

void drawKeyboardPanel(Canvas& canvas, bool leftSide, const HackAtlas::Atlas& atlas, const Rect* pressedKeyRect,
                        bool capsLatched, bool shiftEngaged, bool isTypingMode, bool isLinkMode)
{
    canvas.fillRect({0, 0, canvas.width(), canvas.height()}, kPanelColor);

    for (const KeyRect& key : layoutKeys(leftSide, canvas.width()))
    {
        bool inverted = (pressedKeyRect && sameRect(key.rect, *pressedKeyRect)) ||
                        (key.action == KeyRect::Action::CapsToggle && capsLatched) ||
                        (key.action == KeyRect::Action::ShiftToggle && shiftEngaged);
        Pixel faceColor = inverted ? kKeyLabelColor : kKeyColor;
        Pixel textColor = inverted ? kKeyColor : kKeyLabelColor;

        canvas.fillRect(key.rect, faceColor);
        drawBoxOutline(canvas, key.rect, kKeyBorderColor, 1);

        std::optional<std::u32string> text =
            isTypingMode ? std::optional(typingLabelFor(key, shiftEngaged)) : commandLegendFor(key, isLinkMode);
        if (!text)
            continue; // dead in this mode -- blank key face, no text at all

        int textWidth = static_cast<int>(text->size()) * atlas.cellWidth;
        Point textPos{key.rect.x + std::max(0, (key.rect.w - textWidth) / 2),
                      key.rect.y + std::max(0, (key.rect.h - atlas.cellHeight) / 2)};
        canvas.drawText(*text, textPos, textColor, atlas);
    }
}
