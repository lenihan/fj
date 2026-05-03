# PLAN

## DESIGN

- Glossary
  - Card - A 3" x 5" index card, with typing on one side
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

## TODO

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
  - [X] Support deleting all cards except card 1 TOC and it's thread
  - [X] When a card is deleted, skip over it when moving through thread
  - [X] D toggles deletions of collection cards
  - [ ] When a deleted card gets enter, add a new card to collection/TOC
- [ ] Title
  - [ ] First card in collection allows editing of title
  - [ ] First card of TOC allows editing of title, except card 1 TOC
  - [ ] When title is changed, start of thread updates TOC entry and next thread title
  - [ ] When a title is updated, pass change to next card in thread
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
- [ ] Ability to access characters not available from basic keyboard

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

