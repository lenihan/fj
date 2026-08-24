// bakeFont: offline, Windows-only tool that rasterizes Hack-Regular.ttf into
// a ladder of grayscale-coverage glyph atlases (one per target DPI) and
// emits them as a single compiled-in C++ array of atlases.
//
// This is a one-time dev-machine tool (lives under tools/offline/, not
// tools/, to make that explicit), not something fj ships or links at
// runtime -- its only output is resources/hackAtlas.h/.cpp, which is what
// the core actually embeds and, once the bake looks right, is checked in
// and not expected to change again. That's why it's fine for this tool to
// lean on GDI directly instead of being cross-platform or dependency-free
// itself.
//
// Why a ladder, not one size: fj picks whichever baked atlas is closest to
// the window's current on-screen cell size (see src/canvas.h's pickAtlas)
// so live window resizing can re-render crisply instead of blowing up one
// small bitmap. Each DPI below independently derives its own target cell
// width (same formula the single-size version always used) and is baked
// as its own atlas; sizes that land on the same actual pixel dimensions
// (adjacent DPIs can round the same way) are deduplicated.
//
// Usage (run from repo root):
//   bakeFont.exe [fontPath] [outDir] [--dpis=96,120,144,168,192,216,240,288,336]
//
// Defaults: fontPath = resources/fonts/Hack-Regular.ttf
//           outDir   = resources (hackAtlas.h/.cpp are a generated asset,
//                      alongside the .ttf they're generated from -- not
//                      hand-maintained source, so they don't live in src/)
//           dpis     = Windows' standard scale-factor steps (100%-350%,
//                      96 dpi = 100%). Pass --dpis to bake a different set.

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

// Mirrors Card::kUseabledWidth_scen / Body::kColsPerRow in src/common.h.
// Body rows fill the usable card width with 60 columns; that's the cell
// width each atlas is baked at. Title rows use the same usable width with
// 30 columns, i.e. exactly 2x this cell width -- the core reuses whichever
// atlas is active for Title rows via integer 2x nearest-neighbor scaling.
constexpr double kUsableWidth_in = 5.0 - 2 * 0.1;
constexpr int kBodyColsPerRow = 60;

// 100%, 125%, 150%, 175%, 200%, 225%, 250%, 300%, 350% of the Windows
// baseline 96 dpi -- covers the standard Windows display-scaling steps.
constexpr double kDefaultDpis[] = {96, 120, 144, 168, 192, 216, 240, 288, 336};

// Printable ASCII, plus the two link-arrow glyphs CardItem::linkStr() uses
// (U+2191 upwards arrow "^", U+2192 rightwards arrow "->").
std::vector<char32_t> glyphCodepoints()
{
    std::vector<char32_t> cps;
    for (char32_t cp = 0x20; cp <= 0x7E; ++cp)
        cps.push_back(cp);
    cps.push_back(0x2191); // up arrow
    cps.push_back(0x2192); // right arrow
    return cps;
}

struct BakedFont
{
    int cellWidth_px{0};
    int cellHeight_px{0};
    int bytesPerRow{0};
    std::vector<char32_t> codepoints;
    std::vector<std::vector<uint8_t>> bits; // one entry per glyph
};

[[noreturn]] void fail(const std::string& msg)
{
    std::fprintf(stderr, "bakeFont: %s\n", msg.c_str());
    std::exit(1);
}

// Selects Hack into hdc at a given pixel height and returns its measured
// (fixed-pitch) advance width, or nullopt if GDI silently fell back to a
// different font (the classic gotcha when lfFaceName doesn't match).
std::optional<int> measureWidthAtHeight(HDC hdc, int height_px)
{
    LOGFONTW lf{};
    lf.lfHeight = -height_px; // negative = character height in pixels, not cell height
    lf.lfWeight = FW_REGULAR;
    lf.lfCharSet = ANSI_CHARSET;
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    wcscpy_s(lf.lfFaceName, L"Hack");

    HFONT font = CreateFontIndirectW(&lf);
    if (!font)
        return std::nullopt;

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

    wchar_t actualFace[LF_FACESIZE]{};
    GetTextFaceW(hdc, LF_FACESIZE, actualFace);
    if (std::wstring(actualFace) != L"Hack")
    {
        // GDI couldn't find "Hack" and silently substituted another font.
        SelectObject(hdc, oldFont);
        DeleteObject(font);
        return std::nullopt;
    }

    TEXTMETRICW tm{};
    GetTextMetricsW(hdc, &tm);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
    return tm.tmAveCharWidth;
}

