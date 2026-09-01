// main.cpp -- composes the core (Cursor/Canvas) with a platform shell
// (platform.h) into a running app. Deliberately has no #ifdef/platform
// header of its own: everything it touches is the platform.h contract, so
// this file is meant to be shared as-is once Linux/web shells exist (see
// PLAN.md), not rewritten per platform.
//
// fj isn't really a Windows app that happens to draw a card -- it's an
// emulator for a piece of keyboard-only hardware that doesn't exist yet
// (see PLAN.md), and this window IS that hardware's monitor. Everything
// window-chrome-y (the title bar, right-click-to-calibrate, resizing) is
// the emulator, not the emulated device -- the real hardware won't have
// any of it. The monitor's own physical size is layout.h's
// Monitor::kWidth_in/kHeight_in (5x5), separate from Card::kWidth_in/
// kHeight_in (5x3, the card shown *on* it) -- see layout.h's Monitor
// comment for how the two relate.

#include "canvas.h"
#include "cardItem.h"
#include "cursor.h"
#include "hackAtlas.h"
#include "keyboardPanel.h"
#include "layout.h"
#include "platform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

namespace
{

// The Body-cell pixel width a given on-screen pixel width implies -- same
// relationship tools/offline/bakeFont used to choose each atlas's size in
// the first place, so feeding it back into pickAtlas finds whichever baked
// atlas best matches how big the window currently is. +4 accounts for
// CardItem::sideMargin_px: the card's true rendered width is
// (Body::kColsPerRow + 4) cells (60 of text plus a 2-cell margin on each
// side, i.e. 4 whole extra cells -- see CardItem::cardWidth_px), not
// Body::kColsPerRow of them.
int desiredCellWidth_px(int width_px)
{
    return width_px / (Body::kColsPerRow + 4);
}

// "fj (emulated) - 15"x5" 100% -- press F5 to fix calibration" --
// describes the whole emulated device now (Device::kWidth_in/kHeight_in:
// the monitor plus both keyboard panels), not the monitor alone, since the
// window represents the whole device (see this file's top comment).
// unit_px is one 5"x5" region's actual on-screen size -- min(window
// width / 3, window height), since the window itself can be any shape now
// (see createPlatformWindow's comment) and the 3:1 device is letterboxed/
// pillarboxed to fit whichever axis is more constraining (see main()'s
// redraw). percent is unit_px against Monitor::kWidth_in (still 5in --
// every region, panel or screen, is the same 5"x5") at the display's true
// DPI. ceil matches the old Qt app's rounding (PLAN.md): never displays a
// percent lower than what's actually achieved. The trailing hint exists
// purely so KeyEvent::Kind::Calibrate is discoverable at all -- there's
// no other UI surface pointing at it. F5, not a platform-specific
// affordance like win32Window.cpp's "Fix Calibration..." system-menu
// item: this file has no #ifdef/platform header of its own (see the top
// comment) and stays that way, so the one thing it advertises has to
// actually work identically on every shell -- Xlib has no portable
// equivalent of GetSystemMenu to hang a second hint off of. "(emulated)"
// is there so the title itself keeps making the point this file's top
// comment makes.
//
// dpiKnown false means dpi is startupDpi()'s bare fallback guess, not a
// real reading or a user calibration (see platform.h's displayDpi
// comment) -- printing a specific "100%" in that case would be the exact
// false-precision mistake PLAN.md's Physical accuracy section already
// covers elsewhere: the window's initial size is deliberately computed
// FROM that same guessed dpi (see main()), so it would always read
// "100%" regardless of whether the guess bore any relation to the real
// display. Showing "unconfirmed" instead is the honest version of the
// same readout.
std::string titleFor(int unit_px, int dpi, bool dpiKnown)
{
    char buf[128];
    if (!dpiKnown)
    {
        std::snprintf(buf, sizeof(buf), "fj (emulated) - %.0f\"x%.0f\" size unconfirmed -- press F5 once sized correctly",
                      Device::kWidth_in, Device::kHeight_in);
        return buf;
    }
    double percent = unit_px / (dpi * Monitor::kWidth_in) * 100.0;
    std::snprintf(buf, sizeof(buf), "fj (emulated) - %.0f\"x%.0f\" %d%% -- press F5 to fix calibration",
                  Device::kWidth_in, Device::kHeight_in, static_cast<int>(std::ceil(percent)));
    return buf;
}

// Persisted calibration override for when the OS-reported monitor size
// (win32Window.cpp's displayDpi) is simply wrong -- a hardware/EDID data
// problem no DPI-awareness fix can correct. See platform.h's
// Kind::Calibrate comment: the user drag-resizes the window against a
// physical ruler, then presses F5, and calibratedDpi() below is what
// makes that stick across launches instead of just for the one session.
std::filesystem::path calibrationFilePath()
{
    // Windows: %APPDATA%\fj\calibration.txt. Linux: the XDG Base
    // Directory convention -- $XDG_CONFIG_HOME/fj/calibration.txt,
    // falling back to ~/.config/fj/calibration.txt (the documented XDG
    // default) when XDG_CONFIG_HOME isn't set. No #ifdef on which
    // platform this is -- this file is meant to be shared as-is (see the
    // top comment), so this just checks whichever of these env vars
    // actually exists; at most one of the three branches below is ever
    // going to fire on a given platform. (An earlier version of this
    // only checked APPDATA -- calibration still worked for the running
    // session on Linux, since main()'s own dpi variable gets updated
    // regardless, but silently never persisted across relaunches.)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // std::getenv: fine for this single-threaded, read-once use
#endif
    if (const char* appData = std::getenv("APPDATA"))
        return std::filesystem::path(appData) / "fj" / "calibration.txt";
    if (const char* xdgConfig = std::getenv("XDG_CONFIG_HOME"))
        return std::filesystem::path(xdgConfig) / "fj" / "calibration.txt";
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".config" / "fj" / "calibration.txt";
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    return {};
}

// known is false only when there's no saved calibration yet AND the
// platform couldn't get a real reading (see platform.h's displayDpi
// comment) -- 96 is then just a starting guess, not a trustworthy value,
// which titleFor's dpiKnown parameter exists to say so honestly instead
// of claiming a confident (but potentially meaningless) percentage.
struct StartupDpi
{
    int value;
    bool known;
};

StartupDpi startupDpi()
{
    auto path = calibrationFilePath();
    if (!path.empty())
    {
        std::ifstream in(path);
        double dpi = 0.0;
        if (in >> dpi && dpi > 0.0)
            return {static_cast<int>(std::lround(dpi)), true};
    }
    if (std::optional<int> real = displayDpi())
        return {*real, true};
    return {96, false};
}

void saveCalibratedDpi(int dpi)
{
    auto path = calibrationFilePath();
    if (path.empty())
        return;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream(path) << dpi;
}

} // namespace

