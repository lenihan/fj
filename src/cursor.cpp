#include "cursor.h"
#include "canvas.h"
#include "hackAtlas.h"
#include "tocItem.h"

#include <cassert>
#include <chrono>

namespace
{

// Fills a freshly-created help Content card's body (row 0, the title, is
// set separately -- a brand-new page has one of its own, a continuation
// page inherits its predecessor's automatically, see CardStack::add's
// ThreadMode::Continue branch) and locks it read-only. Every help page
// this is used for is fully written in one shot -- no case here ever
// fills a page without immediately locking it -- so combining both into
// one call rather than two separate ones removes a way to forget the
// second half.
void fillHelpPage(CardItem& card, std::initializer_list<std::u32string> lines)
{
    Row row = 1;
    for (const std::u32string& line : lines)
        card.setText(row++, line);
    card.setReadOnly(true);
}

constexpr Pixel kCardColor = 0x00FDF9F0;       // Card::kColor "#fdf9f0"
constexpr Pixel kTitleLineColor = 0x00C9A1AE;  // Title::kLineColor
constexpr Pixel kBodyLineColor = 0x007D93EA;   // Body::kLineColor (alpha dropped -- was opaque anyway)
constexpr Pixel kBlack = 0x00000000;
constexpr Pixel kLightGray = 0x00A3A3A3;       // Colors::kLightGray
constexpr Pixel kOrangishRed = 0x00E33B24;     // Colors::kOrangishRed -- typing mode's own cursor
constexpr Pixel kDeletedRed = 0x00FF0000;

// Bold/saturated, not the keyboard panel's own pale mode tints
// (keyboardPanel.cpp's kCommandColor/kNavigationColor) -- a thin outline
// or triangle needs real contrast against the cream card background
// (kCardColor) to read clearly, unlike a whole key face's flat fill.
constexpr Pixel kCommandGreen = 0x001E8E3E;   // general command mode's cursor
constexpr Pixel kNavigationBlue = 0x001A73E8; // Link/Navigation mode's cursor

// Cosmetic pen widths in the old Qt code (Pen::kDeletedWidth,
// kTypingModeCursorWidth) were screen-pixel-fixed regardless of view
// zoom -- the direct translation is a fixed pixel width, independent of
// which atlas is active or how the final present-time stretch scales it.
constexpr int kDeletedLineWidth_px = 10;
constexpr int kCursorOutlineWidth_px = 2;

void drawBoxOutline(Canvas& canvas, Rect r, Pixel color, int thickness)
{
    canvas.line({r.x, r.y}, {r.x + r.w, r.y}, color, thickness);
    canvas.line({r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, color, thickness);
    canvas.line({r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}, color, thickness);
    canvas.line({r.x, r.y + r.h}, {r.x, r.y}, color, thickness);
}

// Background, row separator lines, and every row's text. Old Qt code did
// this via CardItem::setupBackground()/setupLines() (once, at construction
// time, as child scene items) plus RowItem::paint() (once per row, called
// by the scene automatically). With no scene, it all happens here, drawn
// fresh from the card's current data every time Cursor::draw() runs.
//
// NOTE: doesn't implement the old "darken all but the current row while
// typing" effect -- that relied on Qt's alpha-blended brush
// (Colors::kDarkenedColor has alpha=50); Canvas::blendRect could do this
// now, but nothing wires it up yet. Deferred alongside the other visual
// polish (rounded corners, line caps).
void drawCard(const CardItem& card, Canvas& canvas, const HackAtlas::Atlas& atlas, const HackAtlas::Atlas& titleAtlas)
{
    int width = card.cardWidth_px(atlas);
    int height = card.cardHeight_px(atlas);
    canvas.fillRect({0, 0, width, height}, kCardColor);

    // Text is inset by sideMargin_px so the first/last character isn't
    // flush against the card's own edge; the separator lines below still
    // span the card's full width, like ruled paper.
    int marginX = card.sideMargin_px(atlas);

    for (Row row = 0; row < Card::kNumRows; ++row)
    {
        int top = card.rowTop_px(row, atlas);
        int cellH = card.cellHeight_px(row, atlas);
        const HackAtlas::Atlas& rowAtlas = row == 0 ? titleAtlas : atlas; // see cursor.h's draw comment

        if (row < Card::kNumRows - 1)
        {
            Pixel lineColor = row == 0 ? kTitleLineColor : kBodyLineColor;
            canvas.line({0, top + cellH}, {width, top + cellH}, lineColor, 1);
        }

        // Rows are generally taller than a glyph's own pixel height (see
        // CardItem::cellHeight_px) -- center the glyph within the row
        // rather than pinning it to the top.
        int glyphHeight = rowAtlas.cellHeight;
        int textY = top + (cellH - glyphHeight) / 2;

        Pixel textColor = card.rowReadOnly(row) ? kLightGray : kBlack;
        canvas.drawText(card.text(row), {marginX, textY}, textColor, rowAtlas);
    }
}

} // namespace

Year currentCalendarYear()
{
    // std::chrono's calendar API (C++20/23), not the old C localtime/tm
    // dance: std::chrono::year_month_day over a floor'd system_clock
    // reading is the modern way to pull a real calendar year out of the
    // system clock.
    auto today = std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())};
    return static_cast<Year>(static_cast<int>(today.year()));
}