// Binary search isn't safe here (width vs. height isn't guaranteed strictly
// monotonic at small sizes due to hinting/rounding), so scan linearly and
// keep whichever height's width is closest to target.
int findBestHeightForWidth(HDC hdc, int targetWidth_px)
{
    int bestHeight = -1;
    int bestDelta = INT_MAX;
    for (int h = 4; h <= 96; ++h)
    {
        auto w = measureWidthAtHeight(hdc, h);
        if (!w)
            continue;
        int delta = std::abs(*w - targetWidth_px);
        if (delta < bestDelta)
        {
            bestDelta = delta;
            bestHeight = h;
        }
    }
    if (bestHeight < 0)
        fail("could not select \"Hack\" via GDI at any pixel height -- "
             "was the font file loaded successfully?");
    return bestHeight;
}

BakedFont bakeFont(HDC hdc, double dpi)
{
    const int targetWidth_px = static_cast<int>(std::lround(kUsableWidth_in / kBodyColsPerRow * dpi));
    const int height_px = findBestHeightForWidth(hdc, targetWidth_px);

    LOGFONTW lf{};
    lf.lfHeight = -height_px;
    lf.lfWeight = FW_REGULAR;
    lf.lfCharSet = ANSI_CHARSET;
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    lf.lfQuality = ANTIALIASED_QUALITY; // grayscale AA, not CLEARTYPE_QUALITY -- ClearType's
                                        // RGB subpixel fringing assumes a fixed 1:1 physical
                                        // pixel layout, which doesn't survive being reduced to
                                        // a single coverage byte or later stretched (see
                                        // Canvas::blitGlyph and win32Window.cpp's live resize).
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    wcscpy_s(lf.lfFaceName, L"Hack");
    HFONT font = CreateFontIndirectW(&lf);
    SelectObject(hdc, font);

    TEXTMETRICW tm{};
    GetTextMetricsW(hdc, &tm);
    SetTextAlign(hdc, TA_TOP | TA_LEFT);
    SetBkColor(hdc, RGB(255, 255, 255));
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, OPAQUE);

    BakedFont result;
    result.cellWidth_px = tm.tmAveCharWidth;
    result.cellHeight_px = tm.tmAscent + tm.tmDescent;
    result.bytesPerRow = result.cellWidth_px; // one coverage byte per pixel, no bit-packing
    result.codepoints = glyphCodepoints();

    // One reusable top-down 32bpp DIB section, cleared and redrawn per glyph.
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = result.cellWidth_px;
    bmi.bmiHeader.biHeight = -result.cellHeight_px; // negative = top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dibPixels = nullptr;
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &dibPixels, nullptr, 0);
    if (!dib)
        fail("CreateDIBSection failed");
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(hdc, dib));
    SelectObject(hdc, font); // re-select after SelectObject(dib) reset it on some drivers

    const auto* pixels = static_cast<const uint32_t*>(dibPixels);
    RECT cellRect{0, 0, result.cellWidth_px, result.cellHeight_px};

    for (char32_t cp : result.codepoints)
    {
        wchar_t ch = static_cast<wchar_t>(cp);
        ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &cellRect, &ch, 1, nullptr);
        GdiFlush();

        // GDI's grayscale AA renders black text on a white background with
        // R==G==B at every pixel, the level directly reflecting how little
        // ink covers that pixel (255 = pure background, 0 = pure ink) --
        // invert it once here so the baked byte is coverage (0 = no ink,
        // 255 = full ink), which is exactly the alpha Canvas::blitGlyph
        // blends with.
        std::vector<uint8_t> glyphCoverage(static_cast<size_t>(result.bytesPerRow) * result.cellHeight_px, 0);
        for (int y = 0; y < result.cellHeight_px; ++y)
        {
            for (int x = 0; x < result.cellWidth_px; ++x)
            {
                uint32_t bgr = pixels[static_cast<size_t>(y) * result.cellWidth_px + x];
                uint8_t r = static_cast<uint8_t>(bgr >> 16);
                glyphCoverage[static_cast<size_t>(y) * result.bytesPerRow + x] = static_cast<uint8_t>(255 - r);
            }
        }
        result.bits.push_back(std::move(glyphCoverage));
    }

    SelectObject(hdc, oldBitmap);
    DeleteObject(dib);
    DeleteObject(font);

    return result;
}

