#include "cardItem.h"
#include "rowItem.h"

#include <QPen>

CardItem::CardItem(uint16_t page, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), m_cardNum(page)
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

uint8_t CardItem::userRowsPerCard() const
{
    return Card::kNumRows - 1; // last row for page number
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

const RowItem* CardItem::rowItem(uint8_t row) const
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row];
}
