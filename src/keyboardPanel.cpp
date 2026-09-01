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
//
// Every letter sits at its real-QWERTY position -- q/w/e/r/t, a/s/d/f/g,
// z/x/c/v/b, y/u/i/o/p, h/j/k/l, n/m -- matching what a real keyboard
// (and a real touch-typist's fingers) already expects in typing mode.
// An earlier version of this table physically relocated c/t/d/e/n (and,
// as a knock-on, q/w/a/s) to new grid positions so +card/+toc/del/edit/
// nav would sit spatially close to cmd -- reverted: the user was
// explicit that only the *command-mode function* a key performs should
// move, never the letter it types, since Cursor dispatches command mode
// on the exact same codepoint typing mode sends (there is no separate
// "command identity" for a key, only its one codepoint -- see
// fillKeyEvent below) -- so relocating a function inherently means
// relocating whatever letter used to carry that codepoint too, which is
// precisely the confusion this reverts. c/t/d/e/n (+card/+toc/del/edit/
// nav) simply live wherever their own mnemonic letter's real position
// already is; only "caps" -> "cmd" is still a label change here, since
// that key was never a letter to begin with.
const std::array<std::array<const char32_t*, KeyboardPanel::kCols>, KeyboardPanel::kKeyRows> kLeftKeys = {{
    {U";", U"1", U"2", U"3", U"4", U"5"},
    {U"tab", U"q", U"w", U"e", U"r", U"t"},
    {U"cmd", U"a", U"s", U"d", U"f", U"g"},
    {U"shift", U"z", U"x", U"c", U"v", U"b"},
}};

const std::array<std::array<const char32_t*, KeyboardPanel::kCols>, KeyboardPanel::kKeyRows> kRightKeys = {{
    {U"6", U"7", U"8", U"9", U"0", U"bs"},
    {U"y", U"u", U"i", U"o", U"p", U"-"},
    {U"h", U"j", U"k", U"l", U"'", U"enter"},
    {U"n", U"m", U",", U".", U"/", U"shift"},
}};

constexpr Pixel kPanelColor = 0x00202020;
constexpr Pixel kKeyColor = 0x00E8E4DA; // "white"/no mode -- close to Card::kColor, so the whole device reads as one object
constexpr Pixel kKeyBorderColor = 0x00000000;
constexpr Pixel kKeyLabelColor = 0x00202020;

// A bright, saturated outline -- deliberately not any color a key's face
// or another indicator already uses (kKeyColor's cream, the mode tints
// below, or kKeyBorderColor's black), so hover always reads as its own
// distinct thing no matter what else a key is currently showing.
constexpr Pixel kHoverBorderColor = 0x00FFD700; // gold
constexpr int kHoverBorderWidth_px = 2;

// f/j's own home-row tactile-bump marker -- see its own comment in
// drawKeyboardPanel.
constexpr int kHomeMarkerThickness_px = 3;

// Soft, light tints (readable with the same dark kKeyLabelColor text
// every other key already uses) rather than fully saturated colors --
// keeps the mode-colored keys consistent with kKeyColor's own soft,
// cream-adjacent palette instead of clashing with it.
constexpr Pixel kEditColor = 0x00EBC4C4;       // red -- e's own mode-key face
constexpr Pixel kCommandColor = 0x00C4E0C4;    // green
constexpr Pixel kNavigationColor = 0x00C4D4EB; // blue
constexpr Pixel kDisabledColor = 0x00CACACA;   // gray -- deliberately not tinted toward red/green,
                                                // so "disabled" doesn't read as some fourth mode color

// Typing mode's four-tier red, brightest (most saturated) to dimmest --
// same hue throughout, just less pink/more toward kKeyColor's neutral
// cream as each tier matters less: letters are what you're mostly
// typing, digits less often, punctuation/spacebar less often still, and
// tab/shift/enter/bs -- control keys, not text you're producing --
// least of all.
constexpr Pixel kEditLetterColor = 0x00E8A6A6;  // brighter/more saturated than the old uniform kEditColor
constexpr Pixel kEditNumberColor = 0x00EBC4C4;  // == kEditColor -- the previous uniform shade
constexpr Pixel kEditOtherColor = 0x00EEDDDD;   // punctuation/spacebar
constexpr Pixel kEditControlColor = 0x00F0E8E8; // tab/shift/enter/bs -- closest to neutral