// Bakes one atlas per requested dpi, deduplicating any that land on the
// same actual (cellWidth,cellHeight) -- adjacent dpis can round to the
// same GDI size -- and returns them sorted ascending by cellWidth.
std::vector<BakedFont> bakeLadder(const fs::path& fontPath, const std::vector<double>& dpis)
{
    if (AddFontResourceExW(fontPath.wstring().c_str(), FR_PRIVATE, nullptr) == 0)
        fail("AddFontResourceExW failed for " + fontPath.string());

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc)
        fail("CreateCompatibleDC failed");

    std::vector<BakedFont> atlases;
    for (double dpi : dpis)
    {
        BakedFont baked = bakeFont(hdc, dpi);
        bool duplicate = std::any_of(atlases.begin(), atlases.end(),
                                      [&](const BakedFont& existing)
                                      {
                                          return existing.cellWidth_px == baked.cellWidth_px &&
                                                 existing.cellHeight_px == baked.cellHeight_px;
                                      });
        if (duplicate)
        {
            std::printf("  dpi=%.0f -> %dx%d px/glyph (duplicate, skipped)\n", dpi, baked.cellWidth_px,
                        baked.cellHeight_px);
            continue;
        }
        std::printf("  dpi=%.0f -> %dx%d px/glyph\n", dpi, baked.cellWidth_px, baked.cellHeight_px);
        atlases.push_back(std::move(baked));
    }

    DeleteDC(hdc);
    RemoveFontResourceExW(fontPath.wstring().c_str(), FR_PRIVATE, nullptr);

    std::sort(atlases.begin(), atlases.end(),
              [](const BakedFont& a, const BakedFont& b) { return a.cellWidth_px < b.cellWidth_px; });
    return atlases;
}

void writeHeader(const std::vector<BakedFont>& atlases, const fs::path& outDir)
{
    std::ofstream out(outDir / "hackAtlas.h");
    out << "// Generated by tools/offline/bakeFont -- do not hand-edit.\n"
           "#pragma once\n\n"
           "#include <cstddef>\n"
           "#include <cstdint>\n\n"
           "namespace HackAtlas\n{\n\n"
           "struct Glyph\n{\n"
           "    char32_t codepoint;\n"
           "    const uint8_t* bits; // Atlas::bytesPerGlyph bytes, row-major, one byte per\n"
           "                         // pixel: 0 = no ink, 255 = full ink (grayscale coverage,\n"
           "                         // i.e. an alpha value -- see Canvas::blitGlyph)\n"
           "};\n\n"
           "struct Atlas\n{\n"
           "    int cellWidth;\n"
           "    int cellHeight;\n"
           "    int bytesPerRow;\n"
           "    int bytesPerGlyph;\n"
           "    std::size_t glyphCount;\n"
           "    const Glyph* glyphs;\n"
           "};\n\n"
        << "inline constexpr std::size_t kAtlasCount = " << atlases.size() << ";\n"
        << "extern const Atlas kAtlases[kAtlasCount]; // sorted ascending by cellWidth\n\n"
           "} // namespace HackAtlas\n";
}

void writeSource(const std::vector<BakedFont>& atlases, const fs::path& outDir)
{
    std::ofstream out(outDir / "hackAtlas.cpp");
    out << "// Generated by tools/offline/bakeFont -- do not hand-edit.\n"
           "#include \"hackAtlas.h\"\n\n"
           "namespace HackAtlas\n{\n\n";

    for (size_t a = 0; a < atlases.size(); ++a)
    {
        const BakedFont& font = atlases[a];
        for (size_t i = 0; i < font.codepoints.size(); ++i)
        {
            out << "static const uint8_t kAtlas" << a << "GlyphBits" << i << "[" << font.bits[i].size()
                << "] = {";
            const auto& bits = font.bits[i];
            for (size_t b = 0; b < bits.size(); ++b)
            {
                if (b % 16 == 0)
                    out << "\n    ";
                char buf[8];
                std::snprintf(buf, sizeof(buf), "0x%02X,", bits[b]);
                out << buf;
            }
            out << "\n};\n\n";
        }

        out << "static const Glyph kAtlas" << a << "Glyphs[" << font.codepoints.size() << "] = {\n";
        for (size_t i = 0; i < font.codepoints.size(); ++i)
        {
            out << "    { 0x" << std::hex << static_cast<uint32_t>(font.codepoints[i]) << std::dec
                << ", kAtlas" << a << "GlyphBits" << i << " },\n";
        }
        out << "};\n\n";
    }

    out << "const Atlas kAtlases[kAtlasCount] = {\n";
    for (size_t a = 0; a < atlases.size(); ++a)
    {
        const BakedFont& font = atlases[a];
        out << "    { " << font.cellWidth_px << ", " << font.cellHeight_px << ", " << font.bytesPerRow << ", "
            << (font.bytesPerRow * font.cellHeight_px) << ", " << font.codepoints.size() << ", kAtlas" << a
            << "Glyphs },\n";
    }
    out << "};\n\n} // namespace HackAtlas\n";
}