Cursor::Cursor()
{
    setupInitialContent();

    assert(m_currentCard);
}

CardNumber Cursor::lastCardNumber() const
{
    // m_currentCard->year(), not m_year: this answers "how many cards are
    // in the stack I'm *looking at* right now" (nextCard()'s own "am I
    // already at the end" check, right below), which has to follow
    // wherever the cursor actually is -- Master, say -- independent of
    // m_year's own separate meaning (see its own comment: which stack
    // *new* content targets, deliberately not the same question).
    return m_yearToCardStack.at(m_currentCard->year())->lastCardNumber();
}

Year Cursor::year() const { return m_year; }
void Cursor::setYear(Year year)
{
    // Years only ever advance. addCard() always creates new content in
    // m_yearToCardStack.at(m_year) regardless of which card is currently
    // being viewed (see its own comment) -- m_year is the *only* thing
    // that decides where new content goes, so refusing to move it
    // backward here is what actually enforces "new content can only go
    // in the current year; you can't add cards to a previous year," not
    // just a convention nothing currently violates. A no-op, not an
    // assert: unlike this file's other "can't do that" cases (see
    // shakeCardNo()), this has to hold in a Release build too, not just
    // as a debug-only invariant.
    if (year < m_year)
        return;

    m_year = year;
    // Nothing has ever created a second year's stack until now (the real
    // app only ever runs with m_year == Master::kYear today -- there's no
    // year-rollover feature yet) -- lazily creating one here rather than
    // requiring some separate "start a new year" call makes switching to
    // a year just work, the same way a std::map's operator[] does.
    if (!m_yearToCardStack.contains(year))
        m_yearToCardStack.emplace(year, std::make_unique<CardStack>(year));
}

Row Cursor::row() const { return m_row; }
void Cursor::setRow(Row row) { m_row = row; }

Col Cursor::col() const { return m_col; }
void Cursor::setCol(Col col) { m_col = col; }

CardItem* Cursor::currentCard() { return m_currentCard; }
void Cursor::setCurrentCard(CardItem* card) { m_currentCard = card; }

bool Cursor::isTypingMode() const { return m_keyboardMode == KeyboardMode::Typing; }
bool Cursor::isCommandMode() const { return m_keyboardMode == KeyboardMode::Command; }
bool Cursor::isLinkMode() const { return m_navigationMode == NavigationMode::Link; }

void Cursor::enterTypingMode()
{
    // canEdit() bundles every reason this can be refused (read-only,
    // deleted, or a TOC continuation page) into the one predicate
    // CardItem itself owns -- checked before anything below touches
    // m_row, not after: a stack's own card-0 TOC (Master's, every year's)
    // is permanently read-only, and this used to only be checked *after*
    // the isTOC() branch below had already set m_row = 0 -- so pressing
    // 'e' on one silently left the cursor on the title row despite the
    // switch to typing mode being refused, corrupting later navigation
    // mode Cursor/Link decisions (toggleNavigationMode() also branches on
    // m_row == 0 for TOC cards). Found live, not by inspection: pressing
    // 'e' then 'n' on Master's own TOC looked like both keys had stopped
    // doing anything.
    if (!m_currentCard->canEdit())
    {
        shakeCardNo();
        return;
    }

    if (m_currentCard->isTOC())
        m_row = 0; // move to title -- canEdit() above already confirmed this is the thread-start TOC

    m_keyboardMode = KeyboardMode::Typing;
    m_navigationMode = NavigationMode::Cursor; // so you can use navigation keys

    if (m_capsDown)
    {
        // A chorded command ('c'/'t'/'e' -- see handleKey's dispatch)
        // just explicitly, intentionally moved us to typing mode *while*
        // cmd is still physically down, mid-gesture -- handleKey's own
        // CapsLock-release branch (still to come, once cmd actually lifts)
        // needs to know that happened, or its usual "revert to whatever
        // preceded this hold" bookkeeping (m_capsTapLatched/m_wasTypingMode,
        // both stale by definition here -- captured before this command
        // ever ran) will stomp straight back over it. Found live, not by
        // inspection: hold cmd, drag to 'c', release landed back in
        // command mode instead of typing mode on the new card.
        m_modeChangedDuringHold = true;
    }
    else
    {
        // A stale latch, not just irrelevant, if left set: 'e' reaches
        // typing mode without ever going through the CapsLock-release
        // branch that owns this flag, so leaving it true here meant the
        // *next* cmd tap still believed it was releasing an
        // already-latched command mode and immediately bounced straight
        // back to typing, instead of latching a fresh one -- found live,
        // not by inspection: cmd -> e -> cmd flashed to command mode and
        // back to typing in one tap. Only relevant outside a hold ('e'
        // pressed as its own separate tap, not chorded) -- see the
        // m_capsDown branch above for the chord's own version of this.
        m_capsTapLatched = false;
    }
}

void Cursor::enterCommandMode() { m_keyboardMode = KeyboardMode::Command; }

