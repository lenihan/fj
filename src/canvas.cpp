#include "canvas.h"
#include "hackAtlas.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{

// Signed area of the triangle a,b,c (positive/negative by winding order).
// Used both to test degeneracy and, per-pixel, to test which side of each
// edge a point falls on -- a point is inside the triangle iff it's on the
// same side of all three edges, regardless of the triangle's own winding.
long long edge(Point a, Point b, Point c)
{
    return static_cast<long long>(b.x - a.x) * (c.y - a.y) -
           static_cast<long long>(b.y - a.y) * (c.x - a.x);
}

const HackAtlas::Glyph* findGlyph(const HackAtlas::Atlas& atlas, char32_t codepoint)
{
    for (std::size_t i = 0; i < atlas.glyphCount; ++i)
        if (atlas.glyphs[i].codepoint == codepoint)
            return &atlas.glyphs[i];
    return nullptr;
}

} // namespace

const HackAtlas::Atlas& pickAtlas(int desiredCellWidth_px)
{
    const HackAtlas::Atlas* best = &HackAtlas::kAtlases[0];
    int bestDelta = std::abs(best->cellWidth - desiredCellWidth_px);
    for (std::size_t i = 1; i < HackAtlas::kAtlasCount; ++i)
    {
        int delta = std::abs(HackAtlas::kAtlases[i].cellWidth - desiredCellWidth_px);
        if (delta < bestDelta)
        {
            bestDelta = delta;
            best = &HackAtlas::kAtlases[i];
        }
    }
    return *best;
}

Canvas::Canvas(int width_px, int height_px)
    : m_width(width_px), m_height(height_px), m_pixels(static_cast<std::size_t>(width_px) * height_px, 0)
{
}

void Canvas::fillRect(Rect rect, Pixel color)
{
    int x0 = std::max(0, rect.x);
    int y0 = std::max(0, rect.y);
    int x1 = std::min(m_width, rect.x + rect.w);
    int y1 = std::min(m_height, rect.y + rect.h);

    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            m_pixels[static_cast<std::size_t>(y) * m_width + x] = color;
}

void Canvas::blit(const Canvas& src, Point at)
{
    int x0 = std::max(0, at.x);
    int y0 = std::max(0, at.y);
    int x1 = std::min(m_width, at.x + src.m_width);
    int y1 = std::min(m_height, at.y + src.m_height);

    for (int y = y0; y < y1; ++y)
    {
        int srcY = y - at.y;
        Pixel* dstRow = &m_pixels[static_cast<std::size_t>(y) * m_width + x0];
        const Pixel* srcRow = &src.m_pixels[static_cast<std::size_t>(srcY) * src.m_width + (x0 - at.x)];
        std::copy(srcRow, srcRow + (x1 - x0), dstRow);
    }
}

void Canvas::blitScaled(const Canvas& src, Rect destRect, bool smooth)
{
    if (destRect.w <= 0 || destRect.h <= 0 || src.m_width <= 0 || src.m_height <= 0)
        return;

    int x0 = std::max(0, destRect.x);
    int y0 = std::max(0, destRect.y);
    int x1 = std::min(m_width, destRect.x + destRect.w);
    int y1 = std::min(m_height, destRect.y + destRect.h);

    if (!smooth)
    {
        // Nearest-neighbor: an index-and-copy loop, no per-pixel
        // floating-point interpolation -- see this method's header
        // comment for why the live-resize-tick caller needs this instead
        // of the smooth path below.
        for (int y = y0; y < y1; ++y)
        {
            int srcY = std::clamp((y - destRect.y) * src.m_height / destRect.h, 0, src.m_height - 1);
            const Pixel* srcRow = &src.m_pixels[static_cast<std::size_t>(srcY) * src.m_width];
            Pixel* dstRow = &m_pixels[static_cast<std::size_t>(y) * m_width];
            for (int x = x0; x < x1; ++x)
            {
                int srcX = std::clamp((x - destRect.x) * src.m_width / destRect.w, 0, src.m_width - 1);
                dstRow[x] = srcRow[srcX];
            }
        }
        return;
    }

    auto channel = [](Pixel p, int shift) { return static_cast<double>((p >> shift) & 0xFF); };

    for (int y = y0; y < y1; ++y)
    {
        // +0.5/-0.5: samples at each destination pixel's center mapped
        // back into source space, not its top-left corner -- the usual
        // resize-filter convention, otherwise the whole image drifts half
        // a source pixel toward the top-left.
        double srcYf = ((y - destRect.y) + 0.5) * src.m_height / destRect.h - 0.5;
        int sy0 = std::clamp(static_cast<int>(std::floor(srcYf)), 0, src.m_height - 1);
        int sy1 = std::clamp(sy0 + 1, 0, src.m_height - 1);
        double fy = std::clamp(srcYf - std::floor(srcYf), 0.0, 1.0);

        for (int x = x0; x < x1; ++x)
        {
            double srcXf = ((x - destRect.x) + 0.5) * src.m_width / destRect.w - 0.5;
            int sx0 = std::clamp(static_cast<int>(std::floor(srcXf)), 0, src.m_width - 1);
            int sx1 = std::clamp(sx0 + 1, 0, src.m_width - 1);
            double fx = std::clamp(srcXf - std::floor(srcXf), 0.0, 1.0);

            Pixel p00 = src.m_pixels[static_cast<std::size_t>(sy0) * src.m_width + sx0];
            Pixel p10 = src.m_pixels[static_cast<std::size_t>(sy0) * src.m_width + sx1];
            Pixel p01 = src.m_pixels[static_cast<std::size_t>(sy1) * src.m_width + sx0];
            Pixel p11 = src.m_pixels[static_cast<std::size_t>(sy1) * src.m_width + sx1];

            auto lerp = [&](int shift)
            {
                double top = channel(p00, shift) + (channel(p10, shift) - channel(p00, shift)) * fx;
                double bot = channel(p01, shift) + (channel(p11, shift) - channel(p01, shift)) * fx;
                return top + (bot - top) * fy;
            };

            auto r = static_cast<Pixel>(std::lround(lerp(16)));
            auto g = static_cast<Pixel>(std::lround(lerp(8)));
            auto b = static_cast<Pixel>(std::lround(lerp(0)));
            m_pixels[static_cast<std::size_t>(y) * m_width + x] = (r << 16) | (g << 8) | b;
        }
    }
}