Pixel modeColorPixel(ModeColor color)
{
    switch (color)
    {
    case ModeColor::Edit: return kEditColor;
    case ModeColor::EditLetter: return kEditLetterColor;
    case ModeColor::EditNumber: return kEditNumberColor;
    case ModeColor::EditOther: return kEditOtherColor;
    case ModeColor::EditControl: return kEditControlColor;
    case ModeColor::Command: return kCommandColor;
    case ModeColor::Navigation: return kNavigationColor;
    case ModeColor::Disabled: return kDisabledColor;
    case ModeColor::None: default: return kKeyColor;
    }
}

void drawBoxOutline(Canvas& canvas, Rect r, Pixel color, int thickness)
{
    canvas.line({r.x, r.y}, {r.x + r.w, r.y}, color, thickness);
    canvas.line({r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, color, thickness);
    canvas.line({r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}, color, thickness);
    canvas.line({r.x, r.y + r.h}, {r.x, r.y}, color, thickness);
}

// The longest *regularly shown* single-width key label/legend --
// pickPanelAtlas sizes text to fit this within one key's pitch, not the
// double-wide spacebar's more generous width. Deliberately NOT the
// longest string a key can ever show: KeyMessage's own disabled-key
// explanations ("No history", "Read-Only", etc., up to 11 codepoints)
// are longer than every one of these, but sizing the *whole panel's*
// everyday font down to fit those was exactly backwards -- the user was
// explicit, found live: real keyboard keycaps size their legend to the
// keycap, around a third of its height, and letters were reading far
// smaller than that (a single glyph squeezed down to fit an 11-
// character message that only shows for a second, rarely, on one key at
// a time). So this now tracks "shift"/"enter"/"+card"/"prevT"/"nextT"
// (5, the longest strings shown as a matter of course) instead --
// letting the rare disabled-key messages spill past their key's own
// border into the surrounding gap if they need to, rather than
// shrinking every letter on the panel to accommodate them.
constexpr std::size_t kLongestSingleWidthLabel = 5;

// What a key labeled `label` does -- see KeyRect::Action's comment.
// "tab" has no corresponding Cursor::handleKey case at all today (PLAN.md's
// Keyboard Mapping table specs it, but nothing implements it yet -- see
// PLAN.md's Ortholinear Keyboard section), so it's a deliberate no-op
// rather than guessing a mapping that isn't real. Every other key already
// has a well-defined physical meaning: "cmd"/"shift" get the two-gesture
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
        // Never actually dispatched (Action::None) -- set purely so this
        // key has a stable identity main.cpp's physical-key flash (see
        // platform.h's onPhysicalKey) can match against, the same way
        // every other key's kind/codepoint already doubles as its
        // identity there.
        key.codepoint = U'\t';
    }
    else if (key.label == U"shift")
    {
        key.action = KeyRect::Action::ShiftToggle;
    }
    else if (key.label == U"cmd")
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
            // A real press+release pair, mirroring what an actual
            // hardware tap sends -- and what Cursor::handleKey's
            // tap-vs-hold detection (m_capsTapLatched) assumes: it only
            // recognizes "nothing typed between this press and its
            // matching release" by seeing two separate CapsLock events.
            // A mouse click's own down/up is already one gesture, but
            // collapsing it into a single event whose `pressed` mirrored
            // the new latch state (what this used to do) meant Cursor
            // never actually saw a press *and* a release -- so its tap
            // detection could never fire, and a second plain click could
            // never reach the branch that releases the latch. Found
            // live, not by inspection: two clean clicks on cmd with
            // nothing else pressed between them never returned to typing
            // mode. outcome.capsLatched still just tracks the new state,
            // for the panel's own highlight -- unrelated to what events
            // Cursor itself needs to see.
            outcome.capsLatched = !capsLatchedBefore;
            outcome.events.push_back({KeyEvent::Kind::CapsLock, 0, true});
            outcome.events.push_back({KeyEvent::Kind::CapsLock, 0, false});
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
    int maxCellWidth_px = pitch_px / static_cast<int>(kLongestSingleWidthLabel + 1);

    // Deliberately not canvas.h's pickAtlas() -- that picks whichever
    // baked size is *closest* to the target either way, which is right
    // for the card body/title (matching physical pixel density as
    // closely as possible, where landing a hair over or under doesn't
    // cost anything but a little blur), but wrong here: a key's text has
    // a hard ceiling (its own pitch), so "closest" can round up and
    // overflow past the key's border. This picks the largest baked
    // atlas that still fits under maxCellWidth_px -- the user was
    // explicit the panel should use the largest font that fits, not
    // just whichever is nearest. kAtlases is sorted ascending by
    // cellWidth (see hackAtlas.h), so the last one still <= the ceiling
    // is the answer; if even the smallest doesn't fit (an extremely
    // small panel), fall back to it anyway rather than rendering
    // nothing.
    const HackAtlas::Atlas* best = &HackAtlas::kAtlases[0];
    for (std::size_t i = 0; i < HackAtlas::kAtlasCount; ++i)
    {
        if (HackAtlas::kAtlases[i].cellWidth > maxCellWidth_px)
            break;
        best = &HackAtlas::kAtlases[i];
    }
    return *best;
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
    // cmd always describes itself, in either command sub-state -- it's
    // how you step back out one level (Navigation -> general command ->
    // typing -- see cursor.cpp's CapsLock-release branch), so it's never
    // blank the way a truly dead key is.
    if (key.label == U"cmd") return std::u32string(U"cmd");

    // Navigation mode (Link): only the keys that mean something here at
    // all -- see cursor.cpp's matching exclusive-navigation-mode gate in
    // Cursor::handleKey, which this must stay in sync with. s/a (nav/edit)
    // are ordinary blocked keys here now, not mode-exit shortcuts -- cmd
    // is the only way out.
    if (isLinkMode)
    {
        if (key.label == U"i") return std::u32string(U"prev");
        if (key.label == U"k") return std::u32string(U"next");
        if (key.label == U"j") return std::u32string(U"back");
        if (key.label == U"l") return std::u32string(U"go");
        return std::nullopt;
    }

    // General command mode (Cursor navigation sub-state): every key
    // Cursor::handleKey's switch implements. i/k/j/l move the cursor, so
    // they show actual arrow glyphs rather than the word -- same U+2190..
    // U+2193 arrows CardItem::linkStr() already uses for card links, now
    // also baked into bakeFont's glyphCodepoints() for this.
    if (key.label == U"i") return std::u32string(U"↑");
    if (key.label == U"k") return std::u32string(U"↓");
    if (key.label == U"j") return std::u32string(U"←");
    if (key.label == U"l") return std::u32string(U"→");
    if (key.label == U"a") return std::u32string(U"edit");
    if (key.label == U"u") return std::u32string(U"prev");
    if (key.label == U"o") return std::u32string(U"next");
    if (key.label == U"e") return std::u32string(U"del");
    if (key.label == U"q") return std::u32string(U"+card");
    if (key.label == U"w") return std::u32string(U"+toc");
    if (key.label == U"s") return std::u32string(U"nav");
    if (key.label == U"m") return std::u32string(U"prevT");
    if (key.label == U".") return std::u32string(U"nextT");
    return std::nullopt;
}

