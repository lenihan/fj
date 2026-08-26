# PLAN

## DESIGN

- Glossary
  - Card - A 3" x 5" index card, with typing on one side (colloquial
    name; renders landscape, 5in wide x 3in tall -- see `layout.h`'s
    `Card::kWidth_in`/`kHeight_in`)
    - Content card - Contains anything you want, has lines indicating rows
    - TOC card - Table of contents card that points to other TOC/content cards, no lines (blank)
  - Card stack - 1 or more cards
    - Master card stack - Read-only with help thread and list of year card stacks
    - Year card stack - New cards are placed the the card stack for current year (e.g. 2026)
  - Card number - The unique, sequential number of a card in a card stack
    - External card number - A card number from a different card stack in form YEAR-CARDNUM
  - Thread - 2 or more cards that are related
    - Do not have to be consecutive
    - Can cross card stacks
  - Row - A card is divided into 11 rows
    - Top: Title row - Names thread
    - Middle: 9 content rows
    - Bottom: Navigation row - Card number and prev/next card in thread
      - Left: previous card of thread
      - Middle: card number
      - Right: next card in thread
- Hierarchy
  - Master card stack
    - Master TOC
      - Help thread
      - Year card stacks (1 or more)
    - Help thread cards
  - Year card stack (1 or more)
    - Year TOC
      - Content/TOC (1 or more)
    - Content/TOC cards
- Concepts
  - New cards only for current year stack
  - If next card of thread is blank, then end of thread
  - →CARDNUM or →YEAR-CARDNUM - A link to another card
    - Used in navigation road for prev/next card in thread
    - Use to link one card to another card
  - ↑CARDNUM or ↑YEAR-CARDNUM - A link to parent TOC card
  - YEAR- prefix is dropped if YEAR is same as card stack's YEAR

## ARCHITECTURE