void Cursor::toggleNavigationMode()
{
    if (m_currentCard->isTOC())
    {
        m_navigationMode = m_row == 0
            ? NavigationMode::Cursor // title can only do cursor navigation
            : NavigationMode::Link;  // TOC only has link navigation
    }
    else
        m_navigationMode =
            (m_navigationMode == NavigationMode::Link) ? NavigationMode::Cursor : NavigationMode::Link;
}

void Cursor::up()
{
    if (m_row == 0)
        return; // can't leave title via up

    if (m_navigationMode == NavigationMode::Link)
    {
        if (m_currentCard->hasLinks())
            m_currentCard->prevLink();
    }
    else if (m_navigationMode == NavigationMode::Cursor)
    {
        // General command mode's arrows exist to position the cursor for
        // *editing* -- meaningless on a card you can't edit anyway (see
        // KeyDisabledState's own comment in keyboardPanel.h for why the
        // panel grays them out here). shakeCardNo() matches every other
        // canEdit()-gated refusal (enterTypingMode()'s own, most
        // directly).
        if (!m_currentCard->canEdit())
        {
            shakeCardNo();
            return;
        }
        ColCount oldColsPerRow = m_currentCard->colPerRow(m_row);
        prevRow();
        ColCount newColsPerRow = m_currentCard->colPerRow(m_row);
        m_col = static_cast<Col>(static_cast<unsigned>(m_col) * newColsPerRow / oldColsPerRow);
    }
    else
        assert(false); // unknown navigation mode
}

void Cursor::down()
{
    if (m_navigationMode == NavigationMode::Link)
    {
        if (m_currentCard->hasLinks())
            m_currentCard->nextLink();
    }
    else if (m_navigationMode == NavigationMode::Cursor)
    {
        if (!m_currentCard->canEdit()) // see up()'s own comment
        {
            shakeCardNo();
            return;
        }
        if (m_row != 0 && m_currentCard->isTOC())
            nextRow();
        else
        {
            ColCount oldColsPerRow = m_currentCard->colPerRow(m_row);
            nextRow();
            ColCount newColsPerRow = m_currentCard->colPerRow(m_row);
            m_col = static_cast<Col>(static_cast<unsigned>(m_col) * newColsPerRow / oldColsPerRow);
        }
    }
    else
        assert(false); // unknown navigation mode
}

void Cursor::left()
{
    if (m_navigationMode == NavigationMode::Link)
    {
        if (m_linkHistory.empty())
            return; // noop
        CardItem* prevCard = m_linkHistory.back();
        m_linkHistory.pop_back();
        showCard(prevCard);
    }
    else if (m_navigationMode == NavigationMode::Cursor)
    {
        if (!m_currentCard->canEdit()) // see up()'s own comment
        {
            shakeCardNo();
            return;
        }
        if (m_row != 0 && m_currentCard->isTOC())
            ; // noop
        else
        {
            if (m_row == 0 && m_col == 0)
                return; // can't leave while working on title
            bool nextLeftIsTOC = m_row == 1 && m_col == m_currentCard->firstColAt(m_row) &&
                                  m_currentCard->isThreadStart();
            if (nextLeftIsTOC)
                return; // can't leave content for TOC via arrow keys
            if (m_col == m_currentCard->firstColAt(m_row))
            {
                Row oldRow = m_row;
                prevRow();
                if (oldRow != m_row)
                    m_col = m_currentCard->lastColAt(m_row);
            }
            else
                m_col--;
        }
    }
}

void Cursor::right()
{
    if (m_row == 0)
    {
        if (m_col == m_currentCard->lastColAt(m_row))
            return; // can't leave while working on title
        m_col++;
    }
    else if (m_navigationMode == NavigationMode::Link)
    {
        if (m_currentCard->hasLinks())
        {
            CardItem::CardLink link = m_currentCard->currentLink();
            CardItem* targetCard = link.targetCard;
            assert(targetCard);

            // A link can point at a card that's since been deleted -- walk
            // forward through its own thread for a live card to land on
            // instead, the same way prevThreadCard()/nextThreadCard()
            // already do. If nothing live is left in that thread at all,
            // there's nothing to link to -- a no-op, not a fallback to
            // showing deleted content (the TOC's row-based right() below
            // does fall back that way; found via testing that doing the
            // same here still let a fully-deleted single-card thread stay
            // reachable, which defeats the point of skipping deleted cards
            // at all).
            CardItem* liveTarget = targetCard;
            while (liveTarget && liveTarget->deleted())
                liveTarget = liveTarget->threadNext();
            if (!liveTarget)
                return;
            targetCard = liveTarget;

            if (targetCard == m_currentCard->threadPrev())
            {
                if (!m_linkHistory.empty() && m_linkHistory.back() == targetCard)
                    m_linkHistory.pop_back();
            }
            else
                m_linkHistory.push_back(m_currentCard);
            showCard(targetCard);
        }
    }
    else if (m_navigationMode == NavigationMode::Cursor)
    {
        if (!m_currentCard->canEdit()) // see up()'s own comment
        {
            shakeCardNo();
            return;
        }
        if (m_currentCard->isTOC())
        {
            auto* toc = dynamic_cast<TOCItem*>(m_currentCard);
            assert(toc);
            if (toc->numberContent() > 0)
            {
                CardItem* newCard = toc->cardAtRow(m_row);

                CardItem* nextCard = newCard;
                while (nextCard && nextCard->deleted())
                    nextCard = nextCard->threadNext();
                if (nextCard && !nextCard->deleted())
                    newCard = nextCard;
                assert(newCard);

                showCard(newCard);
            }
        }
        else
        {
            if (m_col == m_currentCard->lastColAt(m_row))
            {
                Row oldRow = m_row;
                nextRow();
                if (oldRow != m_row)
                    m_col = 0;
            }
            else
                m_col++;
        }
    }
}

