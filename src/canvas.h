// canvas.h -- fj's own software rasterizer: the only way core code draws
// pixels. It's what src/cursor.cpp and src/cardItem.cpp's draw methods get
// rewritten against, and what the future RowItem replacement draws text
// through.
//
// Deliberately minimal, derived from what those files actually draw today
// (see PLAN_addendum.md), not a general 2D API: fillRect, fillTriangle,
// line (built from two filled triangles -- see canvas.cpp), and glyph/text
// drawing from the baked Hack atlas (hackAtlas.h). No anti-aliasing, no
// rounded corners, no line caps, no clip regions beyond the canvas bounds
// -- all deferred the same way non-blocky font scaling was.
//
// Canvas works entirely in pixel (_view) coordinates. Converting from the
// scene's inch-based coordinate system (_scen -- see PLAN.md's
// "Coordinate System" section) is the caller's job, not this header's.

#pragma once

#include "platform.h"

#include <cstdint>
#include <span>
#include <vector>

struct Point
{
    int x{0};
    int y{0};
};

struct Rect
{
    int x{0};
    int y{0};
    int w{0};
    int h{0};
};

namespace HackAtlas
{
struct Glyph;
}

class Canvas
{
  public:
    Canvas(int width_px, int height_px);

    int width() const { return m_width; }
    int height() const { return m_height; }

    // Matches PlatformWindow::present()'s signature directly:
    //   window.present(canvas.pixels(), canvas.width(), canvas.height());
    std::span<const Pixel> pixels() const { return m_pixels; }

    void fillRect(Rect rect, Pixel color);
    void fillTriangle(Point p0, Point p1, Point p2, Pixel color);

    // A filled quad along the segment; no rounded caps (see the header
    // comment -- same deferred-polish precedent as font scaling).
    void line(Point p0, Point p1, Pixel color, int thickness);

    // Looks up codepoint in the baked Hack atlas and blits it at pos,
    // scaled by an integer factor (2 for Title rows reusing the Body
    // atlas, per PLAN_addendum.md). Silently no-ops if codepoint isn't
    // baked -- there are only 97 of them (ASCII 0x20-0x7E plus the two
    // link arrows CardItem::linkStr() uses).
    void drawChar(char32_t codepoint, Point pos, Pixel color, int scale = 1);

    // Draws each codepoint left-to-right, advancing by the atlas cell
    // width per character -- fj is fixed-pitch-only, so this is genuinely
    // just repeated drawChar. Takes decoded codepoints rather than a
    // UTF-8/QString byte sequence so this header doesn't need an opinion
    // on string encoding; that's the core model's call (phase 3), not the
    // renderer's.
    void drawText(std::span<const char32_t> text, Point pos, Pixel color, int scale = 1);

  private:
    void blitGlyph(const HackAtlas::Glyph& glyph, Point pos, Pixel color, int scale);

    int m_width;
    int m_height;
    std::vector<Pixel> m_pixels;
};
