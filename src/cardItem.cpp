#include "cardItem.h"
#include "rowItem.h"

#include <QPen>

CardItem::CardItem(uint16_t page, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), m_page(page), m_currentRow(0), m_currentCol(0)
{
    // Card background
    setRect(Card::kRect_scn);
    setPen(Qt::NoPen);
    setBrush(QBrush(Card::kColor));

    // Row lines
    for (int i = 0; i < Card::kNumRows - 1; ++i)
    {
        auto* line = new QGraphicsLineItem(this);
        const qreal y =
            Card::kTop_scn + Title::kRowHeight_scn + (i * Body::kRowHeight_scn);
        line->setLine(Card::kLeft_scn, y, Card::kRight_scn, y);

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
    const QString pageNum = QString::number(m_page+1);
    QString centeredPageNum(cols, ' ');
    const qsizetype position = (cols - pageNum.length())/2;
    const qsizetype n = pageNum.length();
    centeredPageNum.replace(position, n, pageNum);
    Q_ASSERT(centeredPageNum.length() == cols);
    m_rows[lastRow]->setText(centeredPageNum);
}

void CardItem::setChar(const QChar ch, const uint8_t row, const uint8_t col)
{
    Q_ASSERT(row < Card::kNumRows);
    Q_ASSERT(col < m_rows[row]->colPerRow());
    auto x = m_rows[row]->text().size();
    auto y = m_rows[row]->colPerRow();
    Q_ASSERT(m_rows[row]->text().length() <= m_rows[row]->colPerRow());
    QString t = m_rows[row]->text();
    if (t.length() < (col + 1))
    {
        t += QString(col + 1 - t.length(), ' ');
    }
    t[col] = ch;
    m_rows[row]->setText(t);
}

void CardItem::setText(const uint8_t row, const QString& text)
{
    m_rows[row]->setText(text);
}

void CardItem::setText(QStringList text)
{
    for (int i = 0; i < Card::kNumRows; ++i)
    {
        QString t = m_rows[i]->text();
        t.replace(0, text[i].size(), text[i]);
        m_rows[i]->setText(t);
    }
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