void Cursor::enter()
{
    if (m_row == 0)
    {
        // Done with title. Every continuation's own title row is a
        // read-only copy made once at creation time (see CardStack::add's
        // ThreadMode::Continue branch) -- only threadStart()'s title row
        // is ever actually editable, so this is the one place an edit can
        // happen and the only place it needs to propagate from. The TOC's
        // own reference to this title needs no propagation at all: it
        // reads text(0) fresh every time it's drawn (see TOCItem::text()).
        if (m_currentCard->isThreadStart())
        {
            std::u32string title = m_currentCard->text(0);
            for (CardItem* card = m_currentCard->threadNext(); card; card = card->threadNext())
                card->setText(0, title);
        }

        m_row++;
        if (m_currentCard->isContent())
            enterTypingMode();
        else if (m_currentCard->isTOC())
        {
            enterCommandMode();
            m_navigationMode = NavigationMode::Link;
        }
    }
    else if (m_keyboardMode == KeyboardMode::Command)
    {
        shakeCardNo();
        return;
    }
    else if (m_currentCard->deleted() && m_currentCard->isContent())
    {
        bool threadDeleted = true;
        CardItem* thread = m_currentCard->threadStart();
        CardItem* last = nullptr;
        while (thread)
        {
            last = thread;
            if (!thread->deleted())
            {
                threadDeleted = false;
                break;
            }
            thread = thread->threadNext();
        }
        // If we have a thread with every card deleted, 'enter' adds a new
        // card to the end of the thread.
        if (threadDeleted)
        {
            m_currentCard = last;
            addContinuationCard(CardItem::Type::Content);
        }
    }
    else if (m_currentCard->isContent())
    {
        if (m_currentCard->readOnly())
            assert(false); // TODO: add new content to m_year, connected to this thread
        else
            nextRowCreateCard();
    }
    m_col = 0;
}

void Cursor::backspace()
{
    if (m_row != 0 && m_currentCard->isTOC())
        ; // noop
    else
    {
        if (m_col == m_currentCard->firstColAt(m_row))
            shakeCardNo();
        else
        {
            m_col--;
            m_currentCard->setChar(U' ', m_row, m_col);
        }
    }
}

void Cursor::charTyped(char32_t c)
{
    if (m_currentCard->deleted() || m_currentCard->readOnly())
    {
        shakeCardNo();
        return;
    }

    if (m_row == 0 || m_currentCard->isContent())
    {
        m_currentCard->setChar(c, m_row, m_col);
        right();
    }
}

void Cursor::nextRow()
{
    if (m_currentCard->isTOC())
    {
        if (m_row == m_currentCard->lastUserRow())
        {
            if (m_currentCard->threadNext() == nullptr)
                return; // last row, last thread -- noop
            nextThreadCard();
        }
        else
        {
            auto* toc = dynamic_cast<TOCItem*>(m_currentCard);
            if (m_row >= toc->numberContent())
                ; // last content row -- noop
            else
                m_row++;
        }
    }
    else if (m_currentCard->isContent())
    {
        if (m_row == m_currentCard->lastUserRow())
        {
            CardItem* oldCard = m_currentCard;
            nextThreadCard();
            if (oldCard != m_currentCard)
                m_row = m_currentCard->firstUserRow();
        }
        else
            m_row++;
    }
}

void Cursor::nextRowCreateCard()
{
    assert(m_row == 0 || m_currentCard->isContent());
    if (m_row == m_currentCard->lastUserRow())
        nextThreadCardCreateCard();
    else
        m_row++;
}

void Cursor::prevRow()
{
    assert(m_row != 0);
    if (m_row == m_currentCard->firstUserRow() && !m_currentCard->isThreadStart())
    {
        CardItem* oldCard = m_currentCard;
        prevThreadCard();
        if (oldCard != m_currentCard)
            m_row = m_currentCard->lastUserRow();
    }
    else
        m_row--;
}

bool Cursor::isAtFirstCard() const { return m_currentCard->cardNumber() == 0; }
bool Cursor::isAtLastCard() const { return m_currentCard->cardNumber() == lastCardNumber(); }

void Cursor::nextCard()
{
    if (isAtLastCard())
    {
        shakeCardNo(); // last card
    }
    else
    {
        // m_currentCard->year(), matching lastCardNumber()'s own comment
        // -- pages through whichever stack is actually being viewed.
        CardStack* cardStack = m_yearToCardStack.at(m_currentCard->year()).get();
        CardNumber cardNum = m_currentCard->cardNumber();
        CardItem* next = cardStack->cardItemAt(cardNum + 1);
        if (next->isTOC())
            tocCurrent();
        // u/o page strictly by card number -- landing on a TOC this way
        // isn't "following a link into it" (which does want Navigation
        // mode -- see right()'s own showCard() calls), so it shouldn't
        // silently bump the user into a different command sub-state than
        // the one they were already in. showCard() itself always forces
        // Navigation mode for any TOC target; save/restore around it
        // rather than touching showCard() itself, since every other
        // caller's own TOC landing *does* want that.
        NavigationMode modeBefore = m_navigationMode;
        showCard(next);
        m_navigationMode = modeBefore;
    }
}

