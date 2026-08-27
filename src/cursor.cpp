#include "cursor.h"
#include "canvas.h"
#include "hackAtlas.h"
#include "tocItem.h"

#include <cassert>

namespace
{
constexpr Pixel kCardColor = 0x00FDF9F0;       // Card::kColor "#fdf9f0"
constexpr Pixel kTitleLineColor = 0x00C9A1AE;  // Title::kLineColor
constexpr Pixel kBodyLineColor = 0x007D93EA;   // Body::kLineColor (alpha dropped -- was opaque anyway)
constexpr Pixel kBlack = 0x00000000;
constexpr Pixel kLightGray = 0x00A3A3A3;       // Colors::kLightGray
constexpr Pixel kOrangishRed = 0x00E33B24;     // Colors::kOrangishRed
constexpr Pixel kDeletedRed = 0x00FF0000;

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

Cursor::Cursor()
{
    m_year = Master::kYear;
    auto masterStack = std::make_unique<CardStack>(m_year);
    CardStack* masterCS = masterStack.get();
    m_yearToCardStack.emplace(m_year, std::move(masterStack));

    showCard(masterCS->tableOfContents());

    addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->setText(0, U"Help");

    m_row = 1;
    enterTypingMode();

    assert(m_currentCard);
}

CardNumber Cursor::lastCardNumber() const
{
    return m_yearToCardStack.at(m_year)->lastCardNumber();
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

void Cursor::enterTypingMode()
{
    if (m_currentCard->isTOC())
    {
        if (m_currentCard != m_currentCard->threadStart())
        {
            shakeCardNo();
            return;
        }
        m_row = 0; // move to title
    }

    if (m_currentCard->readOnly() || m_currentCard->deleted())
    {
        shakeCardNo();
        return;
    }
    m_keyboardMode = KeyboardMode::Typing;
    m_navigationMode = NavigationMode::Cursor; // so you can use navigation keys
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
        // Done with title
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

void Cursor::nextCard()
{
    if (m_currentCard->cardNumber() == lastCardNumber())
    {
        shakeCardNo(); // last card
    }
    else
    {
        CardStack* cardStack = m_yearToCardStack.at(m_year).get();
        CardNumber cardNum = m_currentCard->cardNumber();
        CardItem* next = cardStack->cardItemAt(cardNum + 1);
        if (next->isTOC())
            tocCurrent();
        showCard(next);
    }
}

void Cursor::prevCard()
{
    if (m_currentCard->cardNumber() == 0)
    {
        shakeCardNo(); // first card
    }
    else
    {
        CardStack* cardStack = m_yearToCardStack.at(m_year).get();
        CardNumber cardNumber = m_currentCard->cardNumber();
        CardItem* prev = cardStack->cardItemAt(cardNumber - 1);
        if (prev->isTOC())
            tocCurrent();
        showCard(prev);
    }
}

void Cursor::prevThreadCard()
{
    assert(m_currentCard);
    CardItem* prevCard = m_currentCard->threadPrev();

    while (prevCard && prevCard->deleted())
        prevCard = prevCard->threadPrev();

    if (prevCard && !prevCard->deleted())
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
    assert(m_currentCard);
    CardItem* nextCard = m_currentCard->threadNext();

    while (nextCard && nextCard->deleted())
        nextCard = nextCard->threadNext();

    if (nextCard && !nextCard->deleted())
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
    moveToTOCForNewCard();
    addCard(type, CardStack::ThreadMode::New);
    m_row = 0;
    m_col = 0;
    enterTypingMode();
}

void Cursor::addContinuationCard(CardItem::Type type)
{
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
        if (m_lastKeyKind == KeyEvent::Kind::CapsLock)
            enterCommandMode();
        else if (m_wasTypingMode)
            enterTypingMode();
        else
            enterCommandMode();
        return; // release doesn't update m_lastKeyKind, matching the old
                // code's keyReleaseEvent never touching m_lastKeyPress
    }

    m_lastKeyKind = event.kind;

    if (event.kind == KeyEvent::Kind::CapsLock) // press
    {
        m_capsDown = true;
        m_wasTypingMode = isTypingMode();
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
        switch (event.codepoint)
        {
            case U'i': up(); break;
            case U'k': down(); break;
            case U'j': left(); break;
            case U'l': right(); break;
            case U'e': enterTypingMode(); break;
            case U'u': prevCard(); break;
            case U'o': nextCard(); break;
            case U'd': toggleDeleteCard(); break;
            case U'c': addNewCard(CardItem::Type::Content); break;
            case U't': addNewCard(CardItem::Type::TOC); break;
            case U'n': toggleNavigationMode(); break;
            case U'm': prevThreadCard(); break;
            case U'.': nextThreadCard(); break;
            default: break; // unmapped command-mode key: noop
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
            drawBoxOutline(canvas, box, kOrangishRed, kCursorOutlineWidth_px);
        }
    }
    else if (tempMode == KeyboardMode::Typing) // hollow square
    {
        Rect box{marginX + m_col * cellW, rowTop, cellW, cellH};
        drawBoxOutline(canvas, box, kOrangishRed, kCursorOutlineWidth_px);
    }
    else // Command mode, cursor navigation: upward arrow under the current character
    {
        // Sized directly from the cell -- apex 3/4 down the cell, base at
        // the bottom (unlike drawCard's text, not vertically centered:
        // this is a UI indicator, not row content).
        int centerX = marginX + m_col * cellW + cellW / 2;
        int apexY = rowTop + cellH * 3 / 4;
        int baseY = rowTop + cellH;
        canvas.fillTriangle({centerX, apexY}, {centerX - cellW / 2, baseY}, {centerX + cellW / 2, baseY},
                             kOrangishRed);
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
