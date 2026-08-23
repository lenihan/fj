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

### Starting contract (draft — not finalized)

```cpp
// platform.h — contract between core and each platform shell
#pragma once
#include <cstdint>
#include <span>
#include <functional>

enum class Key { Up, Down, Left, Right, Enter, Escape /* ... */ };

struct PlatformWindow {
    virtual ~PlatformWindow() = default;
    virtual void present(std::span<const uint32_t> pixels, int w, int h) = 0;
    virtual void run(std::function<void(Key)> onKeyDown) = 0; // blocks, owns the loop
};

std::unique_ptr<PlatformWindow> createPlatformWindow(int w, int h, const char* title);
```

### Open design questions (not yet decided)

- Virtual interface (`PlatformWindow` above) vs. a C++20
  concepts-based static-dispatch approach — only one backend is ever
  compiled in at a time, so the vtable indirection may be needless.
- Who owns the event loop — currently sketched as the platform shell's
  `run()` blocking and calling back into the core, which is fine as
  long as fj has no other async work competing for the thread.
- Error handling for window/context creation — candidate:
  `std::expected<std::unique_ptr<PlatformWindow>, std::string>` (C++23)
  instead of a nullable return.

### Superseded

The existing Qt6/CMake/`QGraphicsItem`-based implementation
(`src/cardItem`, `contentItem`, `tocItem`, `rowItem`, `cursor`,
`squareGraphicsView`, `capsLockModifier`) is being replaced. `README.md`
and `CMakeLists.txt` will need to be updated once the Win32 core/shell
skeleton exists (Qt6 dependency removed, new source list).
