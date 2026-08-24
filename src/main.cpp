// main.cpp -- composes the core (Cursor/Canvas) with a platform shell
// (platform.h) into a running app. Deliberately has no #ifdef/platform
// header of its own: everything it touches is the platform.h contract, so
// this file is meant to be shared as-is once Linux/web shells exist (see
// PLAN_addendum.md), not rewritten per platform.

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

// Same arithmetic as cursor.cpp's private cardWidth_px/cardHeight_px --
// duplicated rather than exposed on CardItem, since this is the only other
// caller and it doesn't need general access to a card's pixel geometry.
int cardWidth_px(const CardItem& card, const HackAtlas::Atlas& atlas)
{
    return Body::kColsPerRow * card.cellWidth_px(1, atlas);
}

int cardHeight_px(const CardItem& card, const HackAtlas::Atlas& atlas)
{
    Row lastRow = Card::kNumRows - 1;
    return card.rowTop_px(lastRow, atlas) + card.cellHeight_px(lastRow, atlas);
}

// The Body-cell pixel width a given on-screen pixel width implies -- same
// relationship tools/offline/bakeFont used to choose each atlas's size in
// the first place, so feeding it back into pickAtlas finds whichever baked
// atlas best matches how big the window currently is.
int desiredCellWidth_px(int width_px)
{
    return width_px / Body::kColsPerRow;
}

// "fj - 5.00"x3.00" 100% -- right-click titlebar to fix calibration" --
// percent is width_px (the window's actual current width) against
// Card::kWidth_in at the display's true DPI. Width only: since
// CardItem::cellHeight_px anchors row height to Card::kHeight_in too
// (see cardItem.cpp), a height-based percent would track this one
// almost exactly anyway -- width alone is simpler and matches the
// precedent already set by tools/offline/bakeFont (which only ever
// targets width when choosing what to bake) and createPlatformWindow's
// aspectRatio parameter. ceil matches the old Qt app's rounding
// (PLAN.md): never displays a percent lower than what's actually
// achieved. The trailing hint exists purely so win32Window.cpp's
// system-menu "Fix Calibration..." item (see its addCalibrateMenuItem
// comment) is discoverable at all -- there's no other UI surface
// pointing at it.
std::string titleFor(int width_px, int dpi)
{
    double percent = width_px / (dpi * Card::kWidth_in) * 100.0;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "fj - %.2f\"x%.2f\" %d%% -- right-click titlebar to fix calibration",
                  Card::kWidth_in, Card::kHeight_in, static_cast<int>(std::ceil(percent)));
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
    // target -- exactly Card::kWidth_in/kHeight_in inches at this
    // display's DPI -- not whatever pixel size the nearest baked atlas
    // happens to render at. The initial canvas below is still rendered at
    // that nearest atlas's own resolution (so it's sharp), and
    // present()'s stretch -- the same mechanism every resize already
    // relies on -- covers the small gap between the two. Now that
    // CardItem::cellHeight_px anchors row height to Card::kHeight_in
    // (see cardItem.cpp), that gap is a uniform few percent, not a shape
    // mismatch, so the stretch doesn't distort anything.
    int width_px = static_cast<int>(std::lround(dpi * Card::kWidth_in));
    int height_px = static_cast<int>(std::lround(dpi * Card::kHeight_in));
    const HackAtlas::Atlas* atlas = &pickAtlas(desiredCellWidth_px(width_px));

    double aspectRatio = Card::kWidth_in / Card::kHeight_in;

    auto windowResult = createPlatformWindow(width_px, height_px, aspectRatio, "fj");
    if (!windowResult)
    {
        std::fprintf(stderr, "fj: failed to create window: %s\n", windowResult.error().c_str());
        return 1;
    }
    PlatformWindow window = std::move(*windowResult);

    Canvas canvas(cardWidth_px(*cursor.currentCard(), *atlas), cardHeight_px(*cursor.currentCard(), *atlas), *atlas);

    int currentWidth_px = width_px; // tracked for Kind::Calibrate below

    auto redraw = [&]()
    {
        cursor.draw(canvas, *atlas);
        window.present(canvas.pixels(), canvas.width(), canvas.height());
    };

    redraw();
    window.setTitle(titleFor(width_px, dpi));

    window.run(
        [&](const KeyEvent& event)
        {
            if (event.kind == KeyEvent::Kind::Calibrate)
            {
                // The window's current width IS Card::kWidth_in, by the
                // user's own ruler -- back-derive and persist whatever
                // dpi makes that true, rather than trusting the OS. Not
                // routed through Cursor: this is a window-physical-size
                // concern, not a card-editing one.
                dpi = static_cast<int>(std::lround(currentWidth_px / Card::kWidth_in));
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
            canvas = Canvas(cardWidth_px(*cursor.currentCard(), *atlas), cardHeight_px(*cursor.currentCard(), *atlas),
                             *atlas);
            redraw();
            window.setTitle(titleFor(newWidth_px, dpi));
        });

    return 0;
}