// Tiles every glyph of one atlas into a grid and dumps it as a 24bpp BMP so
// the bake can be eyeballed without any core/renderer code existing yet.
// One file per atlas, named by its cell size.
void writePreviewBmp(const BakedFont& font, const fs::path& outDir)
{
    constexpr int kCols = 16;
    const int rows = static_cast<int>((font.codepoints.size() + kCols - 1) / kCols);
    const int gap = 1;
    const int sheetW = kCols * (font.cellWidth_px + gap) + gap;
    const int sheetH = rows * (font.cellHeight_px + gap) + gap;
    const int rowStride = (sheetW * 3 + 3) & ~3; // BMP rows are 4-byte aligned

    std::vector<uint8_t> pixels(static_cast<size_t>(rowStride) * sheetH, 0xE0); // light gray gaps

    for (size_t g = 0; g < font.bits.size(); ++g)
    {
        int cellX = static_cast<int>(g % kCols) * (font.cellWidth_px + gap) + gap;
        int cellY = static_cast<int>(g / kCols) * (font.cellHeight_px + gap) + gap;
        const auto& bits = font.bits[g];
        for (int y = 0; y < font.cellHeight_px; ++y)
        {
            for (int x = 0; x < font.cellWidth_px; ++x)
            {
                uint8_t coverage = bits[static_cast<size_t>(y) * font.bytesPerRow + x];
                uint8_t v = static_cast<uint8_t>(255 - coverage); // coverage 0 (no ink) -> v 255 (white)
                // BMP is bottom-up; flip y into the buffer.
                int by = sheetH - 1 - (cellY + y);
                size_t idx = static_cast<size_t>(by) * rowStride + (cellX + x) * 3;
                pixels[idx + 0] = v;
                pixels[idx + 1] = v;
                pixels[idx + 2] = v;
            }
        }
    }

    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    fh.bfType = 0x4D42; // 'BM'
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + static_cast<DWORD>(pixels.size());
    ih.biSize = sizeof(ih);
    ih.biWidth = sheetW;
    ih.biHeight = sheetH;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = static_cast<DWORD>(pixels.size());

    char name[64];
    std::snprintf(name, sizeof(name), "hackAtlasPreview_%dx%d.bmp", font.cellWidth_px, font.cellHeight_px);
    std::ofstream out(outDir / name, std::ios::binary);
    out.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
    out.write(reinterpret_cast<const char*>(&ih), sizeof(ih));
    out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
}

std::vector<double> parseDpis(const std::string& csv)
{
    std::vector<double> dpis;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ','))
        dpis.push_back(std::stod(item));
    return dpis;
}

} // namespace

int main(int argc, char** argv)
{
    fs::path fontPath = "resources/fonts/Hack-Regular.ttf";
    fs::path outDir = "resources";
    std::vector<double> dpis(std::begin(kDefaultDpis), std::end(kDefaultDpis));

    bool sawFontPath = false;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.rfind("--dpis=", 0) == 0)
            dpis = parseDpis(arg.substr(7));
        else if (!sawFontPath)
        {
            fontPath = arg;
            sawFontPath = true;
        }
        else
            outDir = arg;
    }

    if (!fs::exists(fontPath))
        fail("font file not found: " + fontPath.string() +
             " (run from the repo root, or pass the path explicitly)");

    std::printf("Baking %zu atlas(es)...\n", dpis.size());
    std::vector<BakedFont> atlases = bakeLadder(fontPath, dpis);

    writeHeader(atlases, outDir);
    writeSource(atlases, outDir);
    for (const BakedFont& font : atlases)
        writePreviewBmp(font, outDir);

    std::printf("Wrote %s\\hackAtlas.h, hackAtlas.cpp, %zu preview BMP(s) (%zu atlases, %zu glyphs each)\n",
                outDir.string().c_str(), atlases.size(), atlases.size(),
                atlases.empty() ? size_t{0} : atlases.front().codepoints.size());
    return 0;
}
