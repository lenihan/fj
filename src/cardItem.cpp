#include "cardItem.h"
#include "rowItem.h"

#include <QPen>

CardItem::CardItem(uint16_t cardNum, uint16_t year, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), m_cardNum(cardNum), m_year(year)
{
    // Card background
    setRect(Card::kRect_scn);
    setPen(Qt::NoPen);
    setBrush(QBrush(Card::kColor));

    // Row lines
    for (int i = 0; i < Card::kNumRows - 1; ++i)
    {
        auto* line = new QGraphicsLineItem(this);
        const qreal y_scn = rowLineY_scn(i);
        line->setLine(Card::kLeft_scn, y_scn, Card::kRight_scn, y_scn);

        QPen pen(i == 0 ? Title::kLineColor : Body::kLineColor);
        pen.setWidthF(3.0);
        pen.setCosmetic(true);
        line->setPen(pen);
    }

    // Reserve space for rows
    m_rows.reserve(Card::kNumRows);
    for (int i = 0; i < Card::kNumRows; ++i)
    {
        m_rows.emplaceBack(new RowItem(static_cast<uint8_t>(i), this));
    }

    // Center page number on last line
    const uint8_t lastRow = Card::kNumRows - 1;
    const uint8_t cols = m_rows[lastRow]->colPerRow();
    const QString pageNum = QString::number(m_cardNum + 1);
    QString centeredPageNum(cols, ' ');
    const qsizetype position = (cols - pageNum.length()) / 2;
    const qsizetype n = pageNum.length();
    centeredPageNum.replace(position, n, pageNum);
    Q_ASSERT(centeredPageNum.length() == cols);
    m_rows[lastRow]->setText(centeredPageNum);
}

void CardItem::setChar(const QChar c, const uint8_t row, const uint8_t col)
{
    Q_ASSERT(row < Card::kNumRows);
    m_rows[row]->setChar(c, row, col);
}

void CardItem::setText(const uint8_t row, const QString& text)
{
    m_rows[row]->setText(text);
}

uint8_t CardItem::colPerRow(uint8_t row) const
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row]->colPerRow();
}

qreal CardItem::rowLineY_scn(uint8_t row) const
{
    const qreal y_scn =
        Card::kTop_scn + Title::kRowHeight_scn + (row * Body::kRowHeight_scn);
    return y_scn;
}

RowItem* CardItem::firstRow()
{
    return m_rows[0];
}

RowItem* CardItem::lastRow()
{
    return m_rows[Card::kNumRows - 1];
}

void CardItem::setThreadStart(bool threadStart)
{
    m_threadStart = threadStart;
}

bool CardItem::threadStart() const
{
    return m_threadStart;
}

uint16_t CardItem::cardNum() const
{
    return m_cardNum;
}

uint16_t CardItem::year() const
{
    return m_year;
}

void CardItem::setThreadPrev(uint16_t cardNum, uint16_t year)
{
    m_threadPrev.m_cardNum = cardNum;
    m_threadPrev.m_year = year;
}

void CardItem::setThreadNext(uint16_t cardNum, uint16_t year)
{
    m_threadNext.m_cardNum = cardNum;
    m_threadNext.m_year = year;
}

uint8_t CardItem::firstEditableRow() const
{
    if (m_threadStart)
        return 0;
    else
        return 1;
}

uint8_t CardItem::lastEditableRow() const
{
    const uint8_t lastRow = Card::kNumRows - 1; // card number
    return lastRow - 1;
}

uint8_t CardItem::lastCol(uint8_t row) const
{
    return colPerRow(row) - 1;
}

uint8_t CardItem::firstCol(uint8_t row) const
{
    return 0;
}

const RowItem* CardItem::rowItem(uint8_t row) const
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row];
}
