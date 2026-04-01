#include "cursor.h"
#include "cardItem.h"
#include "constants.h"
#include "rowItem.h"
#include <QGraphicsScene>
#include <QPainter>
#include <QPen>

Cursor::Cursor(QGraphicsScene* scene) : m_scene(scene)
{
    // Init
    m_year = 2026;
    m_cardNum = 0;
    m_row = 1;
    m_col = 0;

    auto& cardStack = m_yearToCardStack[m_year];
    CardItem* card = cardStack.emplaceBack(new CardItem(m_cardNum));
    card->setText(0, QString::number(m_year) + " Index");
    scene->addItem(card);

    m_currentCard = card;
    Q_ASSERT(m_currentCard);
    Q_ASSERT(m_scene);
}

void Cursor::up()
{
    const bool rowChanged = prevRow();

    if (m_row == 0 && rowChanged)
        bodyToTitleColUpdate();
    else if (m_row == m_currentCard->userRowsPerCard() - 1)
        titleToBodyColUpdate();
}

void Cursor::down()
{
    nextRow();
    if (m_row == 1)
        titleToBodyColUpdate();
    else if (m_row == 0)
        bodyToTitleColUpdate();
}

void Cursor::left()
{
    if (m_col == 0)
    {
        const bool changedRow = prevRow();
        if (changedRow)
            m_col = m_currentCard->colPerRow(m_row) - 1;
    }
    else
        m_col--;
}

void Cursor::right()
{
    if (m_col == (m_currentCard->colPerRow(m_row) - 1))
    {
        nextRow();
        m_col = 0;
    }
    else
        m_col++;
}

void Cursor::enter()
{
    nextRow();
    m_col = 0;
}

void Cursor::backspace()
{
    if (m_col == 0)
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
    if (m_row == (m_currentCard->userRowsPerCard() - 1))
    {
        nextCard();
        m_row = 0;
    }
    else
        m_row++;
}

bool Cursor::prevRow()
{
    bool rowChanged = false;
    if (m_row == 0)
    {
        const bool cardChanged = prevCard();
        if (cardChanged)
        {
            m_row = m_currentCard->userRowsPerCard() - 1;
            rowChanged = true;
        }
    }
    else
    {
        m_row--;
        rowChanged = true;
    }
    return rowChanged;
}

void Cursor::nextCard()
{
    m_currentCard->hide();
    auto& cardStack = m_yearToCardStack[m_year];
    if (m_cardNum == (cardStack.size() - 1))
    {
        CardItem* nextCard = cardStack.emplaceBack(new CardItem(m_cardNum + 1));
        m_scene->addItem(nextCard);
    }
    m_cardNum++;
    m_currentCard = cardStack[m_cardNum];
    m_currentCard->show();
}

bool Cursor::prevCard()
{
    bool cardChanged = false;
    if (m_cardNum != 0)
    {
        m_currentCard->hide();
        m_cardNum--;
        cardChanged = true;
        auto& cardStack = m_yearToCardStack[m_year];
        m_currentCard = cardStack[m_cardNum];
        m_currentCard->show();
    }
    return cardChanged;
}

void Cursor::draw(QPainter* painter, const QRectF& rect)
{
    QPen pen(Qt::red);
    pen.setCosmetic(true);
    pen.setWidthF(2.0);
    painter->setPen(pen);
    painter->setBrush(Qt::red);

    const RowItem* rowItem = m_currentCard->rowItem(m_row);

    const qreal rowHeight_scn = rowItem->rowHeight_scn();
    const qreal charHeight_scn = rowItem->charHeight_scn();
    const qreal charWidth_scn = rowItem->charWidth_scn();
    const qreal lineY_scn = m_currentCard->rowLineY_scn(m_row);

    const QPointF points[3] = {
        QPointF(m_col * charWidth_scn + Card::kBorder_scn,
                lineY_scn - (rowHeight_scn - charHeight_scn) / 2.0),
        QPointF(m_col * charWidth_scn + Card::kBorder_scn - charWidth_scn / 2.0,
                lineY_scn),
        QPointF(m_col * charWidth_scn + Card::kBorder_scn + charWidth_scn / 2.0,
                lineY_scn)};
    painter->drawPolygon(points, 3);
}

void Cursor::bodyToTitleColUpdate()
{
    const uint32_t col = m_col;
    const uint32_t titleCols = Title::kColsPerRow;
    const uint32_t bodyCols = Body::kColsPerRow;
    m_col = static_cast<uint8_t>(col * titleCols / bodyCols);
}

void Cursor::titleToBodyColUpdate()
{
    const uint32_t col = m_col;
    const uint32_t titleCols = Title::kColsPerRow;
    const uint32_t bodyCols = Body::kColsPerRow;
    m_col = static_cast<uint8_t>(col * bodyCols / titleCols);
}