void Cursor::prevCard()
{
    if (isAtFirstCard())
    {
        shakeCardNo(); // first card
    }
    else
    {
        CardStack* cardStack = m_yearToCardStack.at(m_currentCard->year()).get();
        CardNumber cardNumber = m_currentCard->cardNumber();
        CardItem* prev = cardStack->cardItemAt(cardNumber - 1);
        if (prev->isTOC())
            tocCurrent();
        NavigationMode modeBefore = m_navigationMode; // see nextCard()'s own comment
        showCard(prev);
        m_navigationMode = modeBefore;
    }
}

CardItem* Cursor::findLivePrevThreadCard() const
{
    assert(m_currentCard);
    CardItem* prevCard = m_currentCard->threadPrev();
    while (prevCard && prevCard->deleted())
        prevCard = prevCard->threadPrev();
    return prevCard;
}

CardItem* Cursor::findLiveNextThreadCard() const
{
    assert(m_currentCard);
    CardItem* nextCard = m_currentCard->threadNext();
    while (nextCard && nextCard->deleted())
        nextCard = nextCard->threadNext();
    return nextCard;
}

void Cursor::prevThreadCard()
{
    CardItem* prevCard = findLivePrevThreadCard();
    if (prevCard)
    {
        if (prevCard->isTOC())
        {
            auto* toc = dynamic_cast<TOCItem*>(prevCard);
            tocCurrent();
            m_row = toc->rowAtCard(m_currentCard);
        }
        showCard(prevCard);
    }
}

void Cursor::nextThreadCard()
{
    CardItem* nextCard = findLiveNextThreadCard();
    if (nextCard)
    {
        if (nextCard->isTOC())
            tocCurrent();
        showCard(nextCard);
    }
}

void Cursor::nextThreadCardCreateCard()
{
    CardItem* nextCard = m_currentCard->threadNext();
    if (nextCard)
    {
        showCard(nextCard);
        return;
    }

    // Old code dereferenced `nextCard` here even though this branch is
    // only reached when it's null (harmless in practice only because
    // firstUserRow() never touches `this`, but still a real null-deref
    // bug). Fixed: create the continuation card first, then ask *it* for
    // its first user row.
    addContinuationCard(CardItem::Type::Content);
    m_row = m_currentCard->firstUserRow();
}

void Cursor::addNewCard(CardItem::Type type)
{
    // A read-only stack (Master, once setupInitialContent() finishes --
    // see its own comment) can't grow new content at all, not just
    // reject edits to what's already there.
    if (m_yearToCardStack.at(m_year)->readOnly())
    {
        shakeCardNo();
        return;
    }
    moveToTOCForNewCard();
    addCard(type, CardStack::ThreadMode::New);
    m_row = 0;
    m_col = 0;
    enterTypingMode();
}

void Cursor::addContinuationCard(CardItem::Type type)
{
    if (m_yearToCardStack.at(m_year)->readOnly())
    {
        shakeCardNo();
        return;
    }
    addCard(type, CardStack::ThreadMode::Continue);
}

void Cursor::moveToTOCForNewCard()
{
    assert(m_currentCard);

    auto* toc = dynamic_cast<TOCItem*>(m_currentCard->tableOfContents());
    assert(toc);
    while (toc->threadNext())
    {
        assert(toc->isFull());
        toc = dynamic_cast<TOCItem*>(toc->threadNext());
        assert(toc);
    }
    m_currentCard = toc;
    if (toc->isFull())
        addContinuationCard(CardItem::Type::TOC);

    assert(m_currentCard->isTOC());
    assert(m_currentCard->isThreadEnd());
}

void Cursor::toggleDeleteCard()
{
    assert(m_currentCard);
    m_currentCard->setDeleted(!m_currentCard->deleted());
}

