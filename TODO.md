# TODO

- [ ] Make CapsLock state return to normal while fj is running, but is no longer the active window
- [ ] +Shift to move to beginning/end
  - [ ] Shift+U : First card in stack
  - [ ] Shift+O : Last card in stack
  - [ ] Shift+M : First card in thread
  - [ ] Shift+. : Last card in thread
  - [ ] Shift+I : Master card stack TOC
  - [ ] Shift+K : Current card stack TOC
  - [ ] Backspace : Toggle previous card
- [ ] Deleting
  - [ ] Support deleting all cards except card 1 TOC and it's thread
  - [ ] When a card is deleted, skip over it when moving through thread
  - [ ] D toggles deletions of collection cards
- [ ] Title
  - [ ] First card in collection allows editing of title
  - [ ] First card of TOC allows editing of title, except card 1 TOC
  - [ ] When title is changed, start of thread updates TOC entry and next thread title
  - [ ] When a title is updated, pass change to next card in thread
  - [ ] Cannot change title if stack is not current
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
- [ ] Link navigation
  - [ ] Use G for Go when on a collection
  - [ ] Cursor should show up/down arrows and right arrow to remind how to navigate
  - [ ] Use up/down to search through links in form YEAR-CARDNUM
    - [ ] Also go to prev/next thread
    - [ ] On TOC, goes through thread entries and navigation thread
  - [ ] Press right arrow to go to link
  - [ ] When you follow a TOC entry, cursor should select prev thread
    - [ ] This means pressing right arrow twice toggles your position between TOC and TOC entry
- [ ] 0-9 as bookmarks
  - [ ] Shift+# to store
- [ ] Use S for Search
- [ ] Use / for Help
- [ ] Use x for TODO/DONE/No TODO

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
  - g           Go to a card link with up/down to select link, right to follow link
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
  - n
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
  - 5 row, 6 col per side (left and right hand)
  - Possible layout
    Left                Right
    ;     1 2 3 4 5     6 7 8 9 0 bs        (No =)
    tab   q w e r t     y u i o p -         (No [ and ] and \)
    caps  a s d f g     h j k l ' enter
    shift z x c v b     n m , . / shift
           spacebar     spacebar

