#include "cursor.h"
#include "cardItem.h"
#include "cardStack.h"
#include "common.h"
#include "rowItem.h"
#include "tocItem.h"
#include <QDate>
#include <QGraphicsScene>
#include <QMap>
#include <QPainter>
#include <QPen>

Cursor::Cursor(QGraphicsScene* scene) : m_scene(scene)
{
    Q_ASSERT(m_scene);

    // Deleted pen setup
    m_deletedPen.setColor(Qt::red);
    m_deletedPen.setWidthF(Pen::kDeletedWidth);
    m_deletedPen.setCosmetic(true);
    m_deletedPen.setCapStyle(Qt::RoundCap);

    // Typing mode cursor pen setup
    m_typingModeCursorPen.setColor(Colors::kOrangishRed);
    m_typingModeCursorPen.setCosmetic(true);
    m_typingModeCursorPen.setWidthF(Pen::kTypingModeCursorWidth);

    // Darken brush setup
    m_darkenedBrush.setStyle(Qt::SolidPattern);
    m_darkenedBrush.setColor(Colors::kDarkenedColor);

    // Command mode cursor brush setup
    m_commandModeCursorBrush.setColor(Colors::kOrangishRed);

    // Setup master card stack
    m_year = Master::kYear;
    Q_ASSERT(!m_yearToCardStack.contains(m_year));
    auto* masterCS = new CardStack(m_year, scene);
    m_yearToCardStack.insert(m_year, masterCS);
    Q_ASSERT(m_yearToCardStack.contains(m_year));
    showCard(masterCS->tableOfContents());

    addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("Help");
    addNewCard(CardItem::Type::TOC);
    CardItem* test = masterCS->lastCardItem();
    masterCS->lastCardItem()->firstRowItem()->setText("TEST TOC");
    addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("a");
    showCard(test);
   addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("b");
    showCard(test);
   addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("c");
    showCard(test);
   addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("d");
    showCard(test);
   addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("e");
    showCard(test);
   addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("f");
    showCard(test);
   addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("g");
    showCard(test);
   addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("h");
    showCard(test);
   addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("i");
    showCard(test);
   addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("j");
    showCard(test);




    #if 0

    showCard(masterCS->tableOfContents());
    addNewCard(CardItem::Type::TOC);
    masterCS->lastCardItem()->firstRowItem()->setText("TOC 1");
    showCard(masterCS->tableOfContents());
    addNewCard(CardItem::Type::TOC);
    masterCS->lastCardItem()->firstRowItem()->setText("TOC 2");
    showCard(masterCS->tableOfContents());
    addNewCard(CardItem::Type::TOC);
    masterCS->lastCardItem()->firstRowItem()->setText("TOC 3");
    showCard(masterCS->tableOfContents());
    addNewCard(CardItem::Type::TOC);
    masterCS->lastCardItem()->firstRowItem()->setText("TOC 4");
    showCard(masterCS->tableOfContents());
    addNewCard(CardItem::Type::TOC);
    masterCS->lastCardItem()->firstRowItem()->setText("TOC 5");
    showCard(masterCS->tableOfContents());
    addNewCard(CardItem::Type::TOC);
    masterCS->lastCardItem()->firstRowItem()->setText("TOC 6");
    showCard(masterCS->tableOfContents());
    addNewCard(CardItem::Type::TOC);
    masterCS->lastCardItem()->firstRowItem()->setText("TOC 7");
    showCard(masterCS->tableOfContents());
    addNewCard(CardItem::Type::Content);
    masterCS->lastCardItem()->firstRowItem()->setText("Content 1");
    showCard(masterCS->tableOfContents());
#endif

    m_row = 1;
    enterTypingMode();

#if 0   
    // Setup current year card stack
    const Year currentYear = QDate::currentDate().year();
    Q_ASSERT(!m_yearToCardStack.contains(currentYear));

    auto* yearCS = new CardStack(currentYear, this);
    m_yearToCardStack.insert(currentYear, yearCS);
    Q_ASSERT(m_yearToCardStack.contains(currentYear));
    /* these should be set in constructor
    
    // Init
    m_year = QDate::currentDate().year();
    m_currentCard = masterCS.toc();
    m_row = m_currentCard->firstEditableRow();
    m_col = 0;
    */
#endif

    Q_ASSERT(m_currentCard);
}

CardNumber Cursor::lastCardNumber() const
{
    Q_ASSERT(m_yearToCardStack.contains(m_year));
    const auto* cardStack = m_yearToCardStack[m_year];
    return cardStack->lastCardNumber();
}

QGraphicsScene* Cursor::scene()
{
    return m_scene;
}

Year Cursor::year() const
{
    return m_year;
}

void Cursor::setYear(Year year)
{
    m_year = year;
}