ModeColor modeColorFor(const KeyRect& key, bool isTypingMode, bool isLinkMode, const KeyDisabledState& disabled)
{
    // cmd always shows its own color regardless of mode -- unlike s/a, it
    // never produces literal text, so there's no "just an ordinary key"
    // state for it to defer to.
    if (key.label == U"cmd") return ModeColor::Command;

    // Typing mode: every other key is some shade of red, full stop -- the
    // user was explicit that typing mode should read as "everything is
    // red except cmd" (shift/tab included, even though neither is itself
    // a Fire key), then later that letters should read slightly brighter
    // than digits, punctuation less still, and tab/shift/enter/bs least
    // of all -- keyed off what the key actually is (a Fire+Char key's
    // own codepoint, or lack of one), not its label, so it stays correct
    // regardless of which physical spot a letter/digit lives at. s/a are
    // ordinary letters here (this is where they type a literal 's'/'a'),
    // not mode keys -- found live, not by inspection: n (s's predecessor
    // in an earlier version of this mapping) showing its command-mode
    // blue while typing read as wrong the instant it was seen on screen,
    // since here it's just a letter like any other.
    if (isTypingMode)
    {
        if (key.action == KeyRect::Action::Fire && key.kind == KeyEvent::Kind::Char)
        {
            if (key.codepoint >= U'a' && key.codepoint <= U'z') return ModeColor::EditLetter;
            if (key.codepoint >= U'0' && key.codepoint <= U'9') return ModeColor::EditNumber;
            return ModeColor::EditOther; // punctuation, spacebar
        }
        return ModeColor::EditControl; // tab (None), shift (ShiftToggle), enter/bs (Fire but not Char)
    }

    // General command mode: s/a (nav/edit) show their own permanent color
    // here -- they're not producing text in this mode, they're mode-
    // transition keys. editDisabled/deleteDisabled/prevThreadDisabled/
    // nextThreadDisabled/prevCardDisabled/nextCardDisabled override
    // a/e/m/./u/o's usual color with gray here, each driven by the
    // matching CardItem/Cursor predicate -- still shows a legend
    // (commandLegendFor doesn't change), just not the color that would
    // suggest pressing it does something.
    //
    // i/k/j/l (the arrows themselves) also gray out here when
    // editDisabled -- see KeyDisabledState's own comment for why that's
    // deliberate, not an oversight.
    //
    // Navigation mode: j (back)/i (prev)/k (next) get the same disabled
    // treatment, driven by backDisabled/prevDisabled/nextDisabled --
    // Cursor::hasLinkHistory() (nothing to pop) / CardItem::
    // isAtFirstLink()/isAtLastLink() (nothing further that way to select)
    // respectively, independently for i and k.
    //
    // Both branches found live, not by inspection: a blocked s/a still
    // showing its own color read as live when it wasn't, the same
    // mistake disabled-but-uncolored keys would make if this didn't
    // handle them explicitly.
    if (!isLinkMode)
    {
        if (key.label == U"s") return ModeColor::Navigation;
        if (key.label == U"a") return disabled.editDisabled ? ModeColor::Disabled : ModeColor::Edit;
        if (key.label == U"e") return disabled.deleteDisabled ? ModeColor::Disabled : ModeColor::Command;
        if (key.label == U"m") return disabled.prevThreadDisabled ? ModeColor::Disabled : ModeColor::Command;
        if (key.label == U".") return disabled.nextThreadDisabled ? ModeColor::Disabled : ModeColor::Command;
        if (key.label == U"u") return disabled.prevCardDisabled ? ModeColor::Disabled : ModeColor::Command;
        if (key.label == U"o") return disabled.nextCardDisabled ? ModeColor::Disabled : ModeColor::Command;
        if ((key.label == U"i" || key.label == U"k" || key.label == U"j" || key.label == U"l") &&
            disabled.editDisabled)
            return ModeColor::Disabled;
    }
    else
    {
        if (key.label == U"j" && disabled.backDisabled) return ModeColor::Disabled;
        if (key.label == U"i" && disabled.prevDisabled) return ModeColor::Disabled;
        if (key.label == U"k" && disabled.nextDisabled) return ModeColor::Disabled;
    }

    // Whichever other keys commandLegendFor considers live right now --
    // kept in sync with it by construction, since this calls the exact
    // same function rather than re-deriving the same live/blank
    // distinction a second way.
    if (commandLegendFor(key, isLinkMode))
        return isLinkMode ? ModeColor::Navigation : ModeColor::Command;
    return ModeColor::None;
}

