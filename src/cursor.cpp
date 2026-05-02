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
    QColor orangishRed(227, 59, 36);
    m_typingModeCursorPen.setColor(Pen::kOrangishRed);
    m_typingModeCursorPen.setCosmetic(true);
    m_typingModeCursorPen.setWidthF(Pen::kTypingModeCursorWidth);

    // Darken brush setup
    m_darkenedBrush.setColor(Pen::kDarkenedColor);

    // Command mode cursor brush setup
    m_commandModeCursorBrush.setColor(Pen::kOrangishRed);

    // Setup master card stack
    Q_ASSERT(!m_yearToCardStack.contains(Master::kYear));
    auto* masterCS = new CardStack(Master::kYear, this);
    m_yearToCardStack.insert(Master::kYear, masterCS);
    Q_ASSERT(m_yearToCardStack.contains(Master::kYear));

    masterCS->add(CardItem::Type::Content, CardStack::ThreadMode::New);
    masterCS->lastCardItem()->firstRowItem()->setText("Help");
    masterCS->lastCardItem()->firstRowItem()->setReadOnly(true);

    showCard(masterCS->cardItemAt(0));
    m_keyboardMode = KeyboardMode::Command;
    m_row = 1;
    m_col = 0;

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
    m_keyboardMode = KeyboardMode::Typing;
}

void Cursor::enterCommandMode()
{
    m_keyboardMode = KeyboardMode::Command;
}

void Cursor::up()
{
    if (m_row != 0 && m_currentCard->isTOC())
        prevRow();
    else
    {
        if (m_row == 0)
            return; //  can't leave title via up
        if (m_row == 1 && m_currentCard->isThreadStart())
            return; // can't use up to go to TOC

        uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
        prevRow();
        uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
        m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
    }
}

void Cursor::down()
{
    if (m_row != 0 && m_currentCard->isTOC())
        nextRow();
    else
    {
        if (m_row == 0)
            return; // have to use enter to finish title
        uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
        nextRow();
        uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
        m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
    }
}

void Cursor::left()
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