Row Cursor::row() const
{
    return m_row;
}

void Cursor::setRow(Row row)
{
    m_row = row;
}

Col Cursor::col() const
{
    return m_col;
}

void Cursor::setCol(Col col)
{
    m_col = col;
}

CardItem* Cursor::currentCard()
{
    return m_currentCard;
}

void Cursor::setCurrentCard(CardItem* card)
{
    m_currentCard = card;
}

bool Cursor::isTypingMode() const
{
    return m_keyboardMode == KeyboardMode::Typing;
}

bool Cursor::isCommandMode() const
{
    return m_keyboardMode == KeyboardMode::Command;
}

void Cursor::enterTypingMode()
{
    if (m_currentCard->isTOC())
    {
        if (m_currentCard != m_currentCard->threadStart())
        {
            shakeCardNo();
            return;
        }
        m_row = 0; // Move to title
    }

    if (m_currentCard->readOnly() || m_currentCard->deleted())
    {
        shakeCardNo();
        return;
    }
    m_keyboardMode = KeyboardMode::Typing;
    m_navigationMode = NavigationMode::Cursor; // so you can use navigation keys
}

void Cursor::enterCommandMode()
{
    m_keyboardMode = KeyboardMode::Command;
}

void Cursor::toggleNavigationMode()
{
    if (m_currentCard->isTOC())
    {
        m_navigationMode = m_row == 0
            ? NavigationMode::Cursor // Title can only do curor navigation
            : NavigationMode::Link; // TOC only has link navigation mode
    }
    else
        m_navigationMode = (m_navigationMode == NavigationMode::Link)
                               ? NavigationMode::Cursor
                               : NavigationMode::Link;
}

void Cursor::up()
{
    if (m_row == 0)
        return; //  can't leave title via up

    if (m_navigationMode == NavigationMode::Link)
    {
        if (m_currentCard->hasLinks())
            m_currentCard->prevLink();
    }
    else if (m_navigationMode == NavigationMode::Cursor)
    {
        uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
        prevRow();
        uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
        m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
    }
    else
        Q_ASSERT(false); // unknown navigation mode
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
            uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
            nextRow();
            uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
            m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
        }
    }
    else
        Q_ASSERT(false); // unknown navigation mode
}

void Cursor::left()
{
    if (m_navigationMode == NavigationMode::Link)
    {
        // go to back
        if (m_linkHistory.isEmpty())
            return; // noop
        else
        {
            CardItem* prevCard = m_linkHistory.takeLast();
            showCard(prevCard);
        }
    }
    else if (m_navigationMode == NavigationMode::Cursor)
    {
        if (m_row != 0 && m_currentCard->isTOC())
            ; // noop
        else
        {
            if (m_row == 0 && m_col == 0)
                return; // can't leave whle working on title
            bool nextLeftIsTOC = m_row == 1 &&
                                 m_col == m_currentCard->firstColAt(m_row) &&
                                 m_currentCard->isThreadStart();
            if (nextLeftIsTOC)
                return; // Can't leave content for TOC via arrow keys
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
            return; // can't leave whle working on title
        else
            m_col++;
    }
    else if (m_navigationMode == NavigationMode::Link)
    {
        // Follow current link
        if (m_currentCard->hasLinks())
        {
            CardItem::CardLink link = m_currentCard->currentLink();
            CardItem* targetCard = link.targetCard;
            Q_ASSERT(targetCard);
            if (targetCard == m_currentCard->threadPrev())
            {
                // Following prev thread, same as pressing back in link history
                // Remove last link from history
                if (!m_linkHistory.isEmpty())
                {
                    if (m_linkHistory.last() == targetCard)
                        m_linkHistory.removeLast();
                }
            }
            else
                m_linkHistory.append(m_currentCard);
            showCard(targetCard);
        }
    }
    else if (m_navigationMode == NavigationMode::Cursor)
    {
        if (m_currentCard->isTOC())
        {
            auto* toc = dynamic_cast<TOCItem*>(m_currentCard);
            Q_ASSERT(toc);
            if (toc->numberContent() > 0)
            {
                CardItem* newCard = toc->cardAtRow(m_row);

                // Skip over deleted cards
                CardItem* nextCard = newCard;
                while (nextCard && nextCard->deleted())
                    nextCard = nextCard->threadNext();
                if (nextCard && !nextCard->deleted())
                    newCard = nextCard;
                Q_ASSERT(newCard);

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
        {
            enterTypingMode();
        }
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
        // If we have a thread with every card deleted, 'enter' will add a new card to end of thread
        if (threadDeleted)
        {
            m_currentCard = last;
            addContinuationCard(CardItem::Type::Content);
        }
    }
    else if (m_currentCard->isContent())
    {
        if (m_currentCard->readOnly())
            Q_ASSERT(false); // TODO: Add new content to m_year, connected to this thread
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
        {
            // noop
            shakeCardNo();
        }
        else
        {
            // Delete prev character
            m_col--;
            m_currentCard->setChar(' ', m_row, m_col);
        }
    }
}

void Cursor::charTyped(QChar c)
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
                return; // on last row, last thread...noop
            else
                nextThreadCard(); // onlast row...go to next thread
        }
        else
        {
            auto* toc = dynamic_cast<TOCItem*>(m_currentCard);
            if (m_row >= toc->numberContent())
                ; // last row...noop
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
        {
            m_row++;
        }
    }
}

void Cursor::nextRowCreateCard()
{
    Q_ASSERT(m_row == 0 || m_currentCard->isContent());
    if (m_row == m_currentCard->lastUserRow())
        nextThreadCardCreateCard();
    else
        m_row++;
}

void Cursor::prevRow()
{
    Q_ASSERT(m_row != 0);
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
        // Last card...Stop!
        shakeCardNo();
    }
    else
    {
        Q_ASSERT(m_yearToCardStack.contains(m_year));
        auto* cardStack = m_yearToCardStack[m_year];
        CardNumber cardNum = m_currentCard->cardNumber();
        CardItem* nextCard = cardStack->cardItemAt(cardNum + 1);
        if (nextCard->isTOC())
            tocCurrent();
        showCard(nextCard);
    }
}

