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

        const Rect* leftPressedRect = (pressedKey && pressedKey->leftSide) ? &pressedKey->key.rect : nullptr;
        const Rect* rightPressedRect = (pressedKey && !pressedKey->leftSide) ? &pressedKey->key.rect : nullptr;

        // Live preview, not just post-gesture state: a key currently
        // mid-press (pressed but not yet released) previews its effect
        // immediately, the same way its own face already inverts before
        // release -- see keyboardPanel.h's drawKeyboardPanel comment.
        bool shiftEngaged = shiftLatched || (pressedKey && pressedKey->key.action == KeyRect::Action::ShiftToggle);
        bool commandModeForLegend =
            cursor.isCommandMode() || (pressedKey && pressedKey->key.action == KeyRect::Action::CapsToggle);
        bool isTypingModeForLegend = !commandModeForLegend;
        bool isLinkModeForLegend = cursor.isLinkMode(); // only meaningful once actually in command mode

        leftPanelCanvas = Canvas(cardCanvas.width(), cardCanvas.width());
        rightPanelCanvas = Canvas(cardCanvas.width(), cardCanvas.width());
        drawKeyboardPanel(leftPanelCanvas, /*leftSide=*/true, *leftPanelAtlas, leftPressedRect, capsLatched,
                           shiftEngaged, isTypingModeForLegend, isLinkModeForLegend);
        drawKeyboardPanel(rightPanelCanvas, /*leftSide=*/false, *rightPanelAtlas, rightPressedRect, capsLatched,
                           shiftEngaged, isTypingModeForLegend, isLinkModeForLegend);

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
                cursor.handleKey(event);
            capsLatched = outcome.capsLatched;
            shiftLatched = outcome.shiftLatched;
        }
        pressedKey.reset();
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
        onClick);

    return 0;
}
