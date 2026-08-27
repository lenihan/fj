// keyboardPanelTests.cpp -- automated regression tests for layoutKeys
// (see PLAN.md's "Emulator keyboard panels" note). layoutKeys is a pure
// function of (leftSide, panelSize_px) with no Cursor/window dependency
// (see keyboardPanel.h), so -- like cursorTests.cpp -- this needs no
// PlatformWindow, no window, no platform shell code at all.

#include "keyboardPanel.h"
#include "layout.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>
#include <vector>

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

TEST_CASE("each key's label maps to the KeyEvent a physical keypress of it would send")
{
    auto find = [](const std::vector<KeyRect>& keys, std::u32string_view label) -> const KeyRect&
    {
        auto it = std::ranges::find(keys, label, &KeyRect::label);
        REQUIRE(it != keys.end());
        return *it;
    };

    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);

    SECTION("caps sends CapsLock -- the one key whose release matters too")
    {
        const KeyRect& caps = find(keys, U"caps");
        CHECK(caps.clickable);
        CHECK(caps.kind == KeyEvent::Kind::CapsLock);
    }

    SECTION("tab and shift have no mapped action yet -- a click on either is a no-op")
    {
        CHECK_FALSE(find(keys, U"tab").clickable);
        CHECK_FALSE(find(keys, U"shift").clickable);
    }

    SECTION("a single-codepoint label sends exactly that character")
    {
        const KeyRect& q = find(keys, U"q");
        CHECK(q.clickable);
        CHECK(q.kind == KeyEvent::Kind::Char);
        CHECK(q.codepoint == U'q');
    }

    SECTION("spacebar sends a literal space character")
    {
        const KeyRect& spacebar = keys.back();
        CHECK(spacebar.label == U"spacebar");
        CHECK(spacebar.clickable);
        CHECK(spacebar.kind == KeyEvent::Kind::Char);
        CHECK(spacebar.codepoint == U' ');
    }

    SECTION("enter and bs (right panel) send Enter/Backspace")
    {
        auto rightKeys = layoutKeys(/*leftSide=*/false, kPanelSize_px);
        CHECK(find(rightKeys, U"enter").kind == KeyEvent::Kind::Enter);
        CHECK(find(rightKeys, U"bs").kind == KeyEvent::Kind::Backspace);
    }
}

TEST_CASE("hitTestPanel finds the key under a point, and nothing between keys")
{
    auto keys = layoutKeys(/*leftSide=*/true, kPanelSize_px);
    const KeyRect& someKey = keys[7]; // an arbitrary ordinary (non-spacebar) key
    Point center{someKey.rect.x + someKey.rect.w / 2, someKey.rect.y + someKey.rect.h / 2};

    auto hit = hitTestPanel(/*leftSide=*/true, kPanelSize_px, center);
    REQUIRE(hit.has_value());
    CHECK(hit->label == someKey.label);

    // Outside the whole grid entirely -- panelSize_px is well beyond the
    // margin layoutKeys centers the grid within.
    CHECK_FALSE(hitTestPanel(/*leftSide=*/true, kPanelSize_px, {0, 0}).has_value());
}
