// main.cpp -- composes the core (Cursor/Canvas) with a platform shell
// (platform.h) into a running app. Deliberately has no #ifdef/platform
// header of its own: everything it touches is the platform.h contract, so
// this file is meant to be shared as-is once Linux/web shells exist (see
// PLAN_addendum.md), not rewritten per platform.
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
#include "layout.h"
#include "platform.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

// "fj (emulated) - 5.00"x5.00" 100% -- right-click titlebar to fix
// calibration" -- describes the *monitor* (Monitor::kWidth_in/kHeight_in),
// not the card on it, since the window represents the monitor now (see
// this file's top comment). percent is width_px (the window's actual
// current width) against Monitor::kWidth_in at the display's true DPI.
// Width only: Monitor::kWidth_in == Card::kWidth_in, and
// CardItem::cellHeight_px anchors row height to Card::kHeight_in, so a
// height-based percent would track this one almost exactly anyway --
// width alone is simpler and matches the precedent already set by
// tools/offline/bakeFont (which only ever targets width when choosing
// what to bake) and createPlatformWindow's aspectRatio parameter. ceil
// matches the old Qt app's rounding (PLAN.md): never displays a percent
// lower than what's actually achieved. The trailing hint exists purely so
// win32Window.cpp's system-menu "Fix Calibration..." item (see its
// addCalibrateMenuItem comment) is discoverable at all -- there's no
// other UI surface pointing at it. "(emulated)" is there so the title
// itself keeps making the point this file's top comment makes.
std::string titleFor(int width_px, int dpi)
{
    double percent = width_px / (dpi * Monitor::kWidth_in) * 100.0;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "fj (emulated) - %.0f\"x%.0f\" %d%% -- right-click titlebar to fix calibration",
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
    // std::getenv is the portable standard call (this file is meant to be
    // shared as-is on Linux/web -- see the file comment above); MSVC's
    // /W4 flags it as unsafe purely because of a Windows-specific
    // thread-safety caveat that doesn't apply to this single-threaded,
    // read-immediately-and-discard use.
#pragma warning(push)
#pragma warning(disable : 4996)
    const char* appData = std::getenv("APPDATA");
#pragma warning(pop)
    if (!appData)
        return {};
    return std::filesystem::path(appData) / "fj" / "calibration.txt";
}

int startupDpi()
{
    auto path = calibrationFilePath();
    if (!path.empty())
    {
        std::ifstream in(path);
        double dpi = 0.0;
        if (in >> dpi && dpi > 0.0)
            return static_cast<int>(std::lround(dpi));
    }
    return displayDpi();
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
    int dpi = startupDpi();

    // The window always opens at the true, continuous 100% physical
    // target -- exactly Monitor::kWidth_in/kHeight_in inches at this
    // display's DPI -- not whatever pixel size the nearest baked atlas
    // happens to render at. The initial card canvas below is still
    // rendered at that nearest atlas's own resolution (so it's sharp),
    // and present()'s stretch -- the same mechanism every resize already
    // relies on -- covers the small gap between the two.
    int width_px = static_cast<int>(std::lround(dpi * Monitor::kWidth_in));
    int height_px = static_cast<int>(std::lround(dpi * Monitor::kHeight_in));
    const HackAtlas::Atlas* atlas = &pickAtlas(desiredCellWidth_px(width_px));

    double aspectRatio = Monitor::kWidth_in / Monitor::kHeight_in;

    auto windowResult = createPlatformWindow(width_px, height_px, aspectRatio, "fj");
    if (!windowResult)
    {
        std::fprintf(stderr, "fj: failed to create window: %s\n", windowResult.error().c_str());
        return 1;
    }
    PlatformWindow window = std::move(*windowResult);

    Canvas cardCanvas(cursor.currentCard()->cardWidth_px(*atlas), cursor.currentCard()->cardHeight_px(*atlas), *atlas);

    int currentWidth_px = width_px; // tracked for Kind::Calibrate below

    auto redraw = [&]()
    {
        cursor.draw(cardCanvas, *atlas);

        // The monitor canvas is square, cardCanvas.width() on a side:
        // Monitor::kWidth_in == Card::kWidth_in, so at whatever
        // resolution the atlas rendered the card's width, that same
        // pixel count is exactly Monitor::kHeight_in too (both 5in).
        // That's also why only vertical centering is needed below -- the
        // card already spans the monitor's full width. Rebuilt fresh
        // every redraw (not just on resize) so there's never a stale
        // pixel left over from a previous, differently-sized card; it's
        // cheap (see the resize handler's own reasoning below).
        Canvas monitorCanvas(cardCanvas.width(), cardCanvas.width(), *atlas);
        monitorCanvas.blit(cardCanvas, {0, (monitorCanvas.height() - cardCanvas.height()) / 2});
        window.present(monitorCanvas.pixels(), monitorCanvas.width(), monitorCanvas.height());
    };

    redraw();
    window.setTitle(titleFor(width_px, dpi));

    window.run(
        [&](const KeyEvent& event)
        {
            if (event.kind == KeyEvent::Kind::Calibrate)
            {
                // The window's current width IS Monitor::kWidth_in, by
                // the user's own ruler -- back-derive and persist
                // whatever dpi makes that true, rather than trusting the
                // OS. Not routed through Cursor: this is a window-
                // physical-size concern, not a card-editing one.
                dpi = static_cast<int>(std::lround(currentWidth_px / Monitor::kWidth_in));
                saveCalibratedDpi(dpi);
                window.setTitle(titleFor(currentWidth_px, dpi));
                return;
            }
            cursor.handleKey(event);
            redraw();
        },
        [&](int newWidth_px, int newHeight_px)
        {
            // Re-render fully on every resize tick -- cheap enough (a card
            // is ~11 rows, well under a thousand glyph blits) that there's
            // no need to debounce or wait for the drag to settle. The
            // window's own live-client-rect stretch (win32Window.cpp)
            // covers the continuous, sub-atlas-granularity part; this is
            // what keeps the rendered content itself close to native
            // resolution rather than always stretching one small bitmap.
            (void)newHeight_px; // aspect-locked (WM_SIZING) -- width alone determines the atlas and canvas size
            currentWidth_px = newWidth_px;
            atlas = &pickAtlas(desiredCellWidth_px(newWidth_px));
            cardCanvas =
                Canvas(cursor.currentCard()->cardWidth_px(*atlas), cursor.currentCard()->cardHeight_px(*atlas), *atlas);
            redraw();
            window.setTitle(titleFor(newWidth_px, dpi));
        });

    return 0;
}