int main()
{
    Cursor cursor;
    StartupDpi startup = startupDpi();
    int dpi = startup.value;
    bool dpiKnown = startup.known; // updated in Kind::Calibrate below, once the user confirms it

    // Initial window size: the true, continuous physical target (dpi *
    // Device::kWidth_in/kHeight_in -- the whole 15"x5" device, not just
    // the monitor). Doesn't need to land on an atlas-exact pixel count the
    // way an earlier version insisted on -- every redraw (below) already
    // fits its content to whatever size the window actually is, startup
    // included, so there's nothing special to get right here beyond a
    // reasonable starting size.
    int currentWidth_px = static_cast<int>(std::lround(dpi * Device::kWidth_in));
    int currentHeight_px = static_cast<int>(std::lround(dpi * Device::kHeight_in));

    // No aspect-ratio parameter: the window can be resized to any shape
    // (see platform.h's createPlatformWindow comment) -- redraw below
    // fits the square monitor into whatever shape it actually is.
    auto windowResult = createPlatformWindow(currentWidth_px, currentHeight_px, "fj");
    if (!windowResult)
    {
        std::fprintf(stderr, "fj: failed to create window: %s\n", windowResult.error().c_str());
        return 1;
    }
    PlatformWindow window = std::move(*windowResult);

    // atlas/titleAtlas/cardCanvas are all sized for one 5"x5" region's
    // *content* resolution -- unit_px below, whichever window axis is more
    // constraining once divided into the device's 3:1 layout -- see
    // presentFrame's letterboxing comment for why the window itself
    // doesn't have to match that aspect ratio for this to work.
    auto unitPx = [](int width_px, int height_px) { return std::min(width_px / 3, height_px); };
    const HackAtlas::Atlas* atlas = &pickAtlas(desiredCellWidth_px(unitPx(currentWidth_px, currentHeight_px)));

    // Title rows render from their own, separately-picked atlas rather
    // than upscaling atlas's own bitmaps -- see cursor.h's draw comment
    // and PLAN.md's Font atlas section: nearest-neighbor and bilinear
    // upscaling of already anti-aliased coverage data were both tried and
    // both looked noticeably softer than a glyph actually baked at that
    // size. Closest to exactly 2x atlas's cell width, matching the
    // layout math CardItem::cellWidth_px already assumes for Title rows.
    const HackAtlas::Atlas* titleAtlas = &pickAtlas(atlas->cellWidth * 2);

    Canvas cardCanvas(cursor.currentCard()->cardWidth_px(*atlas), cursor.currentCard()->cardHeight_px(*atlas));

    // The size onResizeEnd last actually rebuilt the card for -- lets it
    // skip that work when it fires again for a size that's already
    // settled (e.g. the WM sending a late settling ConfigureNotify or two
    // after the drag itself ends, past xlibWindow.cpp's debounce window),
    // rather than visibly re-rendering again for no actual change.
    int lastSettledWidth_px = currentWidth_px;
    int lastSettledHeight_px = currentHeight_px;

    // caps/shift each support two gestures -- a plain tap toggles a
    // persistent latch (mirroring a real Caps Lock's own latch); a
    // press-drag-release chord (press on caps/shift, drag to another key,
    // release there) applies momentarily to just that one key, since a
    // single mouse/touch point can't hold one key down while also tapping
    // another the way two fingers on a real keyboard can. See
    // keyboardPanel.h's resolveKeyGesture, which implements both gestures
    // for both keys as one pure function -- the onClick handler below is
    // just hit-testing plus replaying whatever it returns.
    bool capsLatched = false;
    bool shiftLatched = false;

    // The key (if any) a press landed on, remembered until the matching
    // release arrives -- resolveKeyGesture needs both ends of the gesture
    // to tell a plain tap from a drag-chord. Also what drives the pressed-
    // key highlight in renderContent below.
    struct PressedKey
    {
        KeyRect key;
        bool leftSide;
    };
    std::optional<PressedKey> pressedKey;

    // The key (if any) currently under the pointer -- drives the hover
    // highlight in renderContent below, independent of press/latch state
    // (see keyboardPanel.h's drawKeyboardPanel comment: hover applies to
    // every key, including ones that don't do anything, since it's purely
    // about where the pointer is). Reuses PressedKey's shape since it's
    // exactly the same "which key, which side" pair hitTestClick already
    // produces for a click.
    std::optional<PressedKey> hoveredKey;

    // Whether hoveredKey actually changed -- onMouseMove fires on every
    // pointer-move tick, far more often than the panel's own key
    // boundaries do, so redrawing unconditionally on every tick would be
    // pure waste on an otherwise-static panel; only a real transition
    // (onto a different key, or off the panel entirely) needs one.
    auto sameHoveredSpot = [](const std::optional<PressedKey>& a, const std::optional<PressedKey>& b)
    {
        if (a.has_value() != b.has_value())
            return false;
        if (!a)
            return true;
        return a->leftSide == b->leftSide && a->key.rect.x == b->key.rect.x && a->key.rect.y == b->key.rect.y &&
               a->key.rect.w == b->key.rect.w && a->key.rect.h == b->key.rect.h;
    };

    // Physical-keyboard press/release (see platform.h's onPhysicalKey) --
    // flashes the matching on-screen key the same way a mouse press
    // already does, entirely separate from pressedKey above: never
    // touches resolveKeyGesture/Cursor, purely visual. Shift gets its own
    // bool instead of a PressedKey, matching drawKeyboardPanel's own
    // shiftEngaged parameter: both physical shift keys already light up
    // together whenever that's true, so there's no need to know which
    // side was actually pressed.
    std::optional<PressedKey> physicalPressedKey;
    bool physicalShiftHeld = false;

    // Which key (if any) should currently show its own "why didn't that
    // work" explanation instead of its usual legend -- set for a few
    // seconds after clicking a disabled key (see onClick's own comment),
    // so the click explains itself instead of just silently doing
    // nothing. Only one shows at a time (simplicity -- these are rare,
    // brief, and never simultaneous in practice). messageGeneration
    // guards the scheduleOnce() callback that clears it against firing
    // late and clobbering a *newer* message: a second disabled-key click
    // before the first one's timer fires bumps this, and the stale
    // callback checks its own captured snapshot against the current
    // value before clearing anything.
    std::optional<KeyMessage> keyMessage;
    int messageGeneration = 0;

    // Finds whichever on-screen key (either panel) has the same kind/
    // codepoint identity as event -- see platform.h's onPhysicalKey
    // comment for why that's enough, without this needing to know
    // anything about physical scan codes itself. cmd is matched by its
    // CapsToggle action specifically (its own KeyRect's kind is
    // CapsLock, same as event's, so kind alone already lands on the
    // right key -- codepoint is irrelevant there and left unchecked).
    // Shift is handled by the caller instead (see physicalShiftHeld
    // above); event.kind == Shift never reaches here.
    auto findPhysicalKey = [&](const KeyEvent& event) -> std::optional<PressedKey>
    {
        for (bool leftSide : {true, false})
        {
            for (const KeyRect& key : layoutKeys(leftSide, cardCanvas.width()))
            {
                if (key.kind == event.kind && (event.kind != KeyEvent::Kind::Char || key.codepoint == event.codepoint))
                    return PressedKey{key, leftSide};
            }
        }
        return std::nullopt;
    };

    // Square, cardCanvas.width() on a side: Monitor::kWidth_in ==
    // Card::kWidth_in, so at whatever resolution the atlas rendered the
    // card's width, that same pixel count is exactly Monitor::kHeight_in
    // too (both 5in) -- and, since KeyboardPanel::kWidth_in/kHeight_in
    // match Monitor's, it's also each keyboard panel's side length.
    // Persistent Canvases, not fresh locals like cardCanvas -- rebuilt in
    // renderContent below every call (not just on resize), so the pressed/
    // latched highlight stays live -- so presentFrame can still reuse
    // whatever was last drawn across any number of live-resize ticks
    // without redoing that work.
    Canvas monitorCanvas(cardCanvas.width(), cardCanvas.width());
    Canvas leftPanelCanvas(cardCanvas.width(), cardCanvas.width());
    Canvas rightPanelCanvas(cardCanvas.width(), cardCanvas.width());

    // Re-picked only on a resize settle (below) -- unlike the panels'
    // pixels themselves, which redraw every frame, the baked atlas they
    // draw text from only needs to change when the panel's own resolution
    // does.
    const HackAtlas::Atlas* leftPanelAtlas = &pickPanelAtlas(leftPanelCanvas.width());
    const HackAtlas::Atlas* rightPanelAtlas = &pickPanelAtlas(rightPanelCanvas.width());

    // The whole 15"x5" device, three regions wide: left panel, monitor,
    // right panel, left to right.
    Canvas deviceCanvas(monitorCanvas.width() * 3, monitorCanvas.height());

    // Draws the card and composites it onto monitorCanvas, then composites
    // all three regions onto deviceCanvas -- the "content changed" half of
    // a frame, as opposed to presentFrame's "put it on screen" half below.
    // Only needed when the card's own pixels actually changed
    // (typing/navigation) or cardCanvas itself was just rebuilt at a new
    // atlas (a resize settling); a live-resize tick's window-shape-only
    // change never needs this, which is the whole reason it's split out
    // rather than folded into presentFrame.
    auto renderContent = [&]()
    {
        cursor.draw(cardCanvas, *atlas, *titleAtlas);
        monitorCanvas = Canvas(cardCanvas.width(), cardCanvas.width());
        monitorCanvas.blit(cardCanvas, {0, (monitorCanvas.height() - cardCanvas.height()) / 2});

        // Mouse takes precedence on the vanishingly unlikely chance both
        // are somehow set at once -- effectivePressedKey is just
        // "whichever press should currently flash," feeding the same
        // leftPressedRect/rightPressedRect drawKeyboardPanel already
        // expects regardless of which source it came from.
        const std::optional<PressedKey>& effectivePressedKey = pressedKey ? pressedKey : physicalPressedKey;
        const Rect* leftPressedRect =
            (effectivePressedKey && effectivePressedKey->leftSide) ? &effectivePressedKey->key.rect : nullptr;
        const Rect* rightPressedRect =
            (effectivePressedKey && !effectivePressedKey->leftSide) ? &effectivePressedKey->key.rect : nullptr;
        const Rect* leftHoveredRect = (hoveredKey && hoveredKey->leftSide) ? &hoveredKey->key.rect : nullptr;
        const Rect* rightHoveredRect = (hoveredKey && !hoveredKey->leftSide) ? &hoveredKey->key.rect : nullptr;

        // Live preview, not just post-gesture state: a key currently
        // mid-press (pressed but not yet released) previews its effect
        // immediately, the same way its own face already inverts before
        // release -- see keyboardPanel.h's drawKeyboardPanel comment.
        // physicalShiftHeld/effectivePressedKey extend that same preview
        // to a physical keypress, not just a mouse one (see platform.h's
        // onPhysicalKey).
        bool shiftEngaged = shiftLatched || physicalShiftHeld ||
                             (pressedKey && pressedKey->key.action == KeyRect::Action::ShiftToggle);
        // Both panels' spacebar light up together regardless of which
        // side is actually pressed -- mouse or physical (effectivePressedKey
        // already unifies the two, same as leftPressedRect/rightPressedRect
        // above), matching shift's own cross-panel behavior. Unlike shift,
        // there's no latch to check: spacebar is an ordinary momentary Fire
        // key, so this is only true for as long as the press itself lasts.
        bool spacebarEngaged = effectivePressedKey && effectivePressedKey->key.label == U"spacebar";
        bool commandModeForLegend =
            cursor.isCommandMode() ||
            (effectivePressedKey && effectivePressedKey->key.action == KeyRect::Action::CapsToggle);
        bool isTypingModeForLegend = !commandModeForLegend;
        bool isLinkModeForLegend = cursor.isLinkMode(); // only meaningful once actually in command mode

        // CardItem's own canEdit()/canDelete()/isAtFirstLink()/
        // isAtLastLink() and Cursor's own hasLinkHistory()/
        // hasPrevThreadCard()/hasNextThreadCard()/isAtFirstCard()/
        // isAtLastCard() -- see their comments -- queried fresh every
        // frame so every key's gray "disabled" styling always matches
        // whichever card/history is actually current right now.
        KeyDisabledState disabled;
        disabled.editDisabled = !cursor.currentCard()->canEdit();
        disabled.deleteDisabled = !cursor.currentCard()->canDelete();
        disabled.backDisabled = !cursor.hasLinkHistory();
        disabled.prevDisabled = cursor.currentCard()->isAtFirstLink();
        disabled.nextDisabled = cursor.currentCard()->isAtLastLink();
        disabled.prevThreadDisabled = !cursor.hasPrevThreadCard();
        disabled.nextThreadDisabled = !cursor.hasNextThreadCard();
        disabled.prevCardDisabled = cursor.isAtFirstCard();
        disabled.nextCardDisabled = cursor.isAtLastCard();

        leftPanelCanvas = Canvas(cardCanvas.width(), cardCanvas.width());
        rightPanelCanvas = Canvas(cardCanvas.width(), cardCanvas.width());
        drawKeyboardPanel(leftPanelCanvas, /*leftSide=*/true, *leftPanelAtlas, leftPressedRect, leftHoveredRect,
                           shiftEngaged, spacebarEngaged, isTypingModeForLegend, isLinkModeForLegend, disabled,
                           keyMessage);
        drawKeyboardPanel(rightPanelCanvas, /*leftSide=*/false, *rightPanelAtlas, rightPressedRect, rightHoveredRect,
                           shiftEngaged, spacebarEngaged, isTypingModeForLegend, isLinkModeForLegend, disabled,
                           keyMessage);

        deviceCanvas = Canvas(monitorCanvas.width() * 3, monitorCanvas.height());
        deviceCanvas.blit(leftPanelCanvas, {0, 0});
        deviceCanvas.blit(monitorCanvas, {monitorCanvas.width(), 0});
        deviceCanvas.blit(rightPanelCanvas, {monitorCanvas.width() * 2, 0});
    };

    // Where deviceCanvas lands within the window -- the largest centered
    // 3:1 rect that fits, letterboxed/pillarboxed rather than distorted
    // (see createPlatformWindow's comment: the window itself doesn't have
    // to match the device's 3:1 aspect). Shared by presentFrame (what to
    // blitScaled deviceCanvas into) and the onClick handler below (what a
    // raw window-pixel click needs to be mapped back out of).
    auto deviceLetterboxRect = [&]() -> Rect
    {
        int unit = unitPx(currentWidth_px, currentHeight_px);
        int deviceWidth = unit * 3;
        return {(currentWidth_px - deviceWidth) / 2, (currentHeight_px - unit) / 2, deviceWidth, unit};
    };

    // Fits deviceCanvas into the window's actual shape. Built at exactly
    // the window's current size and handed to present() as-is, so the
    // platform shell's own stretch (still there as a defensive fallback --
    // see platform.h) is normally a no-op, not a second resample on top of
    // this one. smooth is Canvas::blitScaled's -- see its comment for why
    // a live-resize tick needs false here, not the general redraw default
    // of true.
    auto presentFrame = [&](bool smooth)
    {
        Canvas outputCanvas(currentWidth_px, currentHeight_px);
        outputCanvas.fillRect({0, 0, currentWidth_px, currentHeight_px}, 0x00000000);
        outputCanvas.blitScaled(deviceCanvas, deviceLetterboxRect(), smooth);
        window.present(outputCanvas.pixels(), outputCanvas.width(), outputCanvas.height());
    };

    // The ordinary full refresh -- content changed, so re-render it and
    // present smoothly. Startup, typing/navigation, and a settled resize
    // all use this; only a live-resize tick skips renderContent and
    // calls presentFrame directly (see the onResize callback below).
    auto redraw = [&]()
    {
        renderContent();
        presentFrame(true);
    };

    // Maps a raw window pixel to whichever on-screen key it lands on (if
    // any), undoing deviceLetterboxRect's stretch to land back in
    // deviceCanvas's own pixel space, then splitting into which of the
    // three regions (left panel/monitor/right panel) that falls in --
    // only the panels are clickable in this phase, so a pixel over the
    // monitor/card region or the letterbox margin resolves to nothing.
    auto hitTestClick = [&](int x_px, int y_px) -> std::optional<PressedKey>
    {
        Rect rect = deviceLetterboxRect();
        if (x_px < rect.x || x_px >= rect.x + rect.w || y_px < rect.y || y_px >= rect.y + rect.h)
            return std::nullopt;

        int deviceX = (x_px - rect.x) * deviceCanvas.width() / rect.w;
        int deviceY = (y_px - rect.y) * deviceCanvas.height() / rect.h;
        int unit = deviceCanvas.height(); // one region's own pixel size (deviceCanvas is 3*unit x unit)

        bool leftSide;
        int panelX;
        if (deviceX < unit)
        {
            leftSide = true;
            panelX = deviceX;
        }
        else if (deviceX >= 2 * unit)
        {
            leftSide = false;
            panelX = deviceX - 2 * unit;
        }
        else
        {
            return std::nullopt; // the monitor/card region -- not clickable yet
        }

        std::optional<KeyRect> key = hitTestPanel(leftSide, unit, {panelX, deviceY});
        if (!key)
            return std::nullopt;
        return PressedKey{*key, leftSide};
    };

    // Press hit-tests and remembers the key (no Cursor call yet -- see
    // keyboardPanel.h's resolveKeyGesture for why: a plain tap and a
    // press-drag-release chord need both ends of the gesture to tell
    // apart). Release hit-tests again, resolves the gesture, and replays
    // whatever KeyEvents it produced through Cursor exactly as
    // tests/cursorTests.cpp already does by hand. Always redraws so the
    // pressed/latched highlight (drawKeyboardPanel, via renderContent)
    // stays live even when a gesture cancels.
    auto onClick = [&](int x_px, int y_px, bool pressed)
    {
        if (pressed)
        {
            pressedKey = hitTestClick(x_px, y_px);
            redraw();
            return;
        }

        if (pressedKey)
        {
            std::optional<PressedKey> released = hitTestClick(x_px, y_px);
            std::optional<KeyRect> releasedKey;
            bool releasedLeftSide = false;
            if (released)
            {
                releasedKey = released->key;
                releasedLeftSide = released->leftSide;
            }

            GestureOutcome outcome = resolveKeyGesture(pressedKey->key, pressedKey->leftSide, releasedKey,
                                                         releasedLeftSide, capsLatched, shiftLatched,
                                                         cursor.isTypingMode());
            for (const KeyEvent& event : outcome.events)
            {
                // Most of these have to be read *before* dispatch: unlike
                // e/d (whose target -- the current card -- never changes
                // on a refusal, only on a success), a successful 'j' pops
                // link history, a successful i/k just moves the link
                // selection, and a successful m/./u/o/i/k/j/l just moves
                // the cursor/current card -- none of that necessarily
                // leaves state distinguishable afterward the way
                // canEdit()/canDelete() do (a successful 'j' that empties
                // the history in the process would otherwise look
                // identical to a refused one). isLinkMode is captured
                // too, since i/k/j mean something else entirely outside
                // Navigation mode -- see KeyMessage's own comment.
                bool wasLinkMode = cursor.isLinkMode();
                bool cardWasReadOnly = !cursor.currentCard()->canEdit();
                bool backWasDisabled = !cursor.hasLinkHistory();
                bool prevLinkWasDisabled = cursor.currentCard()->isAtFirstLink();
                bool nextLinkWasDisabled = cursor.currentCard()->isAtLastLink();
                bool prevThreadWasDisabled = !cursor.hasPrevThreadCard();
                bool nextThreadWasDisabled = !cursor.hasNextThreadCard();
                bool prevCardWasDisabled = cursor.isAtFirstCard();
                bool nextCardWasDisabled = cursor.isAtLastCard();

                cursor.handleKey(event);

                // Each explanation shows for a few seconds on whichever
                // key was just refused -- e/d via CardItem::canEdit()/
                // canDelete(), queried fresh right after the dispatch
                // above (still false here means this exact attempt was
                // the one that got refused, not some earlier unrelated
                // state); everything else via the pre-dispatch snapshot
                // above, since Cursor::up()/down()/left()/right()
                // themselves now refuse (see cursor.cpp) rather than
                // silently moving, so the card's canEdit() can't have
                // changed either way. Only one message at a time, for
                // simplicity; messageGeneration lets a second click (of
                // any key) invalidate whichever scheduleOnce() callback is
                // already pending for the first, so it can't clear a
                // *newer* message out from under it later.
                bool isChar = event.kind == KeyEvent::Kind::Char;
                std::optional<KeyMessage> refusal;
                if (isChar && event.codepoint == U'e' && !cursor.currentCard()->canEdit())
                    refusal = KeyMessage{U'e', false, U"Read-Only"};
                else if (isChar && event.codepoint == U'd' && !cursor.currentCard()->canDelete())
                    refusal = KeyMessage{U'd', false, U"Read-Only"};
                else if (wasLinkMode && isChar && event.codepoint == U'j' && backWasDisabled)
                    refusal = KeyMessage{U'j', true, U"No history"};
                else if (wasLinkMode && isChar && event.codepoint == U'i' && prevLinkWasDisabled)
                    refusal = KeyMessage{U'i', true, U"No links"};
                else if (wasLinkMode && isChar && event.codepoint == U'k' && nextLinkWasDisabled)
                    refusal = KeyMessage{U'k', true, U"No links"};
                else if (!wasLinkMode && isChar && event.codepoint == U'm' && prevThreadWasDisabled)
                    refusal = KeyMessage{U'm', false, U"No prevT"};
                else if (!wasLinkMode && isChar && event.codepoint == U'.' && nextThreadWasDisabled)
                    refusal = KeyMessage{U'.', false, U"No nextT"};
                else if (!wasLinkMode && isChar && event.codepoint == U'u' && prevCardWasDisabled)
                    refusal = KeyMessage{U'u', false, U"No prev"};
                else if (!wasLinkMode && isChar && event.codepoint == U'o' && nextCardWasDisabled)
                    refusal = KeyMessage{U'o', false, U"No next"};
                else if (!wasLinkMode && isChar && cardWasReadOnly &&
                         (event.codepoint == U'i' || event.codepoint == U'k' || event.codepoint == U'j' ||
                          event.codepoint == U'l'))
                    refusal = KeyMessage{event.codepoint, false, U"Read-Only"};

                if (refusal)
                {
                    keyMessage = refusal;
                    int myGeneration = ++messageGeneration;
                    window.scheduleOnce(1000,
                                         [&, myGeneration]()
                                         {
                                             if (myGeneration != messageGeneration)
                                                 return; // a newer click already superseded this one
                                             keyMessage.reset();
                                             redraw();
                                         });
                }
            }
            // Cursor's own isCommandMode() -- queried fresh after
            // replaying every event above -- rather than
            // outcome.capsLatched: a plain tap's blind "flip the latch"
            // guess (resolveKeyGesture has no Cursor reference, so it
            // can't know any better) is only right while Command mode is
            // a strict two-state toggle. Tapping cmd from Navigation mode
            // steps up to general command instead of leaving Command mode
            // entirely (see cursor.cpp), so this must stay true there too
            // -- it no longer drives cmd's own rendering (see
            // drawKeyboardPanel's comment: cmd never shows a persistent
            // latched look, only a momentary press/release flash), but
            // resolveKeyGesture's chord dispatch still needs an accurate
            // "is command mode currently latched" to decide whether a
            // hold-cmd-drag-release chord should fire the chorded key
            // directly or synthesize a full press/release pair around it.
            capsLatched = cursor.isCommandMode();
            shiftLatched = outcome.shiftLatched;
        }
        pressedKey.reset();
        redraw();
    };

    // Updates hoveredKey and redraws, but only on an actual transition
    // (see sameHoveredSpot's comment) -- onMouseMove fires far more often
    // than the panel's key boundaries change, so redrawing unconditionally
    // here would be pure waste on an otherwise-static panel.
    auto onMouseMove = [&](int x_px, int y_px)
    {
        std::optional<PressedKey> hit = hitTestClick(x_px, y_px);
        if (sameHoveredSpot(hit, hoveredKey))
            return;
        hoveredKey = hit;
        redraw();
    };

    // Flashes the on-screen key matching a physical keypress -- see
    // platform.h's onPhysicalKey. Shift has no KeyRect identity of its
    // own to look up (see findPhysicalKey's comment), just the shared
    // shiftEngaged bool. Guards against redundant redraws two ways: a
    // held key's OS auto-repeat re-sends the same press over and over
    // (sameHoveredSpot already true, so a no-op), and a release is only
    // honored if it matches whatever's currently flashed -- otherwise
    // it's a stale release for a key some *other* physical press already
    // overtook (fast rollover while typing), and clearing the newer
    // press's own flash early would be wrong.
    auto onPhysicalKey = [&](const KeyEvent& event)
    {
        if (event.kind == KeyEvent::Kind::Shift)
        {
            if (physicalShiftHeld == event.pressed)
                return;
            physicalShiftHeld = event.pressed;
            redraw();
            return;
        }

        std::optional<PressedKey> found = findPhysicalKey(event);
        if (!found)
            return; // a physical key this panel doesn't show at all

        if (event.pressed)
        {
            if (sameHoveredSpot(found, physicalPressedKey))
                return;
            physicalPressedKey = found;
        }
        else
        {
            if (!sameHoveredSpot(found, physicalPressedKey))
                return;
            physicalPressedKey.reset();
        }
        redraw();
    };

    redraw();
    window.setTitle(titleFor(unitPx(currentWidth_px, currentHeight_px), dpi, dpiKnown));

    window.run(
        [&](const KeyEvent& event)
        {
            if (event.kind == KeyEvent::Kind::Calibrate)
            {
                // The window's current constraining dimension IS
                // Monitor::kWidth_in (one device region, by the user's own
                // ruler) -- back-derive and persist whatever dpi makes
                // that true, rather than trusting the OS. Not routed
                // through Cursor: this is a window-physical-size concern,
                // not a card-editing one.
                int unit = unitPx(currentWidth_px, currentHeight_px);
                dpi = static_cast<int>(std::lround(unit / Monitor::kWidth_in));
                dpiKnown = true; // user-confirmed via ruler now, whatever it was before
                saveCalibratedDpi(dpi);
                window.setTitle(titleFor(unit, dpi, dpiKnown));
                return;
            }
            cursor.handleKey(event);
            redraw();
        },
        [&](int newWidth_px, int newHeight_px)
        {
            // Cheap per-tick path (see platform.h's run() comment): skips
            // renderContent entirely (the card's own pixels haven't
            // changed, only the window's shape) and presents with
            // nearest-neighbor scaling, not bilinear -- src is often
            // being stretched by a large, constantly-changing factor
            // here, and a live drag needs this fast enough to keep up
            // with every tick, not just visually smooth once it lands.
            // Bilinear's extra quality is worth paying for once, in
            // onResizeEnd below, not on every one of these.
            currentWidth_px = newWidth_px;
            currentHeight_px = newHeight_px;
            presentFrame(false);
            window.setTitle(titleFor(unitPx(newWidth_px, newHeight_px), dpi, dpiKnown));
        },
        [&](int newWidth_px, int newHeight_px)
        {
            // Fires once a resize actually settles (see platform.h's
            // run() comment) -- this is where the atlas actually changes
            // and the card/panels get rebuilt at native resolution, not
            // on every onResize tick above. Skips all of that if it's
            // already done it for this exact size (see
            // lastSettledWidth_px's comment): a platform shell guarantees
            // onResizeEnd fires once per completed resize, but not that
            // it can never also fire again afterward for a size that
            // hasn't actually changed.
            if (newWidth_px == lastSettledWidth_px && newHeight_px == lastSettledHeight_px)
                return;
            lastSettledWidth_px = newWidth_px;
            lastSettledHeight_px = newHeight_px;

            currentWidth_px = newWidth_px;
            currentHeight_px = newHeight_px;
            int unit = unitPx(newWidth_px, newHeight_px);
            atlas = &pickAtlas(desiredCellWidth_px(unit));
            titleAtlas = &pickAtlas(atlas->cellWidth * 2); // see its selection above
            cardCanvas =
                Canvas(cursor.currentCard()->cardWidth_px(*atlas), cursor.currentCard()->cardHeight_px(*atlas));
            leftPanelAtlas = &pickPanelAtlas(cardCanvas.width());
            rightPanelAtlas = &pickPanelAtlas(cardCanvas.width());
            redraw(); // renderContent rebuilds/redraws the panels at the new resolution
            window.setTitle(titleFor(unit, dpi, dpiKnown));
        },
        onClick, onMouseMove, onPhysicalKey);

    return 0;
}
