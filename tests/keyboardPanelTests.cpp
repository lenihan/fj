// keyboardPanelTests.cpp -- automated regression tests for layoutKeys
// (see PLAN.md's "Emulator keyboard panels" note). layoutKeys is a pure
// function of (leftSide, panelSize_px) with no Cursor/window dependency
// (see keyboardPanel.h), so -- like cursorTests.cpp -- this needs no
// PlatformWindow, no window, no platform shell code at all.

#include "keyboardPanel.h"
#include "layout.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace
{
constexpr int kPanelSize_px = 500; // an arbitrary, reasonably large panel resolution
}

TEST_CASE("layoutKeys returns the full grid plus one spacebar key")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    REQUIRE(keys.size() == KeyboardPanel::kCols * KeyboardPanel::kKeyRows + 1);
}

TEST_CASE("layoutKeys' spacebar key is double-width and aligned to the screen side")
{
    SECTION("left panel: spacebar sits at the panel's right edge")
    {
        auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
        const KeyRect& spacebar = keys.back();
        CHECK(spacebar.label == U"spacebar");

        // Double-width: noticeably wider than an ordinary single key.
        const KeyRect& ordinaryKey = keys.front();
        CHECK(spacebar.rect.w > ordinaryKey.rect.w * 1.5);

        // Right-aligned: its right edge should match the rightmost
        // ordinary key's right edge in the same grid.
        int rightmostOrdinaryEdge = 0;
        for (std::size_t i = 0; i + 1 < keys.size(); ++i)
            rightmostOrdinaryEdge = std::max(rightmostOrdinaryEdge, keys[i].rect.x + keys[i].rect.w);
        CHECK(spacebar.rect.x + spacebar.rect.w == rightmostOrdinaryEdge);
    }

    SECTION("right panel: spacebar sits at the panel's left edge")
    {
        auto keys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
        const KeyRect& spacebar = keys.back();
        CHECK(spacebar.label == U"spacebar");

        int leftmostOrdinaryEdge = kPanelSize_px;
        for (std::size_t i = 0; i + 1 < keys.size(); ++i)
            leftmostOrdinaryEdge = std::min(leftmostOrdinaryEdge, keys[i].rect.x);
        CHECK(spacebar.rect.x == leftmostOrdinaryEdge);
    }
}

TEST_CASE("layoutKeys keeps every key within the panel's bounds")
{
    for (bool leftSide : {true, false})
    {
        auto keys = layoutKeys(leftSide, kPanelSize_px);
        for (const KeyRect& key : keys)
        {
            CHECK(key.rect.x >= 0);
            CHECK(key.rect.y >= 0);
            CHECK(key.rect.x + key.rect.w <= kPanelSize_px);
            CHECK(key.rect.y + key.rect.h <= kPanelSize_px);
        }
    }
}

TEST_CASE("layoutKeys' left and right panels are mirror images with different labels")
{
    auto leftKeys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
    REQUIRE(leftKeys.size() == rightKeys.size());

    // Same geometry (grid position/size), since both panels are the same
    // physical shape -- only the labels differ between the two sides.
    bool anyLabelDiffers = false;
    for (std::size_t i = 0; i < leftKeys.size(); ++i)
    {
        CHECK(leftKeys[i].rect.w == rightKeys[i].rect.w);
        CHECK(leftKeys[i].rect.h == rightKeys[i].rect.h);
        if (leftKeys[i].label != rightKeys[i].label)
            anyLabelDiffers = true;
    }
    CHECK(anyLabelDiffers);
}
