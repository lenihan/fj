# PLAN
- 
- [PLAN](#plan)
  - [DESIGN](#design)
  - [ARCHITECTURE](#architecture)
    - [Dependency policy](#dependency-policy)
    - [Core / platform-shell split](#core--platform-shell-split)
    - [Coordinate system](#coordinate-system)
    - [Physical accuracy](#physical-accuracy)
    - [Font atlas](#font-atlas)
    - [Window resizing](#window-resizing)
    - [Web (Emscripten) shell](#web-emscripten-shell)
  - [TODO](#todo)
    - [Platform / architecture](#platform--architecture)
    - [App features](#app-features)
  - [Keyboard Mapping](#keyboard-mapping)
  - [Ortholinear Keyboard](#ortholinear-keyboard)

## DESIGN

- Glossary
  - Card - A 3" x 5" index card, with typing on one side (colloquial
    name; renders landscape, 5in wide x 3in tall -- see `layout.h`'s
    `Card::kWidth_in`/`kHeight_in`)
    - Content card - Contains anything you want, has lines indicating rows
    - TOC card - Table of contents card that points to other TOC/content cards, no lines (blank)
  - Card stack - 1 or more cards
    - Master card stack - Read-only with a Help TOC (one entry per topic,
      each its own thread) and a list of year card stacks
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
      - Help TOC (one entry per topic)
      - Year card stacks (1 or more)
    - Help topic cards (each topic its own thread)
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
- [x] Automated regression test/harness: `tests/cursorTests.cpp`
      (Catch2 -- see README.md's Tests section for the one deliberate
      exception to the zero-dependency policy) drives `Cursor` directly
      with scripted `KeyEvent` sequences, headless, no platform window
      needed. 13 `TEST_CASE`s covering typing/backspace, Caps Lock hold,
      every command-mode key, thread-boundary navigation (both `.`/`m`
      and ordinary `i`/`k`), deleted-card skipping, a full link-walk from
      the master TOC, and cross-year thread continuation. Found and fixed
      two real bugs along the way, not just test-writing: `right()`'s
      Link-mode branch never skipped a deleted link target (every other
      navigation path already did), and nothing enforced "new content
      only goes in the current year" until `setYear()` was changed to
      refuse moving backward. `cmake --workflow --preset
      windows-x64-debug` (and the Linux equivalent) now build *and* run
      `ctest` in one command (`CMakePresets.json`'s `testPresets`).
- [x] Real initial content, replacing the single blank debug "Help"
      card `Cursor`'s constructor used to seed: Master's own stack
      (read-only, TOC linking to the current year's stack and a Help
      TOC), the current year's own empty stack (its TOC linking back to
      Master), and four Help topics (What is fj/Modes/Keys/Finding your
      way, one a two-page thread) explaining the app using the keyboard
      panel's own vocabulary. All in a new private
      `Cursor::setupInitialContent()`; `currentCalendarYear()` (a free
      function, not a `Cursor` member -- shared with
      `tests/cursorTests.cpp`, which needs the exact same value to point
      scratch content at the same stack) reads the real year via
      `std::chrono`'s C++20/23 calendar API.

      Master's read-only flag is enforced for real: `CardStack` already
      had an unused `readOnly()` (confirmed by grep -- nothing consulted
      it), now wired into `addNewCard()`/`addContinuationCard()`'s own
      gate so `c`/`t` can't add to Master, on top of every individual
      Master card already being marked read-only (blocks typing/editing
      via the existing per-card check).

      Four real bugs found live wiring this up, not by inspection, each
      in shared logic a future feature could hit again:
      1. `m_capsTapLatched` (whether cmd's latch was engaged via a real
         tap) started false even though the constructor now lands in
         command mode directly (`enterCommandMode()`, not a tap) -- so
         the very first cmd tap after launch did nothing until a second
         tap. Now seeded `true` at the end of `setupInitialContent()`.
      2. The constructor left `m_row` at 0 (title) when landing on
         Master's TOC -- `right()` treats row 0 as title-editing
         *before* it even looks at navigation mode, so pressing `l`
         there just moved within the (empty, read-only) title text
         instead of following Master's own link. Now leaves `m_row = 1`.
      3. `TOCItem::addToTOC()` unconditionally resets the current-link
         index to 0 on every call, regardless of `setupLinks()`'s own
         "skip the up-arrow if there is one" logic -- invisible for
         Master's own TOC (no up-arrow there) but meant Help's TOC
         defaulted to its up-arrow instead of its first topic. Fixed by
         explicitly calling `setCurrentLink()` after all four topics are
         added.
      4. A mouse tap on cmd sent a single collapsed `KeyEvent` instead of
         a real press+release pair, so `Cursor`'s tap-vs-hold detection
         could never actually fire from a click at all -- found
         verifying the above live. `resolveKeyGesture`'s `CapsToggle`
         same-key branch now always emits a real pair.

      Also, live user feedback while building this (letter relocation
      since reverted -- see the "fourth round" entry below; the on-card
      cursor coloring below is still current): the keyboard panel's
      edit/navigation mode keys moved next to cmd -- first to left panel
      row 2's 2nd/3rd keys (`q`/`w` swapped into their old spots), then,
      on a second round of feedback once it was visible on screen, down
      one more row to sit directly beside cmd itself (row 3, `a`/`s`
      swapped into their old spots instead) -- spatially clustering all
      three mode keys together, given how central they are. `Cursor`
      dispatches on codepoint, never on physical position, so nothing
      outside `keyboardPanel.cpp`'s layout tables needed to change either
      time. The on-card cursor indicator (not just the keyboard panel)
      also now colors itself per mode -- green
      (general command)/blue (Navigation)/red (typing, unchanged) --
      bold/saturated hues rather than the panel's own pale tints, for
      contrast against the card background.

      47 tests passing (Windows/Linux), web builds clean. Verified live:
      Master's TOC shows both entries and lands in command mode (not
      mid-edit); the year link's TOC is empty and its own link returns
      to Master; the Help link opens a TOC of all four topics, each
      opening directly (not via its own up-arrow) and read-only.
- [x] The `m_year` gap flagged above turned out to have two genuinely
      different fixes, not one:
      1. `setupInitialContent()` never actually advanced `m_year` off
         `Master::kYear` once its own setup finished -- so `addNewCard()`/
         `addContinuationCard()` (`c`/`t`) always resolved
         `m_yearToCardStack.at(m_year)` to Master's own, permanently
         read-only stack, silently doing nothing *anywhere*, not just on
         Master. Fixed with one line, `m_year = currentYear;`, right
         after Master's own content is built.
      2. `lastCardNumber()`/`nextCard()`/`prevCard()` (`u`/`o`) had the
         opposite problem: they *also* read `m_year`, but what they
         actually need is "whichever stack the cursor is currently
         viewing," which isn't always the same thing (`m_year` means
         "which stack new content targets" -- the two concepts the
         now-superseded gap above worried about conflating). Fixed by
         switching those three to `m_currentCard->year()` instead.
      Found via a new regression test (`"pressing 'c'/'t' on the year
      TOC..."`) that reached the year TOC by following Master's own
      link, the same path a real user takes, rather than the old
      `setYear()` shortcut every earlier test used.
- [x] Fixing the above surfaced a second, deeper bug in the same area:
      a hold-cmd-drag-to-key chord whose chorded command itself changes
      mode (`c`/`t`/`e`, via `enterTypingMode()`) had its own transition
      immediately undone by the *same chord's* CapsLock-release
      bookkeeping -- `enterTypingMode()`'s existing "clear the tap
      latch" reset doesn't apply mid-hold (`m_capsDown` true), so the
      release handler's stale `m_wasTypingMode` snapshot (captured
      *before* the chord ran) incorrectly reverted back to Command mode.
      Fixed with a new `m_modeChangedDuringHold` flag: set by
      `enterTypingMode()` specifically when `m_capsDown` is true, and
      checked first by the release handler, which then trusts whatever
      mode the chorded command already set instead of reverting it.
      Also found via the same new regression test above, which failed
      once (`isTypingMode()` false after `sendCommand(cursor, 'c')`)
      before this fix and passed after.
- [x] MSVC-only rendering bug: `cardItem.cpp`'s `U"↑"`/`U"→"`
      link-arrow literals rendered as blank cells on Windows only --
      MSVC's default source encoding is the Windows ANSI code page, not
      UTF-8, so the literal's UTF-8 bytes were being decoded one at a
      time against the wrong code page (confirmed via temporary
      codepoint tracing: `226, 8224, 8216` instead of one `8593`).
      Linux/web unaffected (GCC/Clang already assume UTF-8). Fixed with
      a build flag, not a source-code workaround: `add_compile_options(
      /utf-8)` under `if(MSVC)` in `CMakeLists.txt`.
- [x] Read-only cards (most of Master's, most days) now show it on the
      keyboard panel instead of just silently ignoring `e`/`d`:
      `CardItem::canEdit()`/`canDelete()` (the exact predicates
      `enterTypingMode()`/`setDeleted()` already enforced internally)
      are now exposed publicly so `main.cpp` can know in advance, and a
      new `ModeColor::Disabled` (gray) overrides `e`/`d`'s usual color
      in `modeColorFor` whenever they say no. Clicking a disabled key
      goes further: its text replaces itself with an explanation
      ("Read-Only") and its face inverts (the same visual weight a held
      key gets) for a few seconds -- a new cross-platform one-shot timer
      primitive, `PlatformWindow::scheduleOnce(delay_ms, callback)`, done
      once per shell (Win32 `SetTimer`/`WM_TIMER`; X11 by extending the
      existing resize-settle `select()` loop to also track timer
      deadlines; web via `emscripten_set_timeout`), since nothing like it
      existed yet. `main.cpp`'s own `messageGeneration` counter guards
      against a stale timer clearing a *newer* message out from under it.
      Debugged live down to a genuine one-frame race in how a synthetic
      `PostMessage` test script's fixed capture delay could land outside
      a real (correctly-firing) 5-second window -- not an app bug; the
      underlying logic was confirmed correct via a temporary timestamped
      trace before removal.
- [x] Extended the same disabled-key pattern to Navigation mode and to
      typing mode's own coloring, all from one round of live feedback:
      `j` (back) grays out via a new `Cursor::hasLinkHistory()` when
      there's no history to pop ("No history" on click); `i`/`k`
      (prev/next) gray out via a new `CardItem::linkCount()` when a card
      has one link or fewer ("No links" on click) -- both wired through
      the same `showXDisabledMessage`/`scheduleOnce` machinery above.
      Building this surfaced a real latent bug: every `addNewCard()`/
      `addContinuationCard()` call inside `setupInitialContent()` itself
      (building Help's content) pushes onto the same `m_linkHistory`
      stack a real user's `j` pops from, so `hasLinkHistory()` read true
      the instant the app launched, before any real navigation -- found
      via a new headless test, fixed with `m_linkHistory.clear()` at the
      end of setup. Also: general command mode's `i`/`k`/`j`/`l` legends
      are now actual arrow glyphs (`↑↓←→`) instead of
      the words "up"/"down"/"left"/"right" -- needed baking two more
      codepoints (`←`/`↓`) into `bakeFont`'s glyph set alongside
      the two link-arrows it already had, then re-running it. And typing
      mode's uniform red became three tiers keyed off each key's own
      codepoint -- letters brightest (`EditLetter`), digits a shade less
      (`EditNumber`), everything else least (`EditOther`, punctuation/
      shift/tab/enter/bs/spacebar) -- rather than one flat color for
      every key.

      53 tests passing (Windows/Linux), web and Windows builds clean.
      Verified live on Windows: Master's TOC correctly refuses `e`
      ("Read-Only", inverted) while a fresh year-stack card entered via
      `c` accepts it normally; a fresh launch's `j` (back) is gray until
      a link is followed, then live, then gray again after backing out;
      a TOC with only its own back-link grays `i`/`k` ("No links" on
      click) while Master's two-entry TOC keeps them live; typing mode
      visibly shows three distinct red shades across letters/digits/
      everything else.
- [x] A follow-up round of live feedback on the above, five pieces:
      1. The disabled-key message's 5-second timeout felt too long --
         shortened to 3.
      2. `i`/`k` (prev/next) in Navigation mode now disable
         *independently* via new `CardItem::isAtFirstLink()`/
         `isAtLastLink()`, replacing the old shared "linkCount() <= 1"
         check -- a card with several links only grays out whichever end
         you're actually sitting at, not both just because there's more
         than one.
      3. Typing mode's three red tiers became four: tab/shift/enter/bs
         (`ModeColor::EditControl`, new) now read lighter than
         punctuation/spacebar (`EditOther`), classified the same way as
         before -- Fire+Char keys that are neither letter nor digit stay
         `EditOther`; everything else (not Fire+Char at all) drops to
         `EditControl`.
      4. `m`/`.` (prevT/nextT) now gray out in general command mode via
         two new `Cursor` predicates, `hasPrevThreadCard()`/
         `hasNextThreadCard()` -- refactored out of `prevThreadCard()`/
         `nextThreadCard()`'s own "skip deleted cards" walk so both the
         action and the predicate share one implementation. Narrower
         than it first sounds: `CardStack::add()`'s `ThreadMode::New`
         branch always sets a fresh card's `threadPrev()` to whatever
         TOC it was created from, so an ordinary content card *always*
         has a live "back to the TOC" thread-prev -- only a stack's own
         TOC (`threadPrev() == nullptr`, nothing points to it) actually
         triggers "No prevT"/"No nextT". Found via a test written on the
         wrong assumption (a fresh scratch card has no prev thread
         card) -- it doesn't; that back-link is real, existing,
         intentional behavior.
      5. General command mode's arrows (`i`/`k`/`j`/`l`) now gray out on
         a read-only card, and -- unlike every other disabled key so
         far -- this one isn't purely cosmetic: `Cursor::up()`/`down()`/
         `left()`/`right()`'s own `NavigationMode::Cursor` branches now
         refuse (`shakeCardNo()`, matching `enterTypingMode()`'s own
         canEdit() gate) instead of silently moving the cursor, since
         without that the panel would show "disabled" on a key that
         still visibly worked. Deliberate, not an oversight: these
         arrows exist to position the cursor for *editing*, meaningless
         on a card you can't edit anyway -- Navigation mode's own
         `i`/`k`/`j`/`l` (unaffected, different branch entirely) is how
         a read-only TOC like Master's is meant to be browsed.

      Refactored the keyboard panel's own parameter lists along the way
      -- `modeColorFor`/`drawKeyboardPanel` were accumulating a new bool
      pair per disabled key (six-plus and climbing), easy to
      transpose positionally and hard to read at any call site. Two new
      types replace them: `KeyDisabledState` (one bool field per
      disabled reason, aggregate-initialized with designated
      initializers at call sites, e.g. `{.editDisabled = true}`) for the
      persistent gray styling, and `KeyMessage` (a codepoint + which
      command sub-state it belongs to + the explanation text) for the
      one "why didn't that work" message that can be showing at a time
      -- replacing five-plus `showXDisabledMessage` bools with a single
      `std::optional<KeyMessage>`. The `isLinkMode` field on `KeyMessage`
      does the job the old per-message `isLinkMode`-gated `if`s used to:
      the same physical key (`i`/`k`/`j`) can now mean two different
      things with two different messages depending on command sub-state
      (Navigation's "no links"/"no history" vs. general command's new
      "read-only" arrows), so a message set just before a mode change
      doesn't misapply to the wrong meaning until its timeout.

      56 tests passing (Windows/Linux), web and Windows builds clean.
      Verified live on Windows: a card with two links grays `i` at the
      first and `k` at the last, independently; typing mode shows four
      visibly distinct red shades, tab/shift/enter/bs palest; Master's
      TOC (no threadPrev/threadNext, read-only) grays `prevT`/`nextT`
      ("No prevT"/"No nextT" on click) and all four arrows ("Read-Only"
      on click, and the cursor genuinely doesn't move); the disabled-key
      message visibly persists past 1.2s and is gone by 3.5s.
- [x] A third round of live feedback, four more pieces:
      1. `u`/`o` (prevCard()/nextCard(), by absolute card number) used to
         silently switch into Navigation mode whenever the adjacent card
         happened to be a TOC -- `showCard()` always forces that for any
         TOC target, right for "followed a link into one" (Navigation is
         how you'd want to browse it) but wrong for "paged onto one by
         number," which isn't following a link at all. Fixed by saving
         `m_navigationMode` before `prevCard()`/`nextCard()`'s own
         `showCard()` call and restoring it after, rather than touching
         `showCard()` itself (every other caller's own TOC landing *does*
         want the switch).
      2. `u`/`o` now gray out in general command mode via two new
         `Cursor` predicates, `isAtFirstCard()`/`isAtLastCard()` -- the
         same "would this be a no-op" check `prevCard()`/`nextCard()`
         already made before calling `shakeCardNo()`, now factored out
         and exposed so the panel can know in advance ("No prev"/"No
         next" on click).
      3. The on-card cursor indicator itself (not just the keyboard
         panel) now draws nothing at all in general command mode on a
         read-only card, rather than a green triangle suggesting an
         action that's refused the moment you take it -- Navigation
         mode's own cursor (a different branch of `Cursor::draw()`) is
         unaffected, since browsing a read-only TOC's links is exactly
         how it's meant to work.
      4. The disabled-key message's timeout, already shortened from 5s
         to 3s two rounds ago, felt *still* too long -- shortened again
         to 1s.

      Live-testing the shortened timeout surfaced the same lesson from
      the original Read-Only investigation, now sharper with a smaller
      budget: capturing a screenshot in the same synthetic-click
      invocation (down+up+150ms fixed delay) reliably lands on the
      *press* frame, not the post-release one, because the two
      `PostMessage`s and the app's own dispatch+redraw don't always
      finish inside 150ms -- looks identical to the message already
      being gone (inverted face, but the key's ordinary legend text, not
      the explanation) unless the click and the screenshot are split
      into separate calls with real settle time between them.

      58 tests passing (Windows/Linux), web and Windows builds clean.
      Verified live on Windows: paging with `o` from Master's TOC onto
      the read-only Help TOC stays in general command mode (`cmd`/`edit`/
      `nav` legends throughout, never Navigation's `back`/`next`/`go`);
      `prev` grays out on card 0 with "No prev" on click; no cursor
      triangle anywhere on Master's stack; the message shows at 400ms
      and is gone within roughly a second.
- [x] A fourth round of live feedback reverted the physical letter
      relocation from the two entries below (`+card`/`+toc`/`del` next
      to `cmd`'s row, edit/nav next to `cmd` itself, and their various
      `a`/`s`/`q`/`w` knock-on swaps) -- the user was explicit only a
      key's *command-mode function* should ever move, never the letter
      it produces: Cursor dispatches command mode on the exact same
      codepoint typing mode sends (there's no separate "command
      identity" for a key, independent of that one codepoint -- see
      `fillKeyEvent`), so relocating a function inherently means
      relocating whatever letter used to carry that codepoint too,
      which is exactly the muscle-memory confusion this caused. Fixed
      by reverting `kLeftKeys`/`kRightKeys` to real-QWERTY letter
      positions throughout -- `c`/`t`/`d`/`e`/`n` (+card/+toc/del/edit/
      nav) simply live wherever their own mnemonic letter's real
      position already is (scattered, not clustered near `cmd`), same
      as `d` (delete) and `e`/`n` always did before any of this
      relocation work started. A new headless test locks each letter to
      its real row/col so the mistake can't quietly come back.

      Also: the spacebar now lights up on *both* panels together
      whenever either is pressed (mouse or physical), matching shift's
      existing cross-panel behavior, rather than just the one side
      actually pressed -- a new `spacebarEngaged` bool threaded through
      `drawKeyboardPanel` the same way `shiftEngaged` already was.

      59 tests passing (Windows/Linux), web and Windows builds clean.
      Verified live on Windows: typing mode's letters now read
      `q w e r t` / `a s d f g` / `z x c v b` on the left panel and
      `y u i o p` / `n m , . /` on the right, matching a real keyboard
      exactly; pressing and holding the left panel's spacebar inverts
      both spacebars at once.
- [x] A fifth round: the user wanted +card/+toc/del/edit/nav on
      specific keys -- `q`/`w`/`e`/`a`/`s` respectively -- rather than
      wherever their own mnemonic letter's real position happened to
      land. Since a key's command dispatches on the exact codepoint
      typing mode sends (no separate "command identity" -- see the
      fourth round above), this time the reassignment happens once, at
      the source, in `Cursor::handleKey`'s own switch -- not in the
      keyboard panel's layout table, which stays real-QWERTY throughout.
      `keyboardPanel.cpp`'s `commandLegendFor`/`modeColorFor` and
      `main.cpp`'s disabled-key-message codepoints follow the same
      remap. This *is* a real behavior change (not just cosmetic), so
      every test that invoked one of these five commands via its old
      letter needed updating to the new one -- about two dozen call
      sites in `cursorTests.cpp`, each checked individually rather than
      blindly find-replaced, since old-`e` (edit) and old-`d` (delete)
      landed on new-`a` and new-`e` respectively -- a genuine swap, not
      a simple rename, that a naive substitution would have corrupted.

      Two more pieces of feedback in the same round:
      - The keyboard panel's own font size now picks the *largest* baked
        atlas that still fits a key's text, not just whichever is
        numerically closest to the target -- those aren't the same
        thing (closest can round up and overflow past a key's border).
        `pickPanelAtlas` no longer delegates to `canvas.h`'s `pickAtlas()`
        for this reason: that function's "closest either way" is the
        right call for the card body/title (matching physical pixel
        density, where landing a little over or under costs nothing but
        blur), but wrong for a key label with a hard ceiling.
      - `f`/`j` now draw the same short raised-bump marker a real
        keyboard puts on its own home-row index-finger keys, so touch
        typing on the emulated panel works the same way -- drawn in
        whatever color the key's own text is using, so it stays visible
        against any face color or inversion state.

      59 tests passing (Windows/Linux) -- same count as the fourth
      round's, since this changed existing tests' letters rather than
      adding new ones; web and Windows builds clean. Verified live on
      Windows: `+card`/`+toc`/`del`/`edit`/`nav` sit on `q`/`w`/`e`/`a`/`s`
      while typing mode still types real QWERTY throughout (`q` itself
      creates a new card and then types literally); `f`/`j` show a
      visible dash under their legend/blank face in every mode.
- [x] The panel's font was still too small even after the fifth round's
      "largest that fits" fix -- because what it was fitting was wrong.
      `kLongestSingleWidthLabel` (the string `pickPanelAtlas` sizes
      every key's font to fit) was 11, sized for `KeyMessage`'s own
      rare, brief disabled-key explanations ("No history" and friends)
      -- so the font for every ordinary single-letter key was being
      crushed down to fit an 11-character message that only ever shows
      on one key, for a second, occasionally. The user was explicit,
      found live: a real keyboard's keycap legend runs around a third of
      the keycap's own height, and letters here were reading far
      smaller than that. Fixed by shrinking the *target* string instead
      of the algorithm: `kLongestSingleWidthLabel` now tracks the
      longest string shown as a matter of course (`shift`/`enter`/
      `+card`/`prevT`/`nextT`, 5 characters), not the longest string a
      key can ever show -- the disabled-key messages now simply spill
      past their key's own border into whichever neighbor is drawn
      after them (that neighbor's own opaque face clips the overflow,
      reading as a truncated "Read-O" rather than a garish overlap),
      which is a fine trade for a message that's rare and brief on the
      one key showing it, against a visibly bigger font on every key,
      all the time.

      613 tests passing (unchanged -- no test pins this exact string),
      Windows/Linux/web builds clean. Verified live on Windows: every
      ordinary legend (`cmd`/`+card`/`prev`/`next`/arrows/etc.) reads
      noticeably larger, filling roughly a third of its key's height;
      clicking a disabled key still shows its explanation, now visibly
      clipped by the neighboring key rather than shrunk to fit --
      confirmed as the intended trade-off, not a bug.
- [x] One more round on the same font: single-character keys (every
      ordinary typing-mode letter/digit, single punctuation, and
      single-glyph command legends like the arrow keys) should be *much*
      bigger still, while multi-character keys (`shift`/`enter`/`+card`/
      `prevT`/etc.) stay exactly where the previous round left them --
      two different sizes on the same panel, not one uniform font
      shrunk to accommodate the longer strings. Added a second baked
      atlas, `pickPanelLargeAtlas` (mirroring `pickPanelAtlas`'s own
      "largest that fits" logic, now factored out into a shared
      `pickAtlasForCharBudget(pitch_px, charBudget)` both call), with a
      budget of a single character -- which, given the baked ladder's
      widest atlas is nowhere near a whole key's pitch, always resolves
      to the single largest available size. `main.cpp` picks and caches
      both atlases the same way it already cached one (re-picked only on
      a resize settle, not every frame), and `drawKeyboardPanel` now
      takes both, choosing per key by the rendered text's own length
      (`text->size() == 1`) rather than one fixed size for the whole
      panel.

      613 tests passing (unchanged -- no test pins exact pixel sizes),
      Windows/Linux/web builds clean. Verified live on Windows: general
      command mode's arrow legends (`↑←↓→`) read visibly larger than
      `prev`/`next`/`prevT`/`nextT` beside them; typing mode's letters,
      digits, and single punctuation (`;`/`,`/`.`) all render at the
      larger size while `tab`/`cmd`/`shift`/`spacebar`/`enter`/`bs`
      stay at the prior (smaller) size, side by side on the same panel.
- [x] (Superseded by the entry above, kept for history) `+card`/`+toc`/
      `del` (`c`/`t`/`d`) relocated to the left panel's row 2, keys
      2/3/4 -- another straight label swap in `keyboardPanel.cpp`'s
      layout tables, same as the mode-key relocations above; `a`/`s`/`q`
      take over their old spots.
- [x] Physical-keyboard key presses now flash the matching on-screen
      panel key -- the keyboard equivalent of the mouse hover/press
      feedback above, so typing on a real keyboard visibly connects to
      the emulated one, even for a key (an unbound letter, Shift itself)
      that has no effect on `Cursor` at all. Needed a new `platform.h`
      contract addition, the same shape as hover's `onMouseMove`: a
      `run()` callback, `onPhysicalKey`, firing on every physical key's
      own down *and* up edge (every `Kind`, unlike `onKey`, where only
      CapsLock ever reports a release) -- entirely separate from `onKey`,
      never routed to `Cursor`.

      The hard part, same as hover's own write-up: identifying *which*
      on-screen key to flash without conflating it with what the key
      currently *produces* (Shift changes that without changing which
      physical key it is). Solved the same way `KeyRect` already does it
      internally -- every physical key's identity is its **unshifted**
      `KeyEvent::Kind`/codepoint (a new `Kind::Shift`, used only by this
      callback, stands in for the physical Shift key itself, which has
      no dispatch meaning of its own). Each shell already has its own raw
      per-key identifier for other reasons, conveniently already
      shift-independent everywhere: Win32's `WM_KEYDOWN`/`WM_KEYUP` gives
      virtual-key codes (already position-based --
      `VK_A`..`VK_Z`/`VK_0`..`VK_9` equal their ASCII, so most of the
      table is trivial; the low-level Caps Lock hook feeds it too, since
      that key never reaches ordinary `WM_KEYDOWN`), X11's `KeyPress`/
      `KeyRelease` give a keysym at group/level 0, which is *already*
      shift-independent (no separate "unshift it" step needed at all),
      and the web shell's `KeyboardEvent.code` is spec-guaranteed
      layout-/shift-independent by design -- three small lookup tables,
      one per shell, covering exactly the ~40 keys the panel actually
      shows.

      `main.cpp` tracks a `physicalPressedKey` (found by scanning both
      panels' `layoutKeys()` for a `kind`/codepoint match) alongside the
      existing mouse-driven `pressedKey`, merged into the same
      `leftPressedRect`/`rightPressedRect` `drawKeyboardPanel` already
      expected -- no rendering-side changes needed at all. Shift gets its
      own `physicalShiftHeld` bool instead, merged into the existing
      `shiftEngaged`, since both physical shift keys already light up
      together whenever that's true.

      47 -> 48 tests passing (Windows/Linux), web builds clean. Verified
      live on Windows via synthetic `WM_KEYDOWN`/`WM_KEYUP` `PostMessage`s
      (not `SendInput` -- that requires real OS focus a background test
      process doesn't reliably have, confirmed by it silently going
      nowhere): a dead key (no effect on `Cursor`) still flashes solid
      with no text, exactly like a mouse-pressed dead key already did; a
      live key (`i`, "prev" in Navigation mode) flashes with its legend
      text visible, matching a mouse press pixel-for-pixel. X11/web are
      implemented and build clean but unverified live -- no Linux desktop
      or browser available to test interactively in this environment.
- [ ] Port the old "darken all but the current row while typing" effect
      -- `Canvas::blendRect` (real alpha blending) exists now, nothing
      uses it for this yet
- [ ] `CMakePresets.json`'s `windows-x64`/`windows-arm64` configure
      presets still set `CMAKE_PREFIX_PATH` to the old Qt install path --
      dead now that Qt is gone, harmless but unused
- [ ] ARM64 has never actually been built on this branch (only x64 has
      been compiled/run)
- [ ] Ortholinear keyboard emulator panels -- see "Ortholinear Keyboard"
      section's "Emulator keyboard panels (planned)" write-up for the
      spec (15"x5" window, per-key dynamic legends, clickable keys)
  - [x] Phase 1: visual layout. `src/keyboardPanel.h`/`.cpp` (new,
        core) draws both panels' static key grids (`layoutKeys`, headlessly
        tested in `tests/keyboardPanelTests.cpp`); `main.cpp` composites
        left panel + monitor + right panel into a `deviceCanvas`,
        letterboxed to the window at a 3:1 aspect instead of the old 1:1
        square. `layout.h` gained `KeyboardPanel`/`Device` namespaces.
        Along the way, found and fixed a real startup bug (not
        keyboard-panel-specific, just newly exposed by a device 3x wider
        than before): `createPlatformWindow`'s CW_USEDEFAULT positioning
        lets Windows silently narrow an over-wide initial window to fit
        the screen without telling main.cpp, so the very first frame
        rendered assuming the original, too-wide size and got clipped.
        Fixed in `win32Window.cpp`'s `run()`, which now fires one
        real-size `onResizeEnd` before entering the message loop, reusing
        main.cpp's existing (already-correct) resize-settle handling
        rather than adding new logic. xlib/web shells weren't touched --
        not yet built/tested on this branch to confirm whether they need
        the same fix.
  - [x] Phase 2: clickable keys. `platform.h`'s `PlatformWindow::run()`
        gained `onClick(x_px, y_px, pressed)` (position + a press/release
        edge, nothing else -- your call, since a real hold/release gesture
        doesn't map onto a single mouse/touch point the way it does two
        fingers on a real keyboard); each shell (win32/xlib/web)
        translates its native mouse-button message into it.
        `keyboardPanel.h`'s `KeyRect` grew `clickable`/`kind`/`codepoint`
        (derived from the same label table phase 1 authored --
        "tab"/"shift" aren't clickable at all, since `Cursor::handleKey`
        doesn't implement either yet) plus `hitTestPanel`, both headlessly
        tested. `main.cpp`'s new `onClick` handler resolves a raw
        window-pixel click back through the letterbox math into a key and
        calls `cursor.handleKey` exactly the way `tests/cursorTests.cpp`
        already does by hand. Every key fires on its press edge only,
        except caps, which *toggles* engaged/disengaged per click rather
        than following press/release -- found via live testing, not
        designed up front: tracking "which key the press landed on" (for
        a matching release) broke the moment a different key was clicked
        while caps was conceptually still held, since that click's own
        press silently clobbered the tracking before caps's release ever
        arrived. Toggling needs no `platform.h` changes and matches the
        real constraint (one pointer can't hold one key while tapping
        another). Verified end-to-end via synthetic `WM_LBUTTONDOWN`/`UP`
        messages posted straight to a running `fj.exe`.
  - [x] Phase 2b: press feedback, and a symmetric latch/chord gesture for
        both caps and shift. A pressed key now inverts (face/label colors
        swapped) from press until release; panels redraw every frame
        instead of only on resize so the highlight stays live.
        `KeyRect::clickable` became a 4-way `Action` enum
        (`Fire`/`CapsToggle`/`ShiftToggle`/`None`), and a new pure
        `resolveKeyGesture` function (headlessly tested -- caps's two
        gestures and *both* shift keys' two gestures individually, per
        the user's explicit ask, not assumed identical) replaced the
        phase 2 toggle with the real spec: a plain tap on caps/shift
        toggles a persistent latch (mirroring a real Caps Lock's own
        latch); a press-drag-release chord (the mouse/touch equivalent of
        physically holding one key while tapping another) applies
        momentarily to one key, then reverts to whatever the latch was
        before -- caps drives a real `Cursor` mode exactly as phase 2
        did, shift is a pure client-side case transform on outgoing
        `Char` codepoints (`Cursor` never gets a shift concept). Each
        shell also gained mouse capture (`SetCapture`/`XGrabPointer`/a
        document-level web `mouseup` listener) so a drag ending outside
        the window still resolves correctly instead of silently losing
        its release edge.
        Found and fixed a real, pre-existing bug in `Cursor::handleKey`
        along the way (not new, and not mouse-specific -- reproducible on
        a real keyboard by holding physical Caps Lock a moment with
        nothing typed, then letting go): the CapsLock-release branch
        keyed off `m_lastKeyKind == CapsLock` to mean "stay in command
        mode," which is right for a *first* tap but wrong for a second
        one meant to release the latch, since nothing typed while held
        means `m_lastKeyKind` never changes either time. Fixed with a new
        `m_capsTapLatched` flag distinguishing "latched via a plain tap"
        from "mid-hold," verified with new `tests/cursorTests.cpp` cases
        (a second plain tap now correctly un-latches; a chord while
        already latched correctly stays latched) -- 33/33 tests passing
        on Windows and Linux, web builds clean.
  - [x] Phase 3: dynamic legends. Each key shows what it currently does,
        in one of three ways depending on `Cursor`'s live mode (including
        mid-press, before release -- see phase 2b's own preview
        precedent):
        - **Typing mode**: `typingLabelFor` previews what shift would
          actually produce -- capitals for letters, real shifted symbols
          for digits/punctuation (`;1234567890-',./` -> `:!@#$%^&*()_"<>?`,
          standard US-QWERTY), matching a real keyboard. This is also now
          a real behavior fix, not just a legend: shift+`1` actually
          types `!`.
        - **Navigation mode** (`Cursor::isLinkMode()`, a new accessor):
          `commandLegendFor` shows only `i`/`k`/`j`/`l` (as
          prev/next/back/go) plus `e`/`n`, blank everywhere else.
        - **General command mode**: `commandLegendFor` shows every key
          `Cursor::handleKey`'s switch implements (`up`/`down`/`left`/
          `right`/`edit`/`prev`/`next`/`del`/`+card`/`+toc`/`link`/
          `prevT`/`nextT`), blank for anything unmapped (`tab`, ordinary
          unbound letters).
        Two real bugs found and fixed designing this, both in shared
        `Cursor`/`resolveKeyGesture` logic, not just the display:
        1. Shift-latching while in command mode used to hand
           `Cursor::handleKey`'s switch an uppercase codepoint it
           couldn't match (the switch only has lowercase cases), silently
           breaking every command until shift released. `resolveKeyGesture`
           now takes an explicit `isTypingMode` parameter and only
           transforms the codepoint when it's actually going to be typed
           as text.
        2. **User-reported, not code-discovered**: navigation mode
           (`m_navigationMode == Link`, entered via `n`) let every general
           command key (`u`/`o`/`d`/`c`/`t`/`m`/`.`) keep firing even
           though only `i`/`k`/`j`/`l` actually change behavior there --
           called out directly as a bug, not a feature: entering
           navigation mode should make it exclusive, with the hold-caps
           chord as the way back to the general command set.
           `Cursor::handleKey` now blocks those seven keys while in Link
           mode (calling `shakeCardNo()` for feedback), only `i`/`k`/`j`/
           `l`/`n`/`e` stay live.
        39 tests passing on Windows and Linux (was 33 -- new
        `resolveKeyGesture`/`typingLabelFor`/`commandLegendFor` coverage
        plus a `cursorTests.cpp` case walking the whole exclusive-
        navigation-mode gate), web builds clean. Verified live via the
        same synthetic-click technique as phase 2b: shift and caps both
        preview their panel-wide effect immediately on press, before
        release; navigation mode's restricted legend set confirmed
        on-screen.
  - [x] Mode key background colors, then a full round of naming/behavior
        polish on top once the user started actually using it live:
        - **Colors**: the three mode keys (`cmd`, `n`, `e`) each show a
          fixed identity color (green/blue/red) whenever they're actually
          live; in typing mode every *other* key is red too (shift/tab
          included, not just the ones that do something while typing),
          full stop, cmd excepted. `n`/`e` only wear their identity color
          in general command mode -- in typing mode they're ordinary
          letters (this is where they type a literal `n`/`e`), and in
          Navigation mode they're blocked like any other non-i/k/j/l key,
          so they go blank (`ModeColor::None`) rather than keeping a color
          for something they can't currently do. `ModeColor::Command`
          (green)/`Navigation` (blue) new names avoid a real collision the
          user caught: two different things were both about to be called
          "navigation."
        - **Renamed** "caps" -> **"cmd"** (not "command": at 7 characters
          it would've become the panel's longest label, shrinking every
          other key's text to fit) and "Link mode" -> **"Navigation
          mode"**.
        - **Navigation mode is now fully exclusive**: only i/k/j/l stay
          live; `n`/`e` no longer have their own shortcuts back out (used
          to jump straight to general command / typing respectively).
          `cmd` is the only way out, and it steps up exactly one level at
          a time -- Navigation -> tap -> general command -> tap (or `e`)
          -> typing -- never a two-level jump. `cmd`'s own inverted/
          latched look is reserved for general command mode specifically;
          once you've stepped further into Navigation it shows its plain
          identity color instead, matching `n`/`e`'s own idle treatment.
        - **cmd never shows a persistent latched face at all anymore**,
          even in general command mode -- it inverts only for as long as
          it's actually held down (a momentary flash on press and again
          on release), unlike shift, which still does stay inverted for
          as long as its latch is on. cmd already has its own permanent
          green face to say "this is what I do"; a lasting invert on top
          was redundant once that existed.
        Two real, pre-existing bugs found live (not by inspection) while
        wiring this up, both now fixed:
        1. A mouse tap on `cmd` sent `Cursor::handleKey` a single
           collapsed event (`pressed` mirroring the *new* latch state)
           instead of a real press+release pair -- so `Cursor`'s own tap-
           vs-hold detection (which only recognizes a tap by seeing two
           separate events with nothing typed between) could never
           actually fire from a click. A second plain click on cmd could
           never release the latch back to typing at all; nobody had hit
           it because every prior live test happened to press something
           else in between. `resolveKeyGesture`'s `CapsToggle` same-key
           branch now always emits a real press+release pair.
        2. `enterTypingMode()` never cleared `m_capsTapLatched` when
           typing mode was reached via `'e'` instead of via cmd's own
           release branch -- so `cmd -> e -> cmd` flashed into command
           mode and immediately back out to typing in one tap, since the
           stale flag made the second tap believe it was releasing an
           already-latched mode rather than starting a fresh one.
           `enterTypingMode()` now resets the flag itself.
        Also added a **hover indicator**: every key (including ones that
        do nothing) shows a gold border outline while the pointer is over
        it, independent of press/latch/mode state. Needed a new
        `platform.h` contract addition -- `PlatformWindow::run()` gained
        an `onMouseMove(x, y)` callback, `(-1, -1)` meaning "pointer left
        the window" -- implemented per shell: Win32's `WM_MOUSEMOVE` plus
        `TrackMouseEvent`/`WM_MOUSELEAVE` (the former re-arms itself on
        every move, since it's one-shot), X11's `PointerMotionMask`/
        `LeaveWindowMask` plus `MotionNotify`/`LeaveNotify`, and the web
        shell's `mousemove`/`mouseleave` listeners on the canvas.
        `main.cpp` tracks a `hoveredKey` alongside the existing
        `pressedKey`, redrawing only on an actual key transition (mouse-
        move fires far more often than the panel's key boundaries
        change).
        47 tests passing on Windows and Linux (was 39), web builds clean.
        Verified live via the same synthetic-click/-move technique as
        earlier phases.
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
- [x] Known regression from the Qt port, fixed: retroactive title
      propagation to already-created continuation cards. `Cursor::enter()`
      now walks `threadNext()` and updates every continuation's title row
      when leaving `threadStart()`'s title row (only `threadStart()`'s
      title is ever editable -- every continuation's is a read-only copy
      made once at creation time). The TOC's own reference needed no fix
      at all: `TOCItem::text()` already reads the title fresh every draw
      (see `tocItem.h`'s header comment) -- confirmed by test, not
      assumed. See `tests/cursorTests.cpp`.

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
  - [X] Continuing collection from different card stack -- implemented
        via `Cursor::setYear`/`CardStack::add` (see "Web (Emscripten)
        shell" era's follow-on testing work); `tests/cursorTests.cpp` has
        a dedicated `TEST_CASE` for this
    - [X] Press enter on non-current year collection to continue
          collection -- no special-casing needed: `nextRowCreateCard()`'s
          existing `addContinuationCard()` path already creates in
          whichever stack `Cursor::m_year` (the current year) points at,
          regardless of which year the card being typed on belongs to
    - [X] Update last thread card to point to new collection card in
          current year -- `currentCard->setThreadNext(newCard)`, already
          unconditional in `CardStack::add`'s `Continue` branch
    - [X] Update current year TOC to point to start of collection --
          `CardStack::add` now calls `tableOfContents()->addToTOC(newCard)`
          when a continuation lands in a different year's stack than its
          predecessor
    - [X] New Card prev thread is for non-current year --
          `newCard->setThreadPrev(currentCard)` already pointed at the
          true previous card, not the new year's TOC; confirmed by test,
          not changed
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
  - [x] Use / for Help -- superseded: Help is a TOC entry reached from
        Master (see the new "Initial content" entry under Platform/
        architecture), not a dedicated key
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
  - Possible layout (superseded below -- see "Initial content..." entry
    under Platform/architecture for `caps` -> `cmd`)
    Left                Right
    ;     1 2 3 4 5     6 7 8 9 0 bs        (No =)
    tab   q w e r t     y u i o p -         (No [ and ] and \)
    caps  a s d f g     h j k l ' enter
    shift z x c v b     n m , . / shift
           spacebar     spacebar

Mode-transition dispatch (Command/Typing keyboard mode, Link/Cursor
navigation mode) sketched here originally is now real, implemented code
-- see `Cursor::handleKey` in `src/cursor.cpp`. Current physical layout
(`caps` -> `cmd` aside, identical to the sketch above -- every letter
stays at its real-QWERTY position; see the "A third round..." entry
under Platform/architecture for why an earlier version that physically
relocated `c`/`t`/`d`/`e`/`n`/`q`/`w`/`a`/`s` was reverted):

    Left                Right
    ;     1 2 3 4 5     6 7 8 9 0 bs
    tab   q w e r t     y u i o p -
    cmd   a s d f g     h j k l ' enter
    shift z x c v b     n m , . / shift
           spacebar     spacebar

### Emulator keyboard panels (planned)

The emulator window today is just the 5"x5" screen -- the two 5"x5"
ortholinear keyboard halves sketched above have never actually been
rendered. Planned: widen the window to 15"x5" (left keyboard panel +
screen + right keyboard panel, side by side), each panel drawing the 4
row x 6 col grid above plus a 5th row holding one double-wide spacebar
key aligned to the panel's screen-side edge (innermost column). Keys
are sized to a standard keycap pitch (a physical-keyboard reference
size, not derived from the card's glyph cell) -- these are meant to
look like real keys, not text.

Two things make this more than a rendering task:

- **Per-key display is dynamic, not a fixed printed legend.** Each key
  shows what it currently *does*, not just what letter it is -- e.g.
  `u` reads "Prev card" in Command mode, matching the Keyboard Mapping
  table above, and that legend has to change live as Command/Typing
  mode and Link/Cursor navigation mode change. This needs a source of
  truth for "what does key X do right now," derived from `Cursor`'s
  current mode state -- doesn't exist yet (`Cursor` has no public
  accessor for its own mode today).
- **Keys are clickable, not just decorative.** A mouse click on an
  on-screen key injects the same key event a physical keypress would.
  This is deliberately in scope now rather than deferred: it's the
  only way to exercise the ortholinear layout at all before real
  hardware exists. Needs per-key hit-testing and a synthetic-`KeyEvent`
  injection path into the same place physical keyboard input already
  enters (`Cursor::handleKey` / `platform.h`'s contract).

Both of these are core-architecture decisions (a new "current key
legend" data source, and a new synthetic-input path into the existing
key-event contract), not mechanical plumbing -- per this project's
working-style note (see `CLAUDE.md`), worth a first draft from the
user before implementation, not a generated design.