void Cursor::handleKey(const KeyEvent& event)
{
    if (event.kind == KeyEvent::Kind::CapsLock && !event.pressed)
    {
        m_capsDown = false;
        if (m_modeChangedDuringHold)
        {
            // A chorded command already explicitly set the mode itself
            // (see enterTypingMode()'s own comment) -- trust it
            // completely and skip the tap/hold bookkeeping below
            // entirely, which only knows about state from *before*
            // whatever ran during this hold.
            m_modeChangedDuringHold = false;
            return;
        }
        if (m_lastKeyKind == KeyEvent::Kind::CapsLock)
        {
            // A plain tap: pressed and released with nothing typed while
            // held (m_lastKeyKind is still CapsLock from the press itself,
            // not from some intervening key). The first one latches
            // command mode on and stays -- mirroring a real Caps Lock's
            // own latch, and matching the original design here (a single
            // tap alone was always meant to enter command mode). A
            // *second* plain tap in a row releases that latch back to
            // typing mode -- found missing during the ortholinear keyboard
            // panel's testing (see PLAN.md): without m_capsTapLatched,
            // this branch unconditionally re-entered command mode every
            // time, so a tap-to-toggle gesture (mouse or a real keyboard's
            // own "hold caps a moment with nothing typed, let go") could
            // latch on but never tap back off.
            //
            // A tap always steps up exactly one level, never two: from
            // Link (Navigation) mode, it steps up to general command mode
            // only, leaving the latch on -- a *second* tap from there is
            // what actually reaches typing. Link mode used to let 'n'/'e'
            // jump out on their own (straight back to general command, or
            // straight to typing); the user asked for cmd to be the only
            // way out, one step at a time, so this tap is now the one
            // place that transition happens.
            if (m_capsTapLatched)
            {
                if (m_navigationMode == NavigationMode::Link)
                    m_navigationMode = NavigationMode::Cursor;
                else
                {
                    m_capsTapLatched = false;
                    enterTypingMode();
                }
            }
            else
            {
                m_capsTapLatched = true;
                enterCommandMode();
            }
        }
        else if (m_capsTapLatched)
        {
            // Something was typed while held (a hold-tap-release chord),
            // but command mode is already latched from an earlier plain
            // tap -- stay latched regardless of m_wasTypingMode, the same
            // "state returns to prev" invariant the chord case below
            // follows, just with "prev" being the latch rather than
            // whatever mode preceded this particular press.
            enterCommandMode();
        }
        else if (m_wasTypingMode)
        {
            enterTypingMode();
        }
        else
        {
            enterCommandMode();
        }
        return; // release doesn't update m_lastKeyKind, matching the old
                // code's keyReleaseEvent never touching m_lastKeyPress
    }

    m_lastKeyKind = event.kind;

    if (event.kind == KeyEvent::Kind::CapsLock) // press
    {
        m_capsDown = true;
        m_wasTypingMode = isTypingMode();
        m_modeChangedDuringHold = false; // a fresh hold starting -- see its own comment
        enterCommandMode();
        return;
    }
    if (event.kind == KeyEvent::Kind::Enter)
    {
        enter();
        return;
    }
    if (event.kind == KeyEvent::Kind::Backspace)
    {
        backspace();
        return;
    }

    // event.kind == Char: home-row command dispatch, or literal typing.
    if (isCommandMode() || m_capsDown)
    {
        // Navigation mode (m_navigationMode == Link, entered via 'n') is
        // an exclusive mode: only i/k/j/l, which actually mean something
        // different there, stay live -- an allow-list, not a deny-list,
        // since 'n'/'e' are no longer permanent exceptions either. They
        // used to bail straight back to general command / typing on
        // their own; the user asked for the cmd key to be the only way
        // out, one step at a time (see handleKey's CapsLock-release
        // branch), so from here they're ordinary blocked keys like
        // u/o/d/c/t/m/. always were. shakeCardNo() gives the same "can't
        // do that" feedback prevCard()/nextCard() etc. already use
        // elsewhere.
        bool blockedInLinkMode = isLinkMode() &&
            event.codepoint != U'i' && event.codepoint != U'k' && event.codepoint != U'j' &&
            event.codepoint != U'l';
        if (blockedInLinkMode)
        {
            shakeCardNo();
        }
        else
        {
            switch (event.codepoint)
            {
                case U'i': up(); break;
                case U'k': down(); break;
                case U'j': left(); break;
                case U'l': right(); break;
                // a/s/q/w/e (edit/nav/+card/+toc/del): keyboardPanel.cpp's
                // own comment on kLeftKeys explains why these five sit
                // here and not on e/n/c/t/d, their old spots -- a key's
                // command dispatches on the exact codepoint typing mode
                // sends, so there's no way to relocate a *function*
                // without relocating whichever letter used to carry its
                // codepoint too; this is that relocation, done once here
                // instead of in the keyboard panel's own layout table (so
                // every letter can stay at its real-QWERTY position).
                case U'a': enterTypingMode(); break;
                case U'u': prevCard(); break;
                case U'o': nextCard(); break;
                case U'e': toggleDeleteCard(); break;
                case U'q': addNewCard(CardItem::Type::Content); break;
                case U'w': addNewCard(CardItem::Type::TOC); break;
                case U's': toggleNavigationMode(); break;
                case U'm': prevThreadCard(); break;
                case U'.': nextThreadCard(); break;
                default: break; // unmapped command-mode key: noop
            }
        }
    }
    else
        charTyped(event.codepoint);
}

