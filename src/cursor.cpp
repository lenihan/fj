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

    // Setup master card stack
    Q_ASSERT(!m_yearToCardStack.contains(Master::kYear));
    auto* masterCS = new CardStack(Master::kYear, this);
    m_yearToCardStack.insert(Master::kYear, masterCS);
    Q_ASSERT(m_yearToCardStack.contains(Master::kYear));
    /* TODO: These should be set in constructor
    m_currentCard = masterCS->tableOfContents();
    Q_ASSERT(m_currentCard->cardNumber() == 0);
    m_year = Master::kYear;
    m_row = m_currentCard->firstEditableRow();
    m_col = 0;
    */

    // masterCS->add(CardItem::Type::Content, CardStack::ThreadMode::New);
    // masterCS->lastCardItem()->firstRowItem()->setText("Help");

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

bool Cursor::actionMode() const
{
    return m_actionMode;
}

void Cursor::setActionMode(bool actionMode)
{
    m_actionMode = actionMode;
}

void Cursor::up()
{
    if (m_currentCard->isTOC())
        prevRow();
    else if (m_currentCard->isContent())
    {
        uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
        prevRow();
        uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
        m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
    }
}

void Cursor::down()
{
    if (m_currentCard->isTOC())
        nextRow();
    else if (m_currentCard->isContent())
    {
        uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
        nextRow();
        uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
        m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
    }
}

void Cursor::left()
{
    if (m_currentCard->isTOC())
        ; // noop
    else if (m_currentCard->isContent())
    {
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
    if (m_currentCard->isTOC())
        ; // noop
    else if (m_currentCard->isContent())
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

void Cursor::enter()
{
    if (m_currentCard->isTOC())
    {
        Q_ASSERT(false); // TODO: Goto this row's contents
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
    if (m_currentCard->isTOC())
        ; // noop
    else if (m_currentCard->isContent())
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
    if (m_currentCard->isTOC())
        ; // noop
    else if (m_currentCard->isContent())
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
    Q_ASSERT(m_currentCard->isContent());
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
            if (m_currentCard->threadStart() == m_currentCard)
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
            if (oldCard != m_currentCard)
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
        {
            m_row = 1;
            m_col = 0;
            m_actionMode = true;
        }
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
        {
            m_row = 1;
            m_col = 0;
            m_actionMode = true;
        }
        showCard(prevCard);
    }
}

void Cursor::prevThreadCard()
{
    CardItem* prevCard = m_currentCard->threadPrev();
    if (prevCard)
    {
        if (prevCard->isTOC())
        {
            m_row = 1;
            m_col = 0;
            m_actionMode = true;
        }
        showCard(prevCard);
    }
}

void Cursor::nextThreadCard()
{
    CardItem* nextCard = m_currentCard->threadNext();
    if (nextCard)
    {
        if (nextCard->isTOC())
        {
            m_row = 1;
            m_col = 0;
            m_actionMode = true;
        }
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
    m_actionMode = false;
}

void Cursor::newTOC()
{
    Q_ASSERT(m_yearToCardStack.contains(m_year));
    m_yearToCardStack[m_year]->add(CardItem::Type::TOC, CardStack::ThreadMode::New);
    m_actionMode = false;
}

#if 0
void Cursor::continueContent()
{
    auto& cardStack = m_yearToCardStack[m_year];
    Q_ASSERT(0); // TODO next line
    // cardStack.continueContent(m_currentCard);
    // CardNum newCardNum = cardStack.lastCardNum() + 1;
    // m_row = 1; // !!!
    // m_col = 0;
    // auto* newCard = new TOCItem(newCardNum, m_year);
    // cardStack.add(newCard);
    // newCard->setThreadStart(m_currentCard->threadStart()); // !!!
    // m_currentCard->setThreadNext(newCard);                 // !!!
    // newCard->setThreadPrev(m_currentCard);                 // !!!

    // m_scene->addItem(newCard);
    // showCard(newCard);
}
#endif
void Cursor::draw(QPainter* painter, const QRectF& rect, const bool typing)
{
    Q_ASSERT(m_currentCard);
    const RowItem* rowItem = m_currentCard->rowItemAt(m_row);
    Q_ASSERT(rowItem);

    // Darken all but current row
    if (typing)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 15));

        const qreal rowHeight_scn = rowItem->rowHeight_scn();
        const qreal lineY_scn = m_currentCard->rowLineY_scn(m_row);

        const qreal x = Card::kLeft_scn;
        const qreal y = lineY_scn - rowHeight_scn;
        const qreal w = Card::kRight_scn - Card::kLeft_scn;
        const qreal h = rowHeight_scn;
        const QRectF row_scn(x, y, w, h);

        const QRectF card_itm = m_currentCard->boundingRect();
        const QPolygonF card_scn = m_currentCard->mapRectToScene(card_itm);

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
        const QColor orangishRed(227, 59, 36);
        QPen pen(orangishRed);
        pen.setCosmetic(true);
        pen.setWidthF(2.0);
        painter->setPen(pen);
        painter->setBrush(Qt::transparent);

        const qreal rowHeight_scn = rowItem->rowHeight_scn();
        const qreal charHeight_scn = rowItem->charHeight_scn();
        const qreal charWidth_scn = rowItem->charWidth_scn();
        const qreal lineY_scn = m_currentCard->rowLineY_scn(m_row);

        if (typing)
        {
            const QPointF topLeft(
                m_col * charWidth_scn + Card::kBorder_scn,
                lineY_scn - rowHeight_scn + (rowHeight_scn - charHeight_scn) / 2.0);
            const QPointF bottomRight(
                topLeft.x() + charWidth_scn,
                lineY_scn - (rowHeight_scn - charHeight_scn) / 2.0);
            const QRectF cursorRect(topLeft, bottomRight);
            painter->drawRoundedRect(cursorRect, 25.0, 25.0, Qt::RelativeSize);
        }
        else
        {
            painter->setBrush(orangishRed);
            const qreal deltaCharRow = rowHeight_scn - charHeight_scn;
            const qreal centerX = m_col * charWidth_scn + Card::kBorder_scn + charWidth_scn / 2.0;
            const qreal x1 = centerX;
            const qreal y1 = lineY_scn - deltaCharRow / 2.0;
            const qreal x2 = centerX - charWidth_scn / 2.0;
            const qreal y2 = lineY_scn - deltaCharRow / 10.0;
            const qreal x3 = centerX + charWidth_scn / 2.0;
            const qreal y3 = lineY_scn - deltaCharRow / 10.0;

            const QPointF points[3] = {
                QPointF(x1, y1),
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