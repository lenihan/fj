#include "canvas.h"
#include "hackAtlas.h"

#include <algorithm>
#include <cmath>

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

const HackAtlas::Glyph* findGlyph(char32_t codepoint)
{
    for (std::size_t i = 0; i < HackAtlas::kGlyphCount; ++i)
        if (HackAtlas::kGlyphs[i].codepoint == codepoint)
            return &HackAtlas::kGlyphs[i];
    return nullptr;
}

} // namespace

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
    for (int y = 0; y < HackAtlas::kCellHeight; ++y)
    {
        for (int x = 0; x < HackAtlas::kCellWidth; ++x)
        {
            std::uint8_t byte = glyph.bits[static_cast<std::size_t>(y) * HackAtlas::kBytesPerRow + x / 8];
            bool ink = (byte >> (7 - (x % 8))) & 1;
            if (!ink)
                continue;
            fillRect({pos.x + x * scale, pos.y + y * scale, scale, scale}, color);
        }
    }
}

void Canvas::drawChar(char32_t codepoint, Point pos, Pixel color, int scale)
{
    if (const HackAtlas::Glyph* glyph = findGlyph(codepoint))
        blitGlyph(*glyph, pos, color, scale);
}

void Canvas::drawText(std::span<const char32_t> text, Point pos, Pixel color, int scale)
{
    int advance = HackAtlas::kCellWidth * scale;
    for (std::size_t i = 0; i < text.size(); ++i)
        drawChar(text[i], {pos.x + static_cast<int>(i) * advance, pos.y}, color, scale);
}
