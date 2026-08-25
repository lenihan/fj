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

// "fj (emulated) - 5.00"x5.00" 100% -- press F5 to fix calibration" --
// describes the *monitor* (Monitor::kWidth_in/kHeight_in), not the card
// on it, since the window represents the monitor now (see this file's top
// comment). squareSize_px is the rendered square monitor's actual on-
// screen size -- min(window width, window height), since the window
// itself can be any shape now (see createPlatformWindow's comment) and
// the square is letterboxed/pillarboxed to fit whichever dimension is
// smaller (see main()'s redraw). percent is that against Monitor::kWidth_in
// at the display's true DPI. ceil matches the old Qt app's rounding
// (PLAN.md): never displays a percent lower than what's actually
// achieved. The trailing hint exists purely so
// KeyEvent::Kind::Calibrate is discoverable at all -- there's no other UI
// surface pointing at it. F5, not a platform-specific affordance like
// win32Window.cpp's "Fix Calibration..." system-menu item: this file has
// no #ifdef/platform header of its own (see the top comment) and stays
// that way, so the one thing it advertises has to actually work
// identically on every shell -- Xlib has no portable equivalent of
// GetSystemMenu to hang a second hint off of. "(emulated)" is there so
// the title itself keeps making the point this file's top comment makes.
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
std::string titleFor(int squareSize_px, int dpi, bool dpiKnown)
{
    char buf[128];
    if (!dpiKnown)
    {
        std::snprintf(buf, sizeof(buf), "fj (emulated) - %.0f\"x%.0f\" size unconfirmed -- press F5 once sized correctly",
                      Monitor::kWidth_in, Monitor::kHeight_in);
        return buf;
    }
    double percent = squareSize_px / (dpi * Monitor::kWidth_in) * 100.0;
    std::snprintf(buf, sizeof(buf), "fj (emulated) - %.0f\"x%.0f\" %d%% -- press F5 to fix calibration",
                  Monitor::kWidth_in, Monitor::kHeight_in, static_cast<int>(std::ceil(percent)));
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
    // Monitor::kWidth_in/kHeight_in). Doesn't need to land on an atlas-
    // exact pixel count the way an earlier version insisted on -- every
    // redraw (below) already fits its content to whatever size the
    // window actually is, startup included, so there's nothing special
    // to get right here beyond a reasonable starting size.
    int currentWidth_px = static_cast<int>(std::lround(dpi * Monitor::kWidth_in));
    int currentHeight_px = static_cast<int>(std::lround(dpi * Monitor::kHeight_in));

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

    // atlas/titleAtlas/cardCanvas are all sized for the square monitor's
    // *content* resolution, picked from whichever window dimension is
    // more constraining -- see redraw's letterboxing comment for why the
    // window itself no longer has to be square for this to work.
    const HackAtlas::Atlas* atlas = &pickAtlas(desiredCellWidth_px(std::min(currentWidth_px, currentHeight_px)));

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

    // Square, cardCanvas.width() on a side: Monitor::kWidth_in ==
    // Card::kWidth_in, so at whatever resolution the atlas rendered the
    // card's width, that same pixel count is exactly Monitor::kHeight_in
    // too (both 5in). A persistent Canvas, not a fresh local like
    // cardCanvas -- renderContent (below) is the only thing that rebuilds
    // it, so presentFrame can reuse whatever it last drew across any
    // number of live-resize ticks without redoing that work.
    Canvas monitorCanvas(cardCanvas.width(), cardCanvas.width());

    // Draws the card and composites it onto monitorCanvas -- the
    // "content changed" half of a frame, as opposed to presentFrame's
    // "put it on screen" half below. Only needed when the card's own
    // pixels actually changed (typing/navigation) or cardCanvas itself
    // was just rebuilt at a new atlas (a resize settling); a live-resize
    // tick's window-shape-only change never needs this, which is the
    // whole reason it's split out rather than folded into presentFrame.
    auto renderContent = [&]()
    {
        cursor.draw(cardCanvas, *atlas, *titleAtlas);
        monitorCanvas = Canvas(cardCanvas.width(), cardCanvas.width());
        monitorCanvas.blit(cardCanvas, {0, (monitorCanvas.height() - cardCanvas.height()) / 2});
    };

    // Fits monitorCanvas into the window's actual shape -- which no
    // longer has to be square itself (see createPlatformWindow's
    // comment) -- letterboxed/pillarboxed rather than distorted: the
    // largest centered square that fits, black filling whatever margin
    // that leaves on the wider axis. Built at exactly the window's
    // current size and handed to present() as-is, so the platform
    // shell's own stretch (still there as a defensive fallback -- see
    // platform.h) is normally a no-op, not a second resample on top of
    // this one. smooth is Canvas::blitScaled's -- see its comment for
    // why a live-resize tick needs false here, not the general redraw
    // default of true.
    auto presentFrame = [&](bool smooth)
    {
        Canvas outputCanvas(currentWidth_px, currentHeight_px);
        outputCanvas.fillRect({0, 0, currentWidth_px, currentHeight_px}, 0x00000000);
        int squareSize = std::min(currentWidth_px, currentHeight_px);
        Rect squareRect{(currentWidth_px - squareSize) / 2, (currentHeight_px - squareSize) / 2, squareSize,
                         squareSize};
        outputCanvas.blitScaled(monitorCanvas, squareRect, smooth);
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

    redraw();
    window.setTitle(titleFor(std::min(currentWidth_px, currentHeight_px), dpi, dpiKnown));

    window.run(
        [&](const KeyEvent& event)
        {
            if (event.kind == KeyEvent::Kind::Calibrate)
            {
                // The window's current constraining dimension IS
                // Monitor::kWidth_in, by the user's own ruler -- back-
                // derive and persist whatever dpi makes that true, rather
                // than trusting the OS. Not routed through Cursor: this
                // is a window-physical-size concern, not a card-editing
                // one.
                int squareSize = std::min(currentWidth_px, currentHeight_px);
                dpi = static_cast<int>(std::lround(squareSize / Monitor::kWidth_in));
                dpiKnown = true; // user-confirmed via ruler now, whatever it was before
                saveCalibratedDpi(dpi);
                window.setTitle(titleFor(squareSize, dpi, dpiKnown));
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
            window.setTitle(titleFor(std::min(newWidth_px, newHeight_px), dpi, dpiKnown));
        },
        [&](int newWidth_px, int newHeight_px)
        {
            // Fires once a resize actually settles (see platform.h's
            // run() comment) -- this is where the atlas actually changes
            // and the card gets rebuilt at native resolution, not on
            // every onResize tick above. Skips all of that if it's
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
            atlas = &pickAtlas(desiredCellWidth_px(std::min(newWidth_px, newHeight_px)));
            titleAtlas = &pickAtlas(atlas->cellWidth * 2); // see its selection above
            cardCanvas =
                Canvas(cursor.currentCard()->cardWidth_px(*atlas), cursor.currentCard()->cardHeight_px(*atlas));
            redraw();
            window.setTitle(titleFor(std::min(newWidth_px, newHeight_px), dpi, dpiKnown));
        });

    return 0;
}
