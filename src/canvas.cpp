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

Canvas::Canvas(int width_px, int height_px, const HackAtlas::Atlas& atlas)
    : m_width(width_px), m_height(height_px), m_atlas(&atlas),
      m_pixels(static_cast<std::size_t>(width_px) * height_px, 0)
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

void Canvas::blitGlyph(const HackAtlas::Glyph& glyph, Point pos, Pixel color, int scale)
{
    for (int y = 0; y < m_atlas->cellHeight; ++y)
    {
        for (int x = 0; x < m_atlas->cellWidth; ++x)
        {
            // One coverage byte per source pixel (0 = no ink, 255 = full
            // ink -- see tools/offline/bakeFont). Used directly as
            // blendRect's alpha: every scale x scale destination block
            // gets the same blend weight as its one source pixel, which
            // is nearest-neighbor upscaling of the coverage value, same
            // as the old hard ink/no-ink blit did for the binary case.
            std::uint8_t coverage = glyph.bits[static_cast<std::size_t>(y) * m_atlas->bytesPerRow + x];
            if (coverage == 0)
                continue;
            blendRect({pos.x + x * scale, pos.y + y * scale, scale, scale}, color, coverage);
        }
    }
}

void Canvas::drawChar(char32_t codepoint, Point pos, Pixel color, int scale)
{
    if (const HackAtlas::Glyph* glyph = findGlyph(*m_atlas, codepoint))
        blitGlyph(*glyph, pos, color, scale);
}

void Canvas::drawText(std::span<const char32_t> text, Point pos, Pixel color, int scale)
{
    int advance = m_atlas->cellWidth * scale;
    for (std::size_t i = 0; i < text.size(); ++i)
        drawChar(text[i], {pos.x + static_cast<int>(i) * advance, pos.y}, color, scale);
}