fj is a fully self-contained, zero-third-party-dependency implementation:
only OS-native APIs, so the build "just works" on each platform without
depending on the status of any external library, and stays small/efficient
enough to run on low-end hardware. It isn't really a Windows app that
happens to draw a card -- it's an emulator for a piece of keyboard-only
hardware that doesn't exist yet, and the window is that hardware's
monitor (see `main.cpp`'s file comment). The original Qt6/QGraphicsItem
implementation has been fully removed from this branch.

### Dependency policy

- Windows: Win32 only (window/message loop, GDI for pixel presentation)
  -- **implemented**, see `src/win32Window.cpp`
- Linux: Xlib only (window/events; XShm for fast blitting) -- this is
  treated as "native," not third-party, since it ships with the OS --
  **not started**
- Web: browser-native Canvas API (`putImageData`), reached via Emscripten
  -- Emscripten is a compiler/toolchain, not a linked runtime dependency
  -- **not started**
- No Freetype/fontconfig/GDI-text/DirectWrite/etc. Text is fj's own
  bitmap font + rasterizer, baked offline (see "Font atlas" below) -- the
  one place native text APIs diverge most across platforms (Linux has no
  OS-bundled equivalent to DirectWrite) is sidestepped entirely.

### Core / platform-shell split

- **Core** (`src/cardItem.*`, `cardStack.*`, `contentItem.*`, `tocItem.*`,
  `cursor.*`, `canvas.*`, `layout.h`, `types.h`, `textUtil.h`) -- fully
  portable, no platform code. Owns the pixel buffer (`Canvas`), the baked
  font atlas ladder, drawing primitives, and all fj logic (card/stack/TOC
  model, cursor, navigation state machine).
- **Platform shell** (`src/win32Window.cpp` on Windows; one small file per
  platform otherwise) -- implements `platform.h`'s contract: open a
  window, forward keyboard/resize events into the core, present the
  core's finished pixel buffer. Only one shell is ever compiled into a
  given binary (selected by CMake).
- The contract itself (`PlatformWindow`, `KeyEvent`, `createPlatformWindow`,
  `displayDpi`) is `src/platform.h` -- read it before touching either side
  of the boundary. Its comments, and `src/canvas.h`'s, cover the design
  decisions (why no vtable/template for `PlatformWindow`, why
  `std::expected` for window creation, what's and isn't anti-aliased,
  etc.) in more depth than belongs here.

Build order: **Win32 (done)**, then Linux (Xlib), then Web.

### Coordinate system

Two units, marked with a `_px`/`_in` suffix since both are genuinely in
play:

- `_px` -- plain pixels, origin top-left, +x right, +y down (matches
  `Canvas`'s pixel buffer layout and a top-down Win32 DIB). What
  `CardItem`/`Cursor` compute row/col layout and draw calls in.
- `_in` -- physical inches. The source of truth for card/monitor size
  (`Card::kWidth_in`/`kHeight_in`, `Monitor::kWidth_in`/`kHeight_in` in
  `layout.h`) -- genuinely meant to render at that physical size on
  screen (see "Physical accuracy" below), not just however big the
  fixed-pixel font happens to make it.

(The original Qt implementation used four coordinate systems --
`_scen`/`_font`/`_view`/`_locl` -- because it juggled inches, font-metric
units, view pixels, and item-local space all at once. That's gone along
with Qt.)

### Physical accuracy

fj's window is meant to be true to life: 1 inch on screen is meant to be
1 physical inch, and the monitor being emulated (`Monitor::kWidth_in`/
`kHeight_in`, 5"x5") should measure that with a ruler. Getting this right
took several real fixes, all in `src/win32Window.cpp`:

- `GetDeviceCaps(..., LOGPIXELSX)` returns the OS's display-scaling
  percentage, not the monitor's real pixel density -- fixed by computing
  DPI from `HORZSIZE`/`HORZRES` (the monitor's physical size vs its
  resolution) instead.
- `AdjustWindowRect`'s border-size prediction isn't DPI-parameterized and
  didn't reliably land the client area on the requested size --
  `createPlatformWindow` measures what `CreateWindowExW` actually
  produced and self-corrects via `SetWindowPos`.
- Plain "system DPI aware" mode can have the whole window silently
  bitmap-scaled by DWM if the monitor's real scaling differs from the
  system's -- fixed by declaring true per-monitor-v2 DPI awareness via a
  manifest (`CMakeLists.txt`'s `VS_DPI_AWARE`), not a runtime API call.
- Some displays (especially laptop/tablet built-in panels) report their
  own physical size to the OS imprecisely, which no DPI-awareness fix can
  correct -- `KeyEvent::Kind::Calibrate` (F5, or the window's system-menu
  "Fix Calibration..." item) is the escape hatch: drag-resize against a
  physical ruler, then calibrate, and the result persists to
  `%APPDATA%\fj\calibration.txt`.

### Font atlas

`tools/offline/bakeFont` is a one-time, dev-machine-only tool (not
shipped, never linked into `fj.exe`) that rasterizes
`resources/fonts/Hack-Regular.ttf` via GDI into a ladder of
grayscale-coverage glyph atlases -- one per standard Windows
display-scale step (100%-350%) -- and emits them as
`resources/hackAtlas.h/.cpp`, a compiled-in C++ array the core embeds.
`Canvas::pickAtlas` selects whichever baked atlas is closest to the
window's current size at runtime (see "Window resizing" below for
exactly which dimension "current size" means once the window stopped
being locked to a square), so live resizing re-renders close to native
resolution instead of blowing up one small bitmap.

Glyphs are anti-aliased (`Canvas::blendRect` blends each pixel by its
baked coverage byte, a graduated 0-255 alpha, not a hard 0/255 cutoff --
that's what makes a diagonal or curved stroke read as a smooth line
instead of a staircase). GDI's own AA asked for directly at the small
end of the ladder (8-13px cells) rendered visibly faint/washed-out (not
enough pixels for the AA gradient to represent a stroke's shape), so
`bakeFont` renders each glyph at 4x the target resolution (still with
GDI's AA, which looks good at that larger, more-detailed size) and
box-filter downsamples back to the target (see `downsampleCoverage`) --
crisper edges from the area-average than naive small-size AA gives, but
still a real graduated alpha, not thresholded away. (A fully
thresholded/hard-edged version was tried at one point in response to a
"still looks blurry" report that turned out to actually be the window/
atlas resample mismatches fixed separately below -- hard-thresholding
just traded that bug for visible staircasing on diagonals/curves at the
large end of the ladder instead, confirmed by direct comparison, and was
reverted.) Baked once, checked into `resources/hackAtlas.h/.cpp`, and
compiled into both platforms identically -- Linux and Windows render
from the exact same coverage bytes, not a per-platform rasterization.
Only re-run `bakeFont.exe` if the font, the DPI ladder, the glyph set,
or this rendering approach changes -- its output is checked in and not
expected to change otherwise.

Title rows render from a second, independently-picked atlas (closest to
2x the Body atlas's cell width) rather than upscaling Body's own glyph
bitmaps -- see `Cursor::draw`'s `titleAtlas` parameter and `Canvas`'s
class comment. Both nearest-neighbor block-replication and bilinear
resampling of Body's coverage grid were tried first; neither looked
right, because neither can add resolution a smaller source atlas never
had -- nearest-neighbor makes each already-soft edge pixel cover 4x the
area (reading as grayer/blurrier), and bilinear actively dilutes thin
strokes below full darkness when a stroke is only 1-2 source pixels
wide, confirmed by sampling actual output pixel values in both cases.
`Canvas` doesn't store an atlas at all as a result -- `drawChar`/
`drawText` take one explicitly per call, so one `Canvas` (one rendered
frame) can freely mix atlases per row. Layout (row heights, margins, the
2x cell-size relationship) still comes entirely from the Body atlas
(`CardItem::cellWidth_px` et al.) regardless of which exact atlas
`pickAtlas` lands on for the title -- a deliberate trade of pixel-exact
column alignment for real per-size anti-aliasing, since the two atlases'
cell widths are picked independently and won't always be in an exact
2:1 ratio.

### Window resizing

The window can be resized to any shape -- it isn't locked to a square
the way it once was. An earlier version enforced squareness at the
platform-shell level (win32Window.cpp's `WM_SIZING` clamp,
xlibWindow.cpp's anchor-aware `ConfigureNotify` correction, plus a
`PlatformWindow::resizeTo` main.cpp called on every resize tick to snap
the window to whichever baked atlas it had just picked); all of that
fought the OS's own live-resize tracking, on both platforms, and made
interactive dragging feel glitchy.

Instead, `main.cpp`'s `redraw` fits the square monitor into whatever
shape the window actually is: `Canvas::blitScaled` (a bilinear resample,
the one implementation of that math in the whole codebase now --
win32Window.cpp/xlibWindow.cpp's `present()` used to each carry their own
copy to bridge an analogous gap) scales the rendered square into the
largest centered square that fits, black filling whatever margin that
leaves on the wider axis (letterboxed/pillarboxed, like a video player,
rather than distorted). `createPlatformWindow` no longer takes an
aspect-ratio parameter, and `present()`'s contract changed to match: the
caller is expected to hand over pixels already sized to the window's
current dimensions, so a platform shell's own defensive stretch-to-live-
size (still there in case a resize lands mid-frame) is normally a 1:1
no-op rather than a second resample on top of `blitScaled`'s.

Atlas selection (both Body and, from it, Title -- see "Font atlas" above)
now picks from `min(window width, window height)`, the dimension that
actually constrains how big the square can render, not width alone.

`PlatformWindow::run` takes two resize callbacks, not one, for the same
reason main.cpp's atlas pick shouldn't happen on every tick: `onResize`
fires on every tick of a live resize and is expected to stay cheap
(`redraw` just re-stretches the already-rendered card into the new
size); `onResizeEnd` fires once, after the resize actually settles, and
is where the atlas gets re-picked and the card rebuilt at native
resolution. An earlier version did that work on every `onResize` tick,
which made a single big drag visibly step through several discrete
crisp re-renders instead of one smooth stretch ending in one clean
refresh. Win32 detects "settled" natively (`WM_ENTERSIZEMOVE`/
`WM_EXITSIZEMOVE`, with a non-interactive resize like a maximize firing
`onResizeEnd` straight from `WM_SIZE` instead, since it never gets an
ENTERSIZEMOVE/EXITSIZEMOVE pair); X11 has no equivalent protocol event,
so xlibWindow.cpp debounces instead -- `run()`'s event loop moved from a
plain blocking `XNextEvent` to `select()` on the display connection's fd
with a short timeout, treating "no new `ConfigureNotify` within the
timeout" as settled. `onResizeEnd` also skips its own work (atlas
re-pick, card rebuild) if it fires again for a size it already settled
on -- a platform shell guarantees it fires once per completed resize,
not that a WM can never send a late settling `ConfigureNotify` or two
afterward, which would otherwise cost a second, redundant, visibly
distinct refresh.

`main.cpp` splits "render the card's content" (`renderContent`) from
"put the current content on screen at the current size" (`presentFrame`)
for the same reason: a live-resize tick calls `presentFrame` alone, not
`redraw` (`renderContent` + `presentFrame`) -- the card's own pixels
haven't changed, only the window's shape, so re-running `Cursor::draw`
on every tick was pure waste. `presentFrame` also takes a `smooth` flag
straight from `Canvas::blitScaled`: bilinear (used once, in the settled
redraw, where the gap between the card's native resolution and the
window is already small) is a per-pixel floating-point loop, expensive
enough on a large, constantly-resizing window that a Debug build
couldn't keep up with a fast drag's tick rate -- ticks piled up faster
than they could be drawn, so visible updates kept draining for a
noticeable time after the mouse stopped moving. Every live-resize tick
uses nearest-neighbor instead (an index-and-copy loop, no interpolation)
-- cheap enough to actually keep up, at the cost of looking a bit
blockier for the fraction of a second the drag itself lasts.

That alone didn't fully fix the "can't keep up" symptom -- measuring
with real timing (not guessing) found the actual dominant cost one layer
further down: both platform shells' present()/blit() used to
independently re-query the window's own "live" size (Xlib's
`XGetWindowAttributes`, Win32's `GetClientRect`) and resample/stretch to
fill *that*, on the theory that the caller might hand over some other
size. Once `presentFrame` started always building its output canvas at
exactly the window's current size, that query stopped being a defensive
convenience and started being a race against a fast live resize instead
-- by the time it ran, the window had often already moved on to a
*newer* size than the one that produced these pixels, so the "does
w/h match?" check failed almost every tick and silently fell back to a
full resample over the whole window (measured at 60-90ms/tick on Xlib,
worse than the nearest-neighbor fix was supposed to cost at all).
Neither platform shell re-queries its own size anymore -- both trust
w/h exactly as given (Xlib: a straight `memcpy` per row into the XShm
image; Win32: `SetDIBitsToDevice` instead of `StretchDIBits`+HALFTONE,
since there's no gap left to stretch across) -- which also let Xlib drop
a second real cost hiding behind the first: `ensureImage` reallocates
its XShm segment on nearly every tick of a live resize (the size changes
almost every time), and the attach step used to re-verify XShm actually
works via a fresh `XSync` round trip on every single one of those,
though that support is a fact about the X *connection*, established
once, not something that needs re-proving per image. Cached after the
first real probe instead. Together this took Xlib from ~60-90ms/tick
down to ~10-20ms.

### Web (Emscripten) shell

`webWindow.cpp` implements the same `platform.h` contract as the other
two shells, but a browser tab breaks two of their assumptions outright
rather than just needing a different API for the same idea.

**`run()` can't literally block.** `GetMessageW`/`XNextEvent` can block
their thread forever because each owns a whole OS process; a browser tab
can't block its one JS thread that way without freezing the page. Two
ways to keep `run()`'s "never returns to the caller" contract true
anyway: Asyncify (compiles the whole reachable call graph so it can
save/unwind its native stack and yield back to the browser, keeping a
literal blocking call) or `emscripten_set_main_loop(fn, fps,
simulate_infinite_loop=true)` (the standard Emscripten/SDL2/Dear ImGui
pattern -- registers a per-frame callback and unwinds via a special
internal mechanism that does *not* run the caller's destructors).
Chose the latter: no Asyncify build-size/perf cost, and Qt for
WebAssembly's own default event dispatcher uses the same approach,
reaching for Asyncify only for the harder case (a nested blocking event
loop) fj doesn't have. The "doesn't run destructors" property is load-
bearing, not just a quirk to route around: `main.cpp`'s
`onKey`/`onResize`/`onResizeEnd` lambdas capture `Cursor`/`Canvas`/etc.
by reference from `main()`'s own stack frame, and those references stay
valid only because that frame is genuinely never popped. `mainLoopTick`
itself is empty -- every real event (keystrokes, resize) is delivered by
its own DOM callback the instant it happens, not polled here; the main
loop's only job is keeping `run()` from returning.

**Real text input needs a real text element, not a keydown listener.**
`keydown` only reports a physical key, not a composed character -- dead
keys, IME composition, and non-US layouts all need a genuinely editable
DOM element's `input` event to come out correct, the same job `WM_CHAR`
does on Win32 and `Xutf8LookupString` does on Xlib. `fjCreateDom` builds
an off-screen, permanently-focused `<input>` for exactly this; its
`input` event iterates real Unicode code points (`codePointAt`, not
UTF-16 code units) and forwards each through `Module.ccall` to
`fj_web_on_char`. `Enter`/`Backspace`/`CapsLock`/`F5` are intercepted at
`keydown` instead (returning `true` triggers the runtime's
`preventDefault()` for us) so they're never also seen as stray `input`
events.

**Caps Lock's OS-level lock state can't be suppressed on the web, at
all.** win32Window.cpp's `SetWindowsHookEx` dance and xlibWindow.cpp's
`XkbLockModifiers` both force the *system* Caps Lock off while focused,
so a letter typed during a held-Caps command only ever arrives as its
plain unshifted codepoint. No browser API can mutate OS keyboard-lock
state. Real consequence, not just theoretical: holding Caps Lock to
navigate (`i`/`j`/`k`/`l`) genuinely worked in the browser's own eyes,
but the letters arrived pre-uppercased by the real OS lock the physical
keypress engaged, which `Cursor`'s command dispatch didn't recognize --
found by testing, not inspection. Fixed locally in `webWindow.cpp`
(tracks the physical key's own held state from the `keydown`/`keyup`
edges it already forwards, and lowercases `A`-`Z` while held before
constructing the `Char` event) rather than in `Cursor`, since it's
compensating for a web-only OS quirk, not a shared behavior.

**Canvas `ImageData` is spec-mandated RGBA, not `Pixel`'s BGRX.**
win32Window.cpp's DIB and xlibWindow.cpp's XImage both happen to already
match `Pixel`'s documented in-memory layout (platform.h) on a little-
endian host, so neither needs a conversion pass; this is the one
platform where that coincidence doesn't hold, so `present()` does a real
per-pixel repack into a reused scratch buffer before handing it to
`fjPresentFrame`'s `putImageData`.

**`displayDpi()` has no EDID to fall back on.** Unlike
`GetDeviceCaps(..., HORZSIZE)` or XRandR's mm query, a browser exposes no
physical-monitor-size API at all -- `devicePixelRatio` (CSS's `96 *
ratio` convention) is the best available answer, but it conflates true
pixel density with whatever OS/browser zoom the user has chosen, the
same ambiguity `LOGPIXELSX` was rejected for elsewhere in this codebase.
Unlike the other two platforms, there's no honest "unknown" state to
return `nullopt` for -- a browser always answers *something* -- so this
never returns `nullopt`; the existing `Calibrate` (F5) flow is the
correction path here too, same as on a monitor with bad EDID data on the
other two platforms.

**Resize reuses Xlib's debounce shape, translated to JS timers.** No
`WM_ENTERSIZEMOVE`/`WM_EXITSIZEMOVE` equivalent exists here either --
`emscripten_set_resize_callback` fires `onResize` (cheap tick) on every
event and (re)starts an `emscripten_set_timeout`-based 300ms settle timer
whose firing calls `onResizeEnd`, same shape as xlibWindow.cpp's
`select()`-based approach, just with the browser's own event-loop timer
instead of polling a connection fd.

**Debug wasm is far slower than Debug native for the same code**, more
so than the gap between a Debug and Release *native* build on the other
two platforms -- confirmed by testing, not assumed: a synthetic-input
timing harness showed the actual `char -> redraw -> present` pipeline at
~0ms in both configs (ruling out the redraw pipeline itself), yet real
interactive typing against the Debug build was noticeably laggy and
resolved entirely by switching to Release. `-O0` plus `-sASSERTIONS=1`
(bounds/heap checks on effectively every memory access) is enough
overhead in a wasm interpreter/JIT that it's felt on a normal per-
keystroke workload in a way native `-O0` never is. Left `web-debug` at
`-O0` rather than trading away step-debuggability by default (a
`-O1`-for-debug option was raised and declined) -- `web-release` is the
one to actually use/demo against day to day; `web-debug` is for when you
specifically need to step through wasm in the browser's own debugger.

**Custom `--shell-file` (`src/webShell.html`).** emcc's default shell is
a demo page (a spare `#canvas`, resize/pointer-lock checkboxes, a
Fullscreen button, a status/progress readout) meant for apps that hand
Emscripten's own runtime a canvas to drive (`Module.canvas`, a GL
context) -- `webWindow.cpp` never does that; it builds and owns its
canvas entirely from C++/EM_JS. `webShell.html` is close to empty: just
the `{{{ SCRIPT }}}` placeholder emcc actually requires.

**Known gap: calibration doesn't survive a page reload.**
`main.cpp`'s `saveCalibratedDpi`/`startupDpi` use plain `std::ofstream`/
`ifstream` against a path from `getenv("HOME")` etc. -- Emscripten
supplies a `HOME` and a working `<filesystem>`, so the write succeeds
with no code change needed, but it lands in Emscripten's default MEMFS
(in-memory, wiped on reload), not a real file the way it is on Win32/
Linux. Degrades gracefully (no crash, calibration still works for the
running session), just doesn't persist across a reload the way it does
on the other two platforms. Not fixed -- would need IDBFS (or
localStorage) mounted and explicitly synced if this ever needs to stick.

**Toolchain**: emsdk installed to `%USERPROFILE%\emsdk` (not checked
into the repo -- a dev-machine-local install, same category as an IDE or
compiler install, matching how Visual Studio/CMake themselves aren't
vendored either); `CMakePresets.json`'s `web` preset points
`CMAKE_TOOLCHAIN_FILE` at
`$env{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`.
`emsdk_env.ps1`/`emsdk activate --permanent` didn't reliably put
`upstream/emscripten` (where `emcc` actually lives) on `PATH` in testing
on this machine -- building from a fresh shell needs `emcc`/`ninja` on
`PATH` some other way (e.g. prepend
`%EMSDK%;%EMSDK%\upstream\emscripten;%EMSDK%\ninja\<ver>_64bit`
manually) rather than assuming emsdk's own env scripts handled it.

## TODO

### Platform / architecture

- [x] Linux (Xlib) platform shell
- [x] Web (Emscripten/Canvas) platform shell -- see "Web (Emscripten)
      shell" above for the design (hidden-`<input>` text capture, the
      `simulate_infinite_loop` run() model, the Caps Lock workaround,
      etc.) and its one known gap (calibration doesn't survive a page
      reload, MEMFS only)
- [ ] Automated regression test/harness (scheduled after the Web shell
      above): drive `Cursor`/`CardItem`/`CardStack` with scripted
      `KeyEvent` sequences (headless, no platform window needed -- same
      approach phase 3's verification used) and assert on the resulting
      state/rendered output for the keyboard flows below (typing,
      navigation mode, TOC links, delete toggle, caps-lock handling) plus
      edge cases, so there's a fast, repeatable "does the app still work"
      check instead of a manual pass. Supersedes the old "manual pass"
      TODO that stood here.
- [ ] Port the old "darken all but the current row while typing" effect
      -- `Canvas::blendRect` (real alpha blending) exists now, nothing
      uses it for this yet
- [ ] `CMakePresets.json`'s `windows-x64`/`windows-arm64` configure
      presets still set `CMAKE_PREFIX_PATH` to the old Qt install path --
      dead now that Qt is gone, harmless but unused
- [ ] ARM64 has never actually been built on this branch (only x64 has
      been compiled/run)
- [x] `tools/offline/bakeFont`'s target-width formula used a hardcoded
      4.8in "usable width" instead of the real `CardItem::sideMargin_px`
      relationship (card width == `(Body::kColsPerRow + 4) *
      atlas.cellWidth` spanning `Card::kWidth_in`) -- baked every atlas
      rung about 2.4% too wide, which meant even a "perfect" pickAtlas
      match still needed a shrinking resample, a small but constant blur
      no resampler could fully hide. Fixed and resources/hackAtlas.*
      regenerated (linux-shell branch).
- [x] `README.md` updated for the current preset-based workflow
      (`cmake --workflow --preset windows-x64-debug`, plus Linux/Web)
      and dropped Qt dependency
- [x] `scripts/setup.ps1` was dead (only ever fetched/built Qt) --
      rewritten to install the current prerequisites instead: git/CMake/
      Visual Studio 2022 (Desktop C++ workload) via `winget`, plus emsdk
      for the Web toolchain. Idempotent (checks what's already installed/
      set up before doing anything), so it doubles as an environment
      sanity check, not just a first-time setup script.
- [ ] Known regression from the Qt port: retroactive title propagation to
      *already-created* continuation cards when the thread's title
      changes isn't implemented (`CardStack::add`'s `ThreadMode::Continue`
      only copies the title at creation time) -- judged low-value at the
      time, flagged here in case it matters later

### App features

- [ ] Proof of Concept
  - [X] When typing, hold caps down, enter should be shackCardNo
  - [~] Convert RowItem to be derived from QGraphicsItem -- superseded:
        `RowItem`/`QGraphicsItem` don't exist anymore, see ARCHITECTURE
  - [~] RowItem does background, is shadeded when not active row. --
        superseded by the "darken while typing" TODO under
        Platform/architecture above (same effect, `Canvas::blendRect` is
        the modern equivalent of what this item was asking for); the two
        sub-items below (moving background drawing out of `CardItem`,
        removing highlight code from `Cursor::draw`) describe a
        Qt-specific refactor that's moot -- `Cursor::draw`'s `drawCard`
        drawing the background directly is the current design, not a
        leftover
  - [X] Update variables to use coodsys _XXXX as described above -- the
        `_px`/`_in` convention (see "Coordinate system" above) is used
        consistently throughout the current core
  - [X] When navigating links, left goes to previous location
  - [X] When you follow a TOC to a page, start in navigation mode with the prev thread link highlighted
  - [X] CMake presets
  - [X] Make CapsLock state return to normal while fj is running, but is no longer the active window
  - [X] Deleting
    - [X] Support deleting all cards except card 1 TOC and it's thread
    - [X] When a card is deleted, skip over it when moving through thread
    - [X] D toggles deletions of collection cards
    - [X] When a deleted card gets enter, add a new card to collection/TOC
  - [X] Title
    - [X] First card in collection allows editing of title
    - [X] First card of TOC allows editing of title, except card 1 TOC
    - [X] When title is changed, TOC and entire thread is updated
  - [X] Continuing TOC
    - [X] New content should prev thread should take you back to correct TOC Page
      - [X] Also should select the right TOC entry so right will take you back to where you were
    - [X] Once TOC grows past a page, new TOC should be created
      - [X] New TOC points to prev TOC
      - [X] Prev TOC point to new TOC
  - [ ] Continuing collection from different card stack
    - [ ] Press enter on non-current year collection to continue collection
    - [ ] Update last thread card to point to new collection card in current year
    - [ ] Update current year TOC to point to start of collection
    - [ ] New Card prev thread is for non-current year
  - [ ] Continuing TOC from different card stack
    - [ ] Press C for new collection or T for new TOC
      - [ ] Create a new TOC in current stack to continue TOC thread
        - [ ] Prev points to TOC in other stack
        - [ ] Current stack TOC points to start continued TOC
      - [ ] New card created in current card stack
        - [ ] New TOC point to new card
  - [X] Link navigation
    - [X] Need way to edit title on toc...'e' for edit?
    - [X] Use N for navigation mode
    - [X] Go to prev/next thread
    - [X] On TOC, goes through thread entries and navigation thread
    - [X] Press right arrow to go to link
    - [X] Press left arrow to go back to previous place
    - [X] When you follow a TOC entry, cursor should select next thread
      - [X] This means pressing right will move through cards in thread
  - [X] Verify you can edit with 'E'
    - [X] E -> mode is typing, navigation is cursor, block cursor
      - [X] When editing title, should show as a highlight
    - [X] N -> toggle between links and edit
      - [X] Nothing to edit (second page of TOC) - do nothing
        - [X] UI should indicate issue via shaking no - left and right?
      - [X] Links - box around link
      - [X] Edit - box around cursor location
  - [ ] Use / for Help
  - [ ] Save/load from disk automatically
- [ ] Start personal use
- [ ] UI - make it so user knows what to do instictively
  - [ ] Cursor::shakeCardNo()
- [ ] Test on Linux Raspberry Pi Zero
- [ ] Next
  - [ ] Ability to access characters not available from basic keyboard
  - [ ] Use x for TODO/DONE/No TODO
  - [ ] 0-9 as bookmarks
    - [ ] Shift+# to store
  - [ ] Use S for Search
  - [ ] +Shift to move to beginning/end
    - [ ] Shift+U : First card in stack
    - [ ] Shift+O : Last card in stack
    - [ ] Shift+M : First card in thread
    - [ ] Shift+. : Last card in thread
    - [ ] Shift+I : Master card stack TOC
    - [ ] Shift+K : Current card stack TOC
    - [ ] Backspace : Toggle previous card
  - [ ] User links
    - [ ] How to create a link? "L" then year-cardnumber? Let user find card they want to link to?
    - [ ] Create
    - [ ] Modify
    - [ ] Delete
  - [ ] When you can't go back (left arrow) because there is no history, give user feedback so they know

## Keyboard Mapping

- Number Row
  - `
  - 0-9         Bookmarks, +Shift to set, press to go
  - -
  - =
  - backspace   Previous card toggle, while typing delete char, +Shift delete word
- Tab Row
  - tab
  - q
  - w
  - e           Edit - typing
  - r
  - t           New TOC card
  - y
  - u           Prev card, +Shift first card in stack
  - i           Up, +Shift master card stack TOC
  - o           Next card, +Shift last card in stack
  - p
  - [
  - ]
  - \
- Caps Row
  - a
  - s           Search
  - d           Delete/undelete card
  - f
  - g
  - h
  - j           Left
  - k           Down, +Shift current card stack TOC
  - l           Right
  - ;
  - '
  - enter       Continue content card, nothing on toc
- Shift Row
  - z
  - x           Todo/completed/no todo
  - c           New content card
  - v
  - b
  - n           Navigation mode: up/down to select links, right to follow link, left to go back
  - m           Prev thread card, +Shift first thread card
  - ,
  - .           Next thread card, +Shift last thread card
  - /           Help - go to help card stack
- Spacebar Row
  - spacebar

## Ortholinear Keyboard

- KBDcraft Israfel
  - $70
  - <https://a.co/d/03qzhl5Z>
  - 5 rows, 6 col per side (left and right hand)
  - Possible layout
    Left                Right
    ;     1 2 3 4 5     6 7 8 9 0 bs        (No =)
    tab   q w e r t     y u i o p -         (No [ and ] and \)
    caps  a s d f g     h j k l ' enter
    shift z x c v b     n m , . / shift
           spacebar     spacebar

Mode-transition dispatch (Command/Typing keyboard mode, Link/Cursor
navigation mode) sketched here originally is now real, implemented code
-- see `Cursor::handleKey` in `src/cursor.cpp`.
