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
#include <utility>

namespace
{

// Same arithmetic as cursor.cpp's private cardWidth_px/cardHeight_px --
// duplicated rather than exposed on CardItem, since this is the only other
// caller and it doesn't need general access to a card's pixel geometry.
int cardWidth_px(const CardItem& card, int scale)
{
    return Body::kColsPerRow * card.cellWidth_px(1, scale);
}

int cardHeight_px(const CardItem& card, int scale)
{
    Row lastRow = Card::kNumRows - 1;
    return card.rowTop_px(lastRow, scale) + card.cellHeight_px(lastRow, scale);
}

// The Hack atlas is fixed-pixel, but the card is a physical-inch target
// (layout.h): pick whichever integer scale puts the card's rendered width
// closest to Card::kWidth_in at the display's actual DPI. Width alone is
// enough to drive this -- cardWidth_px/cardHeight_px share a fixed aspect
// ratio, so both track together as scale changes.
int bestFitScale(const CardItem& card, int dpi)
{
    double targetWidth_px = dpi * Card::kWidth_in;

    int bestScale = 1;
    double bestDelta = std::abs(cardWidth_px(card, 1) - targetWidth_px);
    for (int scale = 2; scale <= 20; ++scale)
    {
        double delta = std::abs(cardWidth_px(card, scale) - targetWidth_px);
        if (delta < bestDelta)
        {
            bestDelta = delta;
            bestScale = scale;
        }
    }
    return bestScale;
}

} // namespace

int main()
{
    Cursor cursor;

    int scale = bestFitScale(*cursor.currentCard(), displayDpi());
    int width_px = cardWidth_px(*cursor.currentCard(), scale);
    int height_px = cardHeight_px(*cursor.currentCard(), scale);

    auto windowResult = createPlatformWindow(width_px, height_px, "fj");
    if (!windowResult)
    {
        std::fprintf(stderr, "fj: failed to create window: %s\n", windowResult.error().c_str());
        return 1;
    }
    PlatformWindow window = std::move(*windowResult);

    Canvas canvas(width_px, height_px);

    auto redraw = [&]()
    {
        cursor.draw(canvas, scale);
        window.present(canvas.pixels(), canvas.width(), canvas.height());
    };

    redraw();
    window.run(
        [&](const KeyEvent& event)
        {
            cursor.handleKey(event);
            redraw();
        });

    return 0;
}