void Cursor::draw(Canvas& canvas, const HackAtlas::Atlas& atlas, const HackAtlas::Atlas& titleAtlas) const
{
    assert(m_currentCard);

    drawCard(*m_currentCard, canvas, atlas, titleAtlas);

    if (m_currentCard->deleted())
    {
        int inset = m_currentCard->cellHeight_px(1, atlas); // one body-row-height inset
        Point p1{inset, inset};
        Point p2{m_currentCard->cardWidth_px(atlas) - inset, m_currentCard->cardHeight_px(atlas) - inset};
        canvas.line(p1, p2, kDeletedRed, kDeletedLineWidth_px);
        return;
    }

    KeyboardMode tempMode = m_capsDown ? KeyboardMode::Command : m_keyboardMode;
    int marginX = m_currentCard->sideMargin_px(atlas); // drawCard insets text by this too -- keep cursor indicators aligned with it
    int rowTop = m_currentCard->rowTop_px(m_row, atlas);
    int cellW = m_currentCard->cellWidth_px(m_row, atlas);
    int cellH = m_currentCard->cellHeight_px(m_row, atlas);

    if (tempMode == KeyboardMode::Command && m_navigationMode == NavigationMode::Link)
    {
        if (m_currentCard->hasLinks())
        {
            CardItem::CardLink link = m_currentCard->currentLink();
            int linkTop = m_currentCard->rowTop_px(link.row, atlas);
            int linkCellW = m_currentCard->cellWidth_px(link.row, atlas);
            int linkCellH = m_currentCard->cellHeight_px(link.row, atlas);
            Rect box{marginX + link.col * linkCellW, linkTop, link.charCount * linkCellW, linkCellH};
            drawBoxOutline(canvas, box, kNavigationBlue, kCursorOutlineWidth_px);
        }
    }
    else if (tempMode == KeyboardMode::Typing) // hollow square
    {
        Rect box{marginX + m_col * cellW, rowTop, cellW, cellH};
        drawBoxOutline(canvas, box, kOrangishRed, kCursorOutlineWidth_px);
    }
    else if (m_currentCard->canEdit()) // Command mode, cursor navigation: upward arrow under the current character
    {
        // Nothing drawn at all when the card is read-only (most of
        // Master's, most days) -- the arrows this cursor exists to
        // position for editing are themselves refused there (see
        // up()/down()/left()/right()'s own canEdit() gate), so showing it
        // would suggest an action that can't actually happen. Navigation
        // mode's own cursor (the isLinkMode branch above) is unaffected
        // -- browsing a read-only TOC's links is exactly how it's meant
        // to be used.
        //
        // Sized directly from the cell -- apex 3/4 down the cell, base at
        // the bottom (unlike drawCard's text, not vertically centered:
        // this is a UI indicator, not row content).
        int centerX = marginX + m_col * cellW + cellW / 2;
        int apexY = rowTop + cellH * 3 / 4;
        int baseY = rowTop + cellH;
        canvas.fillTriangle({centerX, apexY}, {centerX - cellW / 2, baseY}, {centerX + cellW / 2, baseY},
                             kCommandGreen);
    }
}

void Cursor::showCard(CardItem* card)
{
    assert(card);
    m_currentCard = card;

    if (m_currentCard->isTOC())
        m_navigationMode = NavigationMode::Link;
}

void Cursor::tocCurrent()
{
    m_row = 1;
    m_col = 0;
    enterCommandMode();
}

void Cursor::addCard(CardItem::Type type, CardStack::ThreadMode threadMode)
{
    m_linkHistory.push_back(m_currentCard);
    CardItem* newCard = m_yearToCardStack.at(m_year)->add(type, threadMode, m_currentCard);
    showCard(newCard);
}

void Cursor::shakeCardNo() const
{
    // TODO: make the card shake left/right quickly like it's saying "no",
    // to give the user feedback they can't do something.
}

