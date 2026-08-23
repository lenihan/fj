## ARCHITECTURE (pivot away from Qt)

fj is moving off Qt to a fully self-contained, zero-third-party-dependency
implementation. Goal: only OS-native APIs, so the build "just works" on
each platform without depending on the status of any external library —
and stays small/efficient enough to run on low-end hardware.

### Dependency policy

- Windows: Win32 only (window/message loop; GDI or Direct2D for pixel
  presentation)
- Linux: Xlib only (window/events; XShm for fast blitting) — this is
  treated as "native," not third-party, since it ships with the OS
- Web: browser-native Canvas API (`putImageData`), reached via
  Emscripten — Emscripten is a compiler/toolchain, not a linked
  runtime dependency
- No Freetype/fontconfig/GDI-text/DirectWrite/etc. Text is rendered by
  fj's own bitmap font + rasterizer (see below), so the one place native
  text APIs diverge most across platforms (Linux has no OS-bundled
  equivalent to DirectWrite) is sidestepped entirely.

### Core / platform-shell split

- **Core** (fully portable, no platform code, the majority of the
  codebase): owns a raw pixel buffer, fj's own bitmap font + glyph
  rasterizer, drawing primitives (rect/line/text), and all fj logic —
  card/stack/TOC model, cursor, navigation state machine. Testable in
  isolation with no window at all.
- **Platform shell** (one small file per platform — Win32, Xlib,
  Emscripten/JS): does exactly three things — open a window, forward
  keyboard events into the core, blit the core's finished pixel buffer
  to the screen. Target: a few hundred lines each.

Build order: **Win32 first**, then Linux (Xlib), then Web.

### Core/shell contract and rasterizer (implemented)

The draft sketch that used to live in this section is superseded by the
real headers — see `src/platform.h` (core/shell contract) and
`src/canvas.h` (drawing primitives). Summary of what got decided:

- `PlatformWindow` is a plain, non-virtual, non-template class (pimpl'd,
  not a virtual base) — only one platform's implementation is ever
  compiled into a given binary (selected by CMake), so neither dynamic
  nor static polymorphism was buying anything, just build time or
  runtime indirection for nothing.
- Window/context creation returns `std::expected<PlatformWindow,
  std::string>` (C++23) rather than a nullable pointer, so a failure
  carries a reason.
- `Canvas` (core-owned pixel buffer + `fillRect`/`fillTriangle`/`line`/
  `drawText`) is a minimal set derived from what `cursor.cpp`/
  `cardItem.cpp` actually draw today, not a general 2D API. `line` is
  built from two filled triangles; no anti-aliasing, rounded corners, or
  caps yet.

### Coordinate system (core)

The Qt app used four coordinate systems (`_scen`/`_font`/`_view`/`_locl`,
see `PLAN.md`) because it juggled inches, font-metric units, view pixels,
and item-local space all at once. The core collapses that to two units,
still marked with a suffix since both are genuinely in play:

- `_px` — plain pixels, origin top-left, +x right, +y down (matches
  `Canvas`'s pixel buffer layout and a top-down Win32 DIB). What
  `CardItem`/`Cursor` actually compute row/col layout and draw calls in.
- `_in` — physical inches. The source of truth for card/row size
  (`Card::kWidth_in`, `kHeight_in`, ...) — e.g. a 3x5 card is genuinely
  meant to render as 3in x 5in on screen, not just "however big the
  fixed-pixel font happens to make it."

Reconciling a fixed-pixel atlas with a physical-inches target: at
window-creation time, query the display's DPI (same call `main.cpp`
already makes today) and pick whichever integer render scale `S` makes
`S * HackAtlas::kCellWidth` land closest to `(Card::kWidth_in / cols) *
dpi`. This is the same mechanism already used to reuse the Body atlas at
2x for Title rows, just computed from the real display instead of a
fixed ratio. It hits the physical size exactly when a display's DPI
makes the best-fit scale come out even, and gets close otherwise — true
per-pixel exactness at every DPI would need either continuous/fractional
scaling (still deferred -- that's what "non-blocky scaling" refers to)
or rebaking the atlas per machine (contradicts "never build hackAtlas
again").

### Superseded

The existing Qt6/CMake/`QGraphicsItem`-based implementation
(`src/cardItem`, `contentItem`, `tocItem`, `rowItem`, `cursor`,
`squareGraphicsView`, `capsLockModifier`) is being replaced. `README.md`
and `CMakeLists.txt` will need to be updated once the Win32 core/shell
skeleton exists (Qt6 dependency removed, new source list).