void Canvas::blendRect(Rect rect, Pixel color, int alpha)
{
    if (alpha <= 0)
        return;
    if (alpha >= 255)
    {
        fillRect(rect, color);
        return;
    }

    int x0 = std::max(0, rect.x);
    int y0 = std::max(0, rect.y);
    int x1 = std::min(m_width, rect.x + rect.w);
    int y1 = std::min(m_height, rect.y + rect.h);

    int fr = (color >> 16) & 0xFF;
    int fg = (color >> 8) & 0xFF;
    int fb = color & 0xFF;

    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            Pixel& dst = m_pixels[static_cast<std::size_t>(y) * m_width + x];
            int br = (dst >> 16) & 0xFF;
            int bg = (dst >> 8) & 0xFF;
            int bb = dst & 0xFF;
            int r = (fr * alpha + br * (255 - alpha)) / 255;
            int g = (fg * alpha + bg * (255 - alpha)) / 255;
            int b = (fb * alpha + bb * (255 - alpha)) / 255;
            dst = (static_cast<Pixel>(r) << 16) | (static_cast<Pixel>(g) << 8) | static_cast<Pixel>(b);
        }
    }
}

void Canvas::fillTriangle(Point p0, Point p1, Point p2, Pixel color)
{
    long long area = edge(p0, p1, p2);
    if (area == 0)
        return; // degenerate (zero-width/zero-height)

    int minX = std::clamp(std::min({p0.x, p1.x, p2.x}), 0, m_width - 1);
    int maxX = std::clamp(std::max({p0.x, p1.x, p2.x}), 0, m_width - 1);
    int minY = std::clamp(std::min({p0.y, p1.y, p2.y}), 0, m_height - 1);
    int maxY = std::clamp(std::max({p0.y, p1.y, p2.y}), 0, m_height - 1);

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            Point p{x, y};
            long long w0 = edge(p1, p2, p);
            long long w1 = edge(p2, p0, p);
            long long w2 = edge(p0, p1, p);
            bool inside = (w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (inside)
                m_pixels[static_cast<std::size_t>(y) * m_width + x] = color;
        }
    }
}

void Canvas::line(Point p0, Point p1, Pixel color, int thickness)
{
    double dx = p1.x - p0.x;
    double dy = p1.y - p0.y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6)
        return; // zero-length: nothing to draw

    // Unit normal, scaled to half the requested thickness, gives the four
    // corners of a rectangle running along the segment.
    double nx = -dy / len * thickness / 2.0;
    double ny = dx / len * thickness / 2.0;

    Point a{static_cast<int>(std::lround(p0.x + nx)), static_cast<int>(std::lround(p0.y + ny))};
    Point b{static_cast<int>(std::lround(p0.x - nx)), static_cast<int>(std::lround(p0.y - ny))};
    Point c{static_cast<int>(std::lround(p1.x - nx)), static_cast<int>(std::lround(p1.y - ny))};
    Point d{static_cast<int>(std::lround(p1.x + nx)), static_cast<int>(std::lround(p1.y + ny))};

    fillTriangle(a, b, c, color);
    fillTriangle(a, c, d, color);
}

void Canvas::blitGlyph(const HackAtlas::Glyph& glyph, Point pos, Pixel color, const HackAtlas::Atlas& atlas)
{
    // One coverage byte per source pixel (0 = no ink, 255 = full ink --
    // see tools/offline/bakeFont), used directly as blendRect's alpha.
    // Always exact, 1:1, no scale/interpolation: a bigger glyph now means
    // the caller picked a bigger baked atlas (see canvas.h's class
    // comment and cursor.cpp's drawCard), not a stretch of this one.
    // Nearest-neighbor block-replication and bilinear resampling of the
    // coverage grid were both tried, for Title rows reusing the (smaller)
    // Body atlas at 2x -- neither looked right, since replicating or
    // interpolating already anti-aliased coverage data can't add
    // resolution the small source atlas never had; it just reads as
    // softer than a glyph actually baked at that size (confirmed by
    // direct comparison -- see PLAN.md).
    for (int y = 0; y < atlas.cellHeight; ++y)
    {
        for (int x = 0; x < atlas.cellWidth; ++x)
        {
            std::uint8_t coverage = glyph.bits[static_cast<std::size_t>(y) * atlas.bytesPerRow + x];
            if (coverage == 0)
                continue;
            blendRect({pos.x + x, pos.y + y, 1, 1}, color, coverage);
        }
    }
}

void Canvas::drawChar(char32_t codepoint, Point pos, Pixel color, const HackAtlas::Atlas& atlas)
{
    if (const HackAtlas::Glyph* glyph = findGlyph(atlas, codepoint))
        blitGlyph(*glyph, pos, color, atlas);
}

void Canvas::drawText(std::span<const char32_t> text, Point pos, Pixel color, const HackAtlas::Atlas& atlas)
{
    for (std::size_t i = 0; i < text.size(); ++i)
        drawChar(text[i], {pos.x + static_cast<int>(i) * atlas.cellWidth, pos.y}, color, atlas);
}