void Cursor::setupInitialContent()
{
    // Master's own stack -- everything starts here.
    m_year = Master::kYear;
    auto masterStack = std::make_unique<CardStack>(m_year);
    CardStack* masterCS = masterStack.get();
    m_yearToCardStack.emplace(m_year, std::move(masterStack));

    TOCItem* masterToc = masterCS->tableOfContents();
    showCard(masterToc);

    // The current year's own stack -- empty, linking back to Master.
    //
    // Deliberately not Cursor::setYear(currentYear) here: that also
    // refuses to move backward (see its own comment), which nothing
    // needs at startup, and every addNewCard()/addContinuationCard()
    // call below (building Help's content) still needs m_year to be
    // Master::kYear -- it's reassigned to currentYear directly, once,
    // after all of Master's own content is built (see below).
    Year currentYear = currentCalendarYear();

    auto yearStack = std::make_unique<CardStack>(currentYear);
    TOCItem* yearToc = yearStack->tableOfContents();
    yearToc->setThreadPrev(masterToc); // the "back to Master" up-arrow
    m_yearToCardStack.emplace(currentYear, std::move(yearStack));
    masterToc->addToTOC(yearToc);

    // A nested Help TOC, listing individual help topics -- exactly the
    // same "add a new TOC card" path 't' already exercises, just
    // invoked directly instead of via a keypress, so it's automatically
    // listed in Master's TOC with its own up-arrow back to Master.
    addNewCard(CardItem::Type::TOC);
    CardItem* helpToc = m_currentCard;
    helpToc->setText(0, U"Help");
    helpToc->setReadOnly(true);

    showCard(helpToc);
    addNewCard(CardItem::Type::Content);
    CardItem* firstTopic = m_currentCard;
    firstTopic->setText(0, U"What is fj");
    fillHelpPage(*m_currentCard,
                 {
                     U"fj is a keyboard-only note system -- no mouse, no menus.",
                     U"Every action is a key on the home row.",
                     U"",
                     U"Notes live on cards, like an index-card box. A card can",
                     U"link to other cards, forming a thread (a running note",
                     U"spanning several cards), or get listed in a table of",
                     U"contents (TOC) alongside other cards.",
                     U"",
                     U"This card and the rest of Help are read-only.",
                 });

    showCard(helpToc);
    addNewCard(CardItem::Type::Content);
    m_currentCard->setText(0, U"Modes");
    fillHelpPage(*m_currentCard, {
                                     U"fj has three modes: typing, command, and Navigation.",
                                     U"",
                                     U"Typing mode is for writing -- every key types its own",
                                     U"letter.",
                                     U"",
                                     U"Command mode (tap cmd) turns the keyboard into",
                                     U"commands instead: i/k/j/l move around, e returns to",
                                     U"typing.",
                                 });
    addContinuationCard(CardItem::Type::Content);
    fillHelpPage(*m_currentCard, {
                                     U"Navigation mode (from command mode, tap nav) narrows",
                                     U"i/k/j/l to just following links: i/k pick a link,",
                                     U"j goes back, l follows it.",
                                     U"",
                                     U"Tap cmd again to step back up one level at a time:",
                                     U"Navigation -> command -> typing.",
                                 });

    showCard(helpToc);
    addNewCard(CardItem::Type::Content);
    m_currentCard->setText(0, U"Keys");
    fillHelpPage(*m_currentCard,
                 {
                     U"i/k/j/l move -- up/down/left/right in command mode,",
                     U"prev/next/back/go for links in Navigation mode.",
                     U"",
                     U"e enters typing mode. cmd enters command mode, and",
                     U"steps back up one level each tap. nav enters",
                     U"Navigation mode.",
                     U"u/o move to the previous/next card by number.",
                     U"c/t add a new card / a new TOC entry. d deletes it.",
                     U"m/. jump to the previous/next card in this thread.",
                 });

    showCard(helpToc);
    addNewCard(CardItem::Type::Content);
    m_currentCard->setText(0, U"Finding your way");
    fillHelpPage(*m_currentCard,
                 {
                     U"A TOC lists cards -- each row links to one. From a",
                     U"TOC, i/k pick a row, l follows it.",
                     U"",
                     U"Every card also carries its own prev/next thread",
                     U"links along its bottom row, for stepping through a",
                     U"thread without returning to the TOC each time.",
                     U"",
                     U"However deep you go, following prev links (or a",
                     U"TOC's own back-link) eventually leads back to Master.",
                 });

    // TOCItem::addToTOC() unconditionally resets the current-link index
    // to 0 every time it's called (see tocItem.cpp), regardless of
    // setupLinks()'s own more careful "skip the up-arrow if there is one"
    // logic -- harmless for Master's own TOC (no up-arrow there, so index
    // 0 already is the first real entry) but not for Help's, which
    // ends up with its up-arrow back to Master sitting at index 0.
    // Without this, landing on Help and immediately pressing 'l' would
    // silently bounce straight back to Master instead of opening its
    // first topic -- found live, not by inspection.
    helpToc->setCurrentLink(firstTopic);

    // Nothing can be added to Master from here on -- see
    // addNewCard()/addContinuationCard()'s own read-only gate.
    masterCS->setReadOnly(true);

    // m_year now switches to meaning what it always should have going
    // forward: which stack *new* content targets (see its own comment),
    // not Master -- every addNewCard()/addContinuationCard() call above
    // needed it to still be Master::kYear while building Help's content
    // there, but real user interaction starts right after this
    // constructor returns, and until now nothing ever moved it off
    // Master::kYear again. That's exactly what made 'c'/'t' silently do
    // nothing everywhere, not just on Master (addCard() always allocates
    // into m_yearToCardStack.at(m_year), and Master's stack is
    // permanently read-only) -- found live, not by inspection.
    m_year = currentYear;

    // Every addCard() call above (each addNewCard()/addContinuationCard()
    // building Help's content) pushed its then-current card onto
    // m_linkHistory -- an internal side effect of setup, not real user
    // navigation. Left alone, hasLinkHistory() would read true the
    // instant the app opens, and the keyboard panel's "No history"
    // disabled styling for 'j' (back) would never show on a fresh
    // launch, and worse, an early 'j' would pop through setup's own
    // leftover trail of Help pages instead of correctly doing nothing --
    // found via the "hasLinkHistory()/linkCount()" test below, not live.
    m_linkHistory.clear();

    showCard(masterToc);
    // Off the title row (0), not wherever the last addNewCard() call
    // above happened to leave it -- right()'s own dispatch checks
    // m_row == 0 *before* it even looks at navigation mode (see its own
    // title-editing special case), so leaving row 0 here would make 'l'
    // just move within the (empty, read-only) title text instead of
    // following Master's own current link -- found live, not by
    // inspection: landing on Master and immediately pressing 'l' did
    // nothing at all.
    m_row = 1;
    m_col = 0;
    enterCommandMode();

    // enterCommandMode() here is a direct call, not a real CapsLock tap
    // -- handleKey()'s own tap-latch tracking (m_capsTapLatched) has no
    // way to know we're landing in command mode "as if" a tap just
    // latched it, so without this, the very first real tap after launch
    // would fall into the "establish the latch, don't act yet" branch
    // instead of actually stepping anywhere -- the same class of
    // staleness bug enterTypingMode() already guards against for the
    // opposite direction. Set true, not false: we ARE starting latched.
    m_capsTapLatched = true;
}
