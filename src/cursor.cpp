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

    // TODO
    // Create master cardstack with empty toc
    // Add help collection
    // Create current year cardstack with empty toc, connect to master cardstack

    // Setup master card stack
    Q_ASSERT(!m_yearToCardStack.contains(Master::kYear));
    m_yearToCardStack.insert(Master::kYear, CardStack{Master::kYear, m_scene});
    Q_ASSERT(m_yearToCardStack.contains(Master::kYear));
    CardStack& masterCS = m_yearToCardStack[Master::kYear];
    m_currentCard = masterCS.toc();
    m_year = Master::kYear;
    m_row = m_currentCard->firstEditableRow();
    m_col = 0;

#if 0   
    masterCS.addCollection(currentCard);
    masterCS.lastCard()->firstRow()->setText("Help");

    // Setup current year card stack
    const Year currentYear = QDate::currentDate().year();
    Q_ASSERT(!m_yearToCardStack.contains(currentYear));
    m_yearToCardStack.insert(currentYear, CardStack{currentYear, m_scene});
    Q_ASSERT(m_yearToCardStack.contains(currentYear));
    // Init
    m_year = QDate::currentDate().year();
    m_currentCard = masterCS.toc();
    m_row = m_currentCard->firstEditableRow();
    m_col = 0;
#endif

    Q_ASSERT(m_currentCard);
}

CardNum Cursor::lastCardNum() const
{
    const auto& cardStack = m_yearToCardStack[m_year];
    return cardStack.lastCardNum();
}

void Cursor::up()
{
    uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
    prevRow();
    uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
    m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
}

void Cursor::down()
{
    uint32_t oldColsPerRow = m_currentCard->colPerRow(m_row);
    nextRow();
    uint32_t newColsPerRow = m_currentCard->colPerRow(m_row);
    m_col = static_cast<uint32_t>(m_col) * newColsPerRow / oldColsPerRow;
}

void Cursor::left()
{
    if (m_col == m_currentCard->firstCol(m_row))
    {
        Row oldRow = m_row;
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
        Row oldRow = m_row;
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
        CardItem* oldCard = m_currentCard;
        nextThreadCard();
        if (oldCard != m_currentCard)
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
        CardItem* oldCard = m_currentCard;
        prevThreadCard();
        if (oldCard != m_currentCard)
            m_row = m_currentCard->lastEditableRow();
    }
    else
        m_row--;
}

void Cursor::nextCard()
{
    if (m_currentCard->cardNum() == lastCardNum())
    {
        // Last card...Stop!
        // TODO: UI indicates at last card
    }
    else
    {
        auto& cardStack = m_yearToCardStack[m_year];
        CardNum cardNum = m_currentCard->cardNum();
        CardItem* nextCard = cardStack.card(cardNum + 1);
        showCard(nextCard);
    }
}

void Cursor::prevCard()
{
    if (m_currentCard->cardNum() == 0)
    {
        // First card...Stop!
        // TODO: UI indicates at last card
    }
    else
    {
        auto& cardStack = m_yearToCardStack[m_year];
        CardNum cardNum = m_currentCard->cardNum();
        CardItem* prevCard = cardStack.card(cardNum - 1);
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
        continueContent();
}

void Cursor::newContent()
{
    auto& cardStack = m_yearToCardStack[m_year];
    CardNum newCardNum = cardStack.lastCardNum() + 1;
    m_col = 0;
    m_row = 0;
    CardItem* newCard = new TOCItem(newCardNum, m_year);
    cardStack.add(newCard);
    newCard->setThreadStart(newCard);

    if (m_currentCard->isTOC())
        newCard->setThreadPrev(m_currentCard);
    else if (m_currentCard->isContent())
        newCard->setThreadPrev(m_currentCard->TOC());

    m_scene->addItem(newCard);
    showCard(newCard);
}

void Cursor::continueContent()
{
    auto& cardStack = m_yearToCardStack[m_year];
    CardNum newCardNum = cardStack.lastCardNum() + 1;
    m_row = 1; // !!!
    m_col = 0;
    auto* newCard = new TOCItem(newCardNum, m_year);
    cardStack.add(newCard);
    newCard->setThreadStart(m_currentCard->threadStart()); // !!!
    m_currentCard->setThreadNext(newCard);                 // !!!
    newCard->setThreadPrev(m_currentCard);                 // !!!

    m_scene->addItem(newCard);
    showCard(newCard);
}

void Cursor::newTOC()
{
    auto& cardStack = m_yearToCardStack[m_year];
    Q_ASSERT(!cardStack.readOnly());
    CardNum newCardNum = cardStack.lastCardNum() + 1;
    m_col = 0;
    m_row = 0;
    TOCItem* newCard = new TOCItem(newCardNum, m_year);
    cardStack.add(newCard);

    for (int i = 1; i < Card::kNumRows; ++i)
    {
        newCard->rowItem(i)->setReadOnly(true);
    }
    newCard->setThreadStart(newCard);

    if (!m_currentCard)
    {
        newCard->setThreadPrev(nullptr);
    }
    else
    {
        if (m_currentCard->isTOC())
            newCard->setThreadPrev(m_currentCard); // sub TOC
        else if (m_currentCard->isContent())
            newCard->setThreadPrev(m_currentCard->TOC());
    }

    m_scene->addItem(newCard);
    m_currentCard = newCard;
    showCard(newCard);
}

void Cursor::continueTOC()
{
}

void Cursor::draw(QPainter* painter, const QRectF& rect, const bool typing)
{
    Q_ASSERT(m_currentCard);
    const RowItem* rowItem = m_currentCard->rowItem(m_row);
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