void Cursor::prevCard()
{
    if (m_currentCard->cardNumber() == 0)
    {
        // First card...Stop!
        shakeCardNo();
    }
    else
    {
        Q_ASSERT(m_yearToCardStack.contains(m_year));
        auto* cardStack = m_yearToCardStack[m_year];
        CardNumber cardNumber = m_currentCard->cardNumber();
        CardItem* prevCard = cardStack->cardItemAt(cardNumber - 1);
        if (prevCard->isTOC())
            tocCurrent();
        showCard(prevCard);
    }
}

void Cursor::prevThreadCard()
{
    Q_ASSERT(m_currentCard);
    CardItem* prevCard = m_currentCard->threadPrev();

    // Skip over deleted cards
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
    Q_ASSERT(m_currentCard);
    CardItem* nextCard = m_currentCard->threadNext();

    // Skip over deleted cards
    while (nextCard && nextCard->deleted())
    {
        nextCard = nextCard->threadNext();
    }

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
        showCard(nextCard);
    else
    {
        Q_ASSERT(m_yearToCardStack.contains(m_year));
        m_row = nextCard->firstUserRow();
        addContinuationCard(CardItem::Type::Content);
    }
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
    Q_ASSERT(m_yearToCardStack.contains(m_year));
    Q_ASSERT(m_currentCard);

    TOCItem* toc = dynamic_cast<TOCItem*>(m_currentCard->tableOfContents());
    Q_ASSERT(toc);
    while (toc->threadNext())
    {
        Q_ASSERT(toc->isFull());
        toc = dynamic_cast<TOCItem*>(toc->threadNext());
        Q_ASSERT(toc);
    }
    m_currentCard = toc;
    if (toc->isFull())
    {
        addContinuationCard(CardItem::Type::TOC);
    }
    Q_ASSERT(m_currentCard->isTOC());
    Q_ASSERT(m_currentCard->isThreadEnd());
}

void Cursor::toggleDeleteCard()
{
    Q_ASSERT(m_currentCard);
    m_currentCard->setDeleted(!m_currentCard->deleted());

    // Remove/add delete slash
    scene()->invalidate(QRectF(), QGraphicsScene::ForegroundLayer);
}

