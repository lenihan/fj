// bakeFont: offline, Windows-only tool that rasterizes Hack-Regular.ttf into
// a 1bpp bitmap glyph atlas and emits it as a compiled-in C++ array.
//
// This is a one-time dev-machine tool, not something fj ships or links at
// runtime -- its only output is hackAtlas.h/.cpp, which is what the core
// actually embeds. That's why it's fine for this tool to lean on GDI
// directly instead of being cross-platform or dependency-free itself.
//
// Usage (run from repo root):
//   bakeFont.exe [fontPath] [outDir] [--dpi=132]
//
// Defaults: fontPath = resources/fonts/Hack-Regular.ttf
//           outDir   = tools/bakeFont
//           dpi      = 132 (matches the Surface Pro 11 dev machine noted in
//                      src/main.cpp; pass --dpi to bake against a different
//                      reference display)

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

// Mirrors Card::kUseabledWidth_scen / Body::kColsPerRow in src/common.h.
// Body rows fill the usable card width with 60 columns; that's the cell
// width the atlas is baked at. Title rows use the same usable width with
// 30 columns, i.e. exactly 2x this cell width -- the core can reuse this
// same atlas for Title rows via integer 2x nearest-neighbor scaling.
constexpr double kUsableWidth_in = 5.0 - 2 * 0.1;
constexpr int kBodyColsPerRow = 60;

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
    lf.lfQuality = NONANTIALIASED_QUALITY; // hard black/white edges for clean 1bpp thresholding
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

BakedFont bakeFont(const fs::path& fontPath, double dpi)
{
    if (AddFontResourceExW(fontPath.wstring().c_str(), FR_PRIVATE, nullptr) == 0)
        fail("AddFontResourceExW failed for " + fontPath.string());

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc)
        fail("CreateCompatibleDC failed");

    const int targetWidth_px = static_cast<int>(std::lround(kUsableWidth_in / kBodyColsPerRow * dpi));
    const int height_px = findBestHeightForWidth(hdc, targetWidth_px);

    LOGFONTW lf{};
    lf.lfHeight = -height_px;
    lf.lfWeight = FW_REGULAR;
    lf.lfCharSet = ANSI_CHARSET;
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    lf.lfQuality = NONANTIALIASED_QUALITY;
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
    result.bytesPerRow = (result.cellWidth_px + 7) / 8;
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

        std::vector<uint8_t> glyphBits(static_cast<size_t>(result.bytesPerRow) * result.cellHeight_px, 0);
        for (int y = 0; y < result.cellHeight_px; ++y)
        {
            for (int x = 0; x < result.cellWidth_px; ++x)
            {
                uint32_t bgr = pixels[static_cast<size_t>(y) * result.cellWidth_px + x];
                uint8_t r = static_cast<uint8_t>(bgr >> 16);
                bool ink = r < 128;
                if (ink)
                    glyphBits[static_cast<size_t>(y) * result.bytesPerRow + (x / 8)] |=
                        static_cast<uint8_t>(0x80 >> (x % 8));
            }
        }
        result.bits.push_back(std::move(glyphBits));
    }

    SelectObject(hdc, oldBitmap);
    DeleteObject(dib);
    DeleteObject(font);
    DeleteDC(hdc);
    RemoveFontResourceExW(fontPath.wstring().c_str(), FR_PRIVATE, nullptr);

    return result;
}

void writeHeader(const BakedFont& font, const fs::path& outDir)
{
    std::ofstream out(outDir / "hackAtlas.h");
    out << "// Generated by tools/bakeFont -- do not hand-edit.\n"
           "#pragma once\n\n"
           "#include <cstddef>\n"
           "#include <cstdint>\n\n"
           "namespace HackAtlas\n{\n"
        << "inline constexpr int kCellWidth = " << font.cellWidth_px << ";\n"
        << "inline constexpr int kCellHeight = " << font.cellHeight_px << ";\n"
        << "inline constexpr int kBytesPerRow = " << font.bytesPerRow << ";\n"
        << "inline constexpr int kBytesPerGlyph = kBytesPerRow * kCellHeight;\n"
        << "inline constexpr std::size_t kGlyphCount = " << font.codepoints.size() << ";\n\n"
           "struct Glyph\n{\n"
           "    char32_t codepoint;\n"
           "    const uint8_t* bits; // kBytesPerGlyph bytes, row-major, MSB-first, 1 = ink\n"
           "};\n\n"
           "extern const Glyph kGlyphs[kGlyphCount];\n\n"
           "} // namespace HackAtlas\n";
}

void writeSource(const BakedFont& font, const fs::path& outDir)
{
    std::ofstream out(outDir / "hackAtlas.cpp");
    out << "// Generated by tools/bakeFont -- do not hand-edit.\n"
           "#include \"hackAtlas.h\"\n\n"
           "namespace HackAtlas\n{\n\n";

    for (size_t i = 0; i < font.codepoints.size(); ++i)
    {
        out << "static const uint8_t kGlyphBits" << i << "[kBytesPerGlyph] = {";
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

    out << "const Glyph kGlyphs[kGlyphCount] = {\n";
    for (size_t i = 0; i < font.codepoints.size(); ++i)
    {
        out << "    { 0x" << std::hex << static_cast<uint32_t>(font.codepoints[i]) << std::dec
            << ", kGlyphBits" << i << " },\n";
    }
    out << "};\n\n} // namespace HackAtlas\n";
}

// Tiles every glyph into a grid and dumps it as a 24bpp BMP so the bake can
// be eyeballed without any core/renderer code existing yet.
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
                bool ink = (bits[static_cast<size_t>(y) * font.bytesPerRow + (x / 8)] >> (7 - (x % 8))) & 1;
                uint8_t v = ink ? 0x00 : 0xFF;
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

    std::ofstream out(outDir / "hackAtlasPreview.bmp", std::ios::binary);
    out.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
    out.write(reinterpret_cast<const char*>(&ih), sizeof(ih));
    out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
}

} // namespace

int main(int argc, char** argv)
{
    fs::path fontPath = "resources/fonts/Hack-Regular.ttf";
    fs::path outDir = "tools/bakeFont";
    double dpi = 132.0;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.rfind("--dpi=", 0) == 0)
            dpi = std::stod(arg.substr(6));
        else if (fontPath == "resources/fonts/Hack-Regular.ttf" && i == 1)
            fontPath = arg;
        else
            outDir = arg;
    }

    if (!fs::exists(fontPath))
        fail("font file not found: " + fontPath.string() +
             " (run from the repo root, or pass the path explicitly)");

    BakedFont font = bakeFont(fontPath, dpi);
    std::printf("Baked Hack at %dx%d px/glyph (target width %.2fpx @ %.0f dpi), %zu glyphs\n",
                font.cellWidth_px, font.cellHeight_px,
                kUsableWidth_in / kBodyColsPerRow * dpi, dpi, font.codepoints.size());

    writeHeader(font, outDir);
    writeSource(font, outDir);
    writePreviewBmp(font, outDir);

    std::printf("Wrote %s\\hackAtlas.h, hackAtlas.cpp, hackAtlasPreview.bmp\n", outDir.string().c_str());
    return 0;
}
