#include "cursor.h"
#include "cardItem.h"
#include "constants.h"
#include "rowItem.h"
#include <QDate>
#include <QGraphicsScene>
#include <QPainter>
#include <QPen>

Cursor::Cursor(QGraphicsScene* scene) : m_scene(scene)
{
    // Init
    m_year = QDate::currentDate().year();
    m_cardNum = 0;
    m_row = 0;
    m_col = 0;

    newCollection();
    Q_ASSERT(m_scene);
    Q_ASSERT(m_currentCard);
}

uint16_t Cursor::lastThreadCard() const
{
    CardItem* lastCard = m_currentCard;
    while (CardItem* nextCard = lastCard->threadNext())
        lastCard = nextCard;
    return lastCard->cardNum();
}

uint16_t Cursor::lastCardNum() const
{
    const auto& cardStack = m_yearToCardStack[m_year];
    return cardStack.size() - 1;
}

void Cursor::up()
{
    const uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
    prevRow();
    const uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
    m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
}

void Cursor::down()
{
    const uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
    nextRow();
    const uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
    m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
}

void Cursor::left()
{
    if (m_col == m_currentCard->firstCol(m_row))
    {
        uint8_t oldRow = m_row;
        prevRow();
        if (oldRow != m_row)
            m_col = m_currentCard->lastCol(m_row);
    }
    else
        m_col--;
}

void Cursor::right()
{
    if (m_col == m_currentCard->lastCol(m_row))
    {
        const uint8_t oldRow = m_row;
        nextRow();
        if (oldRow != m_row)
            m_col = 0;
    }
    else
        m_col++;
}

void Cursor::enter()
{
    nextRowCreateCard();
    m_col = 0;
}

void Cursor::backspace()
{
    if (m_col == m_currentCard->firstCol(m_row))
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

void Cursor::charTyped(QChar c)
{
    m_currentCard->setChar(c, m_row, m_col);
    right();
}

void Cursor::nextRow()
{
    if (m_row == m_currentCard->lastEditableRow())
    {
        const uint16_t oldCardNum = m_cardNum;
        nextThreadCard();
        if (oldCardNum != m_cardNum)
            m_row = m_currentCard->firstEditableRow();
    }
    else
        m_row++;
}

void Cursor::nextRowCreateCard()
{
    if (m_row == m_currentCard->lastEditableRow())
        nextThreadCardCreateCard();
    else
        m_row++;
}

void Cursor::prevRow()
{
    if (m_row == m_currentCard->firstEditableRow())
    {
        const uint16_t oldCardNum = m_cardNum;
        prevThreadCard();
        if (oldCardNum != m_cardNum)
            m_row = m_currentCard->lastEditableRow();
    }
    else
        m_row--;
}

void Cursor::nextCard()
{
    if (m_cardNum == lastCardNum())
    {
        // Last card...Stop!
        // TODO: UI indicates at last card
    }
    else
    {
        auto& cardStack = m_yearToCardStack[m_year];
        CardItem* nextCard = cardStack.at(m_cardNum + 1);
        showCard(nextCard);
    }
}

void Cursor::prevCard()
{
    if (m_cardNum == 0)
    {
        // Last card...Stop!
        // TODO: UI indicates at last card
    }
    else
    {
        auto& cardStack = m_yearToCardStack[m_year];
        CardItem* prevCard = cardStack[m_cardNum - 1];
        showCard(prevCard);
    }
}

void Cursor::prevThreadCard()
{
    CardItem* prevCard = m_currentCard->threadPrev();
    if (prevCard)
        showCard(prevCard);
}

void Cursor::nextThreadCard()
{
    CardItem* nextCard = m_currentCard->threadNext();
    if (nextCard)
        showCard(nextCard);
}

void Cursor::nextThreadCardCreateCard()
{
    CardItem* nextCard = m_currentCard->threadNext();
    if (nextCard)
        showCard(nextCard);
    else
        continueCollection();
}

void Cursor::newCollection()
{
    auto& cardStack = m_yearToCardStack[m_year];
    m_cardNum = cardStack.size();
    m_col = 0;
    m_row = 0;
    CardItem* newCard = cardStack.emplaceBack(new CardItem(m_cardNum, m_year));
    newCard->lastRow()->setReadOnly(true);
    newCard->setThreadStart(newCard);
    // TODO: newCard->setThreadPrev(index card num);

    m_scene->addItem(newCard);
    m_currentCard = newCard;
    showCard(newCard);
}

void Cursor::continueCollection()
{
    auto& cardStack = m_yearToCardStack[m_year];
    m_cardNum = cardStack.size();
    m_row = 1;
    m_col = 0;
    CardItem* newCard = cardStack.emplaceBack(new CardItem(m_cardNum, m_year));
    newCard->firstRow()->setReadOnly(true);
    newCard->lastRow()->setReadOnly(true);
    newCard->setThreadStart(m_currentCard->threadStart());
    m_currentCard->setThreadNext(newCard);
    newCard->setThreadPrev(m_currentCard);

    m_scene->addItem(newCard);
    showCard(newCard);
}

void Cursor::draw(QPainter* painter, const QRectF& rect)
{
    QPen pen(Qt::red);
    pen.setCosmetic(true);
    pen.setWidthF(5.0);
    painter->setPen(pen);
    painter->setBrush(Qt::transparent);

    const RowItem* rowItem = m_currentCard->rowItem(m_row);

    const qreal rowHeight_scn = rowItem->rowHeight_scn();
    const qreal charHeight_scn = rowItem->charHeight_scn();
    const qreal charWidth_scn = rowItem->charWidth_scn();
    const qreal lineY_scn = m_currentCard->rowLineY_scn(m_row);

    const QPointF topLeft(
        m_col * charWidth_scn + Card::kBorder_scn,
        lineY_scn - rowHeight_scn + (rowHeight_scn - charHeight_scn) / 2.0
    );
    const QPointF bottomRight(
        topLeft.x() + charWidth_scn,
        lineY_scn - (rowHeight_scn - charHeight_scn) / 2.0
    );
    const QRectF cursorRect(topLeft, bottomRight);
    painter->drawRoundedRect(cursorRect, 25.0, 25.0, Qt::RelativeSize);
    // painter->drawRect(cursorRect);

    // const QPointF points[3] = {
    //     QPointF(m_col * charWidth_scn + Card::kBorder_scn,
    //             lineY_scn - (rowHeight_scn - charHeight_scn) / 2.0),
    //     QPointF(m_col * charWidth_scn + Card::kBorder_scn - charWidth_scn / 2.0,
    //             lineY_scn),
    //     QPointF(m_col * charWidth_scn + Card::kBorder_scn + charWidth_scn / 2.0,
    //             lineY_scn)};
    // painter->drawPolygon(points, 3);
}

void Cursor::showCard(CardItem* card)
{
    Q_ASSERT(m_currentCard);
    m_currentCard->hide();
    m_currentCard = card;
    m_cardNum = m_currentCard->cardNum();
    m_currentCard->show();
}