void Cursor::draw(QPainter* painter, const QRectF& rect, bool capsDown)
{
    Q_ASSERT(m_currentCard);

    // Draw delete slash if the card is deleted
    if (m_currentCard->deleted())
    {
        QRectF r_scen = m_currentCard->sceneBoundingRect();
        qreal inset_scen = Body::kRowHeight_scen;
        QPointF p1_scen = r_scen.topLeft() + QPointF(inset_scen, inset_scen);
        QPointF p2_scen = r_scen.bottomRight() - QPointF(inset_scen, inset_scen);

        painter->setPen(m_deletedPen);
        painter->drawLine(p1_scen, p2_scen);
        return;
    }

    RowItem* rowItem = m_currentCard->rowItemAt(m_row);
    Q_ASSERT(rowItem);

    KeyboardMode tempMode = m_keyboardMode;
    if (capsDown)
        tempMode = KeyboardMode::Command;

    // If typing, darken all but current row
    if (tempMode == KeyboardMode::Typing)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_darkenedBrush);

        qreal rowHeight_scen = rowItem->rowHeight_scen();
        qreal lineY_scen = m_currentCard->rowLineY_scen(m_row);

        qreal x = Card::kLeft_scen;
        qreal y = lineY_scen - rowHeight_scen;
        qreal w = Card::kRight_scen - Card::kLeft_scen;
        qreal h = rowHeight_scen;
        QRectF row_scen(x, y, w, h);

        QRectF card_locl = m_currentCard->rect();
        QPolygonF card_scen = m_currentCard->mapRectToScene(card_locl);

        // Build a path: outer rect minus inner rect
        QPainterPath path;
        path.addPolygon(card_scen);                                // outer
        path.addRoundedRect(row_scen, 5.0, 5.0, Qt::RelativeSize); // inner (will be subtracted)

        // Set fill rule so the inner area becomes a "hole"
        path.setFillRule(Qt::OddEvenFill); // or WindingFill; OddEven usually works best for holes
        painter->drawPath(path);           // draws only the outline with hole
    }

    // Draw cursor
    {
        painter->setPen(m_typingModeCursorPen);
        painter->setBrush(Qt::transparent);

        qreal rowHeight_scen = rowItem->rowHeight_scen();
        qreal charHeight_scen = rowItem->charHeight_scen();
        qreal charWidth_scen = rowItem->charWidth_scen();
        qreal lineY_scen = m_currentCard->rowLineY_scen(m_row);

        // Draw hollow square around links
        if (tempMode == KeyboardMode::Command && m_navigationMode == NavigationMode::Link)
        {
            if (m_currentCard->hasLinks())
            {
                CardItem::CardLink link = m_currentCard->currentLink();
                qreal linkLineY_scen = link.targetCard->rowLineY_scen(link.row);
                Q_ASSERT(link.targetCard);
                QPointF topLeft(link.col * charWidth_scen + Card::kBorder_scen,
                                linkLineY_scen - rowHeight_scen + (rowHeight_scen - charHeight_scen) / 2.0);
                QPointF bottomRight(topLeft.x() + link.charCount * charWidth_scen,
                                    linkLineY_scen - (rowHeight_scen - charHeight_scen) / 2.0);
                QRectF cursorRect(topLeft, bottomRight);
                qreal percentage = 15.0;
                painter->drawRoundedRect(cursorRect, percentage, percentage, Qt::RelativeSize);
            }
        }
        // Draw cursor in typing mode as a hollow square
        else if (tempMode == KeyboardMode::Typing) // hollow square
        {
            QPointF topLeft(m_col * charWidth_scen + Card::kBorder_scen,
                            lineY_scen - rowHeight_scen + (rowHeight_scen - charHeight_scen) / 2.0);
            QPointF bottomRight(topLeft.x() + charWidth_scen,
                                lineY_scen - (rowHeight_scen - charHeight_scen) / 2.0);
            QRectF cursorRect(topLeft, bottomRight);
            qreal percentage = 15.0;
            painter->drawRoundedRect(cursorRect, percentage, percentage, Qt::RelativeSize);
        }
        // Draw cursor in command mode as an arrow pointing up under the current character
        else
        {
            painter->setBrush(m_commandModeCursorBrush);
            qreal deltaCharRow = rowHeight_scen - charHeight_scen;
            qreal centerX = m_col * charWidth_scen + Card::kBorder_scen + charWidth_scen / 2.0;
            qreal x1 = centerX;
            qreal y1 = lineY_scen - deltaCharRow / 2.0;
            qreal x2 = centerX - charWidth_scen / 2.0;
            qreal y2 = lineY_scen - deltaCharRow / 10.0;
            qreal x3 = centerX + charWidth_scen / 2.0;
            qreal y3 = lineY_scen - deltaCharRow / 10.0;

            QPointF points[3] = {QPointF(x1, y1),
                                 QPointF(x2, y2),
                                 QPointF(x3, y3)};
            painter->drawPolygon(points, 3);
        }
    }
}

void Cursor::showCard(CardItem* card)
{
    Q_ASSERT(card);
    if (m_currentCard)
        m_currentCard->hide();
    card->show();
    m_currentCard = card;

    if (m_currentCard->isTOC())
        m_navigationMode = NavigationMode::Link;

    scene()->invalidate(QRectF(), QGraphicsScene::ForegroundLayer);
}

void Cursor::tocCurrent()
{
    m_row = 1;
    m_col = 0;
    enterCommandMode();
}

void Cursor::addCard(CardItem::Type type, CardStack::ThreadMode threadMode)
{
    Q_ASSERT(m_yearToCardStack.contains(m_year));
    m_linkHistory.append(m_currentCard);
    CardItem* newCard = m_yearToCardStack[m_year]->add(type, threadMode, m_currentCard);
    showCard(newCard);
}

void Cursor::shakeCardNo() const
{
    // TODO: make card shake left and right quickly like it is saying "no"
    //       to give user feedback they can't do something
}
