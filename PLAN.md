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
window's current size at runtime, so live resizing re-renders close to
native resolution instead of blowing up one small bitmap. Glyphs are
anti-aliased (`Canvas::blendRect` blends each pixel by its baked coverage
byte). Only re-run `bakeFont.exe` if the font, the DPI ladder, or the
glyph set changes -- its output is checked in and not expected to change
otherwise.

## TODO

### Platform / architecture

- [ ] Linux (Xlib) platform shell
- [ ] Web (Emscripten/Canvas) platform shell
- [ ] Manual pass through the keyboard flows below (typing, navigation
      mode, TOC links, delete toggle, caps-lock handling) now that
      there's a real window to test against -- hasn't had a dedicated
      end-to-end check since the resize/calibration/rendering work landed
- [ ] Port the old "darken all but the current row while typing" effect
      -- `Canvas::blendRect` (real alpha blending) exists now, nothing
      uses it for this yet
- [ ] `CMakePresets.json`'s `windows-x64`/`windows-arm64` configure
      presets still set `CMAKE_PREFIX_PATH` to the old Qt install path --
      dead now that Qt is gone, harmless but unused
- [ ] ARM64 has never actually been built on this branch (only x64 has
      been compiled/run)
- [ ] `tools/offline/bakeFont`'s target-width formula uses a hardcoded
      4.8in "usable width" (assuming a margin) rather than the real
      `CardItem::sideMargin_px` -- a small precision gap between the
      atlas's chosen resolution and the card's true rendered margin
- [ ] `README.md` still describes the old Qt-based build (generic
      `cmake -S . -B build`, `scripts/setup.ps1`) -- needs updating for
      the current preset-based workflow (`cmake --workflow --preset
      windows-x64-debug`) and dropped Qt dependency
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