void Cursor::right()
{
    if (m_row != 0 && m_currentCard->isTOC())
    {
        auto* toc = dynamic_cast<TOCItem*>(m_currentCard);
        Q_ASSERT(toc);
        if (toc->numberContent() > 0)
        {
            CardItem* newCard = toc->cardAtRow(m_row);
            Q_ASSERT(newCard);
            if (newCard->isContent())
                m_keyboardMode = KeyboardMode::Typing;
            else if(newCard->isTOC())
                m_keyboardMode = KeyboardMode::Command;
            showCard(newCard);
        }
    }
    else
    {
        if (m_row == 0 && m_col == m_currentCard->lastColAt(m_row))
            return; // can't leave whle working on title
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

void Cursor::enter()
{
    if (m_row == 0 || m_currentCard->isContent())
    {
        if (m_currentCard->readOnly())
            Q_ASSERT(false); // TODO: Add new content to m_year, connected to this thread
        else
        {
            if (m_row == 0)
                m_currentCard->firstRowItem()->setReadOnly(true);
            nextRowCreateCard();
            if (m_currentCard->isTOC())
                tocCurrent();
        }
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
            // TODO: Alert user you can't backspace past first col
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
    if (m_currentCard->isTOC())
    {
        if (m_row == m_currentCard->firstUserRow())
        {
            if (m_currentCard->isThreadStart())
                prevThreadCard();
            else
                return; // noop
        }
        else
            m_row--;
    }
    else if (m_currentCard->isContent())
    {
        if (m_row == m_currentCard->firstUserRow())
        {
            CardItem* oldCard = m_currentCard;
            prevThreadCard();
            if (m_currentCard->isTOC())
                tocCurrent();
            else if (oldCard != m_currentCard)
                m_row = m_currentCard->lastUserRow();
        }
        else
            m_row--;
    }
}

void Cursor::nextCard()
{
    if (m_currentCard->cardNumber() == lastCardNumber())
    {
        // Last card...Stop!
        // TODO: UI indicates at last card
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
        // TODO: UI indicates at last card
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
    CardItem* prevCard = m_currentCard->threadPrev();
    if (prevCard)
    {
        if (prevCard->isTOC())
            tocCurrent();
        showCard(prevCard);
    }
}

void Cursor::nextThreadCard()
{
    CardItem* nextCard = m_currentCard->threadNext();
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
        showCard(nextCard);
    else
    {
        Q_ASSERT(m_yearToCardStack.contains(m_year));
        m_yearToCardStack[m_year]->add(CardItem::Type::Content, CardStack::ThreadMode::Continue);
    }
}
void Cursor::newContent()
{
    Q_ASSERT(m_yearToCardStack.contains(m_year));
    m_yearToCardStack[m_year]->add(CardItem::Type::Content, CardStack::ThreadMode::New);
    m_keyboardMode = KeyboardMode::Typing;
}

void Cursor::newTOC()
{
    Q_ASSERT(m_yearToCardStack.contains(m_year));
    m_yearToCardStack[m_year]->add(CardItem::Type::TOC, CardStack::ThreadMode::New);
}

void Cursor::toggleDeleteCard()
{
    Q_ASSERT(m_currentCard);
    m_currentCard->setDeleted(!m_currentCard->deleted());
    scene()->invalidate(QRectF(), QGraphicsScene::ForegroundLayer);
}

void Cursor::draw(QPainter* painter, const QRectF& rect, bool capsDown)
{
    Q_ASSERT(m_currentCard);
    if (m_currentCard->deleted())
    {
        QRectF r_scn = m_currentCard->sceneBoundingRect();
        qreal inset_scn = Body::kRowHeight_scn;
        QPointF p1_scn = r_scn.topLeft()     + QPointF(inset_scn, inset_scn);
        QPointF p2_scn = r_scn.bottomRight() - QPointF(inset_scn, inset_scn);

        painter->setPen(m_deletedPen);
        painter->drawLine(p1_scn, p2_scn);
        return;
    }

    RowItem* rowItem = m_currentCard->rowItemAt(m_row);
    Q_ASSERT(rowItem);

    KeyboardMode tempMode = m_keyboardMode;
    if (capsDown)
        tempMode = KeyboardMode::Command;

    // Darken all but current row
    if (tempMode == KeyboardMode::Typing)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_darkenedBrush);
  

        qreal rowHeight_scn = rowItem->rowHeight_scn();
        qreal lineY_scn = m_currentCard->rowLineY_scn(m_row);

        qreal x = Card::kLeft_scn;
        qreal y = lineY_scn - rowHeight_scn;
        qreal w = Card::kRight_scn - Card::kLeft_scn;
        qreal h = rowHeight_scn;
        QRectF row_scn(x, y, w, h);

        QRectF card_lcl = m_currentCard->rect();
        QPolygonF card_scn = m_currentCard->mapRectToScene(card_lcl);

        // Build a path: outer rect minus inner rect
        QPainterPath path;
        path.addPolygon(card_scn);                                // outer
        path.addRoundedRect(row_scn, 5.0, 5.0, Qt::RelativeSize); // inner (will be subtracted)

        // Set fill rule so the inner area becomes a "hole"
        path.setFillRule(Qt::OddEvenFill); // or WindingFill; OddEven usually works best for holes
        painter->drawPath(path);           // draws only the outline with hole
    }

    // Draw cursor as red empty rounded square

    {
        painter->setPen(m_typingModeCursorPen);
        painter->setBrush(Qt::transparent);

        qreal rowHeight_scn = rowItem->rowHeight_scn();
        qreal charHeight_scn = rowItem->charHeight_scn();
        qreal charWidth_scn = rowItem->charWidth_scn();
        qreal lineY_scn = m_currentCard->rowLineY_scn(m_row);

        if (m_row != 0 && m_currentCard->isTOC())
        {
            auto* toc = dynamic_cast<TOCItem*>(m_currentCard);
            Q_ASSERT(toc);
            if (!toc->empty())
            {
                QPointF topLeft(Card::kBorder_scn,
                                lineY_scn - rowHeight_scn + (rowHeight_scn - charHeight_scn) / 2.0);
                QPointF bottomRight(Card::kRect_scn.width() - Card::kBorder_scn,
                                    lineY_scn - (rowHeight_scn - charHeight_scn) / 2.0);
                QRectF cursorRect(topLeft, bottomRight);
                qreal percentage = 0.25;
                painter->drawRoundedRect(cursorRect, percentage, percentage, Qt::RelativeSize);
            }
        }
        else if (tempMode == KeyboardMode::Typing) // hollow square
        {
            QPointF topLeft(m_col * charWidth_scn + Card::kBorder_scn,
                            lineY_scn - rowHeight_scn + (rowHeight_scn - charHeight_scn) / 2.0);
            QPointF bottomRight(topLeft.x() + charWidth_scn,
                                lineY_scn - (rowHeight_scn - charHeight_scn) / 2.0);
            QRectF cursorRect(topLeft, bottomRight);
            qreal percentage = 15.0;
            painter->drawRoundedRect(cursorRect, percentage, percentage, Qt::RelativeSize);
        }
        else // arrow pointing up under current character
        {
            painter->setBrush(m_commandModeCursorBrush);
            qreal deltaCharRow = rowHeight_scn - charHeight_scn;
            qreal centerX = m_col * charWidth_scn + Card::kBorder_scn + charWidth_scn / 2.0;
            qreal x1 = centerX;
            qreal y1 = lineY_scn - deltaCharRow / 2.0;
            qreal x2 = centerX - charWidth_scn / 2.0;
            qreal y2 = lineY_scn - deltaCharRow / 10.0;
            qreal x3 = centerX + charWidth_scn / 2.0;
            qreal y3 = lineY_scn - deltaCharRow / 10.0;

            QPointF points[3] = {QPointF(x1, y1),
                                 QPointF(x2, y2),
                                 QPointF(x3, y3)};
            painter->drawPolygon(points, 3);
        }
    }
}

void Cursor::showCard(CardItem* card)
{
    Q_ASSERT(m_currentCard);
    m_currentCard->hide();
    m_currentCard = card;
    m_currentCard->show();
}

void Cursor::tocCurrent()
{
    m_row = 1;
    m_col = 0;
    m_keyboardMode = KeyboardMode::Command;
}