void drawKeyboardPanel(Canvas& canvas, bool leftSide, const HackAtlas::Atlas& atlas, const Rect* pressedKeyRect,
                        const Rect* hoveredKeyRect, bool shiftEngaged, bool spacebarEngaged, bool isTypingMode,
                        bool isLinkMode, const KeyDisabledState& disabled, const std::optional<KeyMessage>& message)
{
    canvas.fillRect({0, 0, canvas.width(), canvas.height()}, kPanelColor);

    for (const KeyRect& key : layoutKeys(leftSide, canvas.width()))
    {
        // A key showing its own "why didn't that work" explanation (see
        // message below) matches on codepoint *and* isLinkMode -- see
        // KeyMessage's own comment for why both are needed (the same
        // physical key can mean two different things depending on
        // command sub-state). Requires action == Fire && kind == Char
        // since that's the only thing message->codepoint could mean
        // (cmd/shift/tab/enter/bs never carry a disabled-key message).
        bool showingMessage = message.has_value() && key.action == KeyRect::Action::Fire &&
                              key.kind == KeyEvent::Kind::Char && message->codepoint == key.codepoint &&
                              message->isLinkMode == isLinkMode;

        // cmd inverts only for as long as it's actually held down
        // (pressedKeyRect, same as any other key) -- a momentary flash on
        // press and again on release, not a persistent indicator of
        // command mode being latched. Unlike shift, which does stay
        // inverted for as long as its own latch is on: cmd already has
        // its own permanent Command-colored face (modeColorFor) to show
        // "this is what I do," so a lasting invert on top of that would
        // just be redundant -- the user was explicit cmd shouldn't stay
        // inverted the way shift does. A key showing its own message
        // stays inverted for the message's whole lifetime, not just the
        // originating press -- reads as "this is the reason," the same
        // visual weight a held key already gets, rather than just gray
        // text sitting quietly on its usual disabled face.
        bool inverted = showingMessage || (pressedKeyRect && sameRect(key.rect, *pressedKeyRect)) ||
                        (key.action == KeyRect::Action::ShiftToggle && shiftEngaged) ||
                        (key.label == U"spacebar" && spacebarEngaged);

        // The key's own "light" color -- its mode color if it has one
        // right now (cmd/s/a always; any other key only while live in
        // the current mode), kKeyColor ("white"/no mode) otherwise.
        // Inverting swaps this with the universal dark kKeyLabelColor,
        // exactly the same press-feedback/latch pattern as before, just
        // parameterized per key instead of a single hardcoded pair.
        Pixel lightColor = modeColorPixel(modeColorFor(key, isTypingMode, isLinkMode, disabled));
        Pixel faceColor = inverted ? kKeyLabelColor : lightColor;
        Pixel textColor = inverted ? lightColor : kKeyLabelColor;

        canvas.fillRect(key.rect, faceColor);
        drawBoxOutline(canvas, key.rect, kKeyBorderColor, 1);

        // Drawn on top of the face/border above regardless of whether this
        // key has any text below -- hover applies to every key uniformly,
        // dead ones included (see this function's own header comment), so
        // it has to happen before the early exit for a blank/dead key.
        if (hoveredKeyRect && sameRect(key.rect, *hoveredKeyRect))
            drawBoxOutline(canvas, key.rect, kHoverBorderColor, kHoverBorderWidth_px);

        // Index-finger home markers -- f (left panel)/j (right panel),
        // matching the raised tactile bump a real keyboard puts on those
        // two keys so touch-typing fingers can find home row without
        // looking. Drawn uniformly regardless of live/dead, same as
        // hover above (it's about where your fingers go, not what the
        // key currently does) -- and in textColor, not a fixed color, so
        // it stays visible against any face this key might be showing
        // right now, inverted included.
        bool isHomeMarker = (leftSide && key.label == U"f") || (!leftSide && key.label == U"j");
        if (isHomeMarker)
        {
            int markerWidth_px = key.rect.w * 2 / 5;
            int markerX = key.rect.x + (key.rect.w - markerWidth_px) / 2;
            int markerY = key.rect.y + key.rect.h * 4 / 5;
            canvas.line({markerX, markerY}, {markerX + markerWidth_px, markerY}, textColor, kHomeMarkerThickness_px);
        }

        std::optional<std::u32string> text =
            isTypingMode ? std::optional(typingLabelFor(key, shiftEngaged)) : commandLegendFor(key, isLinkMode);

        // Overrides whatever text was just picked above -- explains *why*
        // the key that was just clicked didn't do anything, instead of it
        // just silently sitting there gray. main.cpp owns clearing this
        // again after a few seconds.
        if (showingMessage)
            text = message->text;

        if (!text)
            continue; // dead in this mode -- blank key face, no text at all

        int textWidth = static_cast<int>(text->size()) * atlas.cellWidth;
        Point textPos{key.rect.x + std::max(0, (key.rect.w - textWidth) / 2),
                      key.rect.y + std::max(0, (key.rect.h - atlas.cellHeight) / 2)};
        canvas.drawText(*text, textPos, textColor, atlas);
    }
}
