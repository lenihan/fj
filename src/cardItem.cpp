#include "cardItem.h"
#include "rowItem.h"
#include <QPen>

CardItem::CardItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), m_cardNum(cardNum), m_year(year)
{
    // Card background
    setRect(Card::kRect_scn);
    setPen(Qt::NoPen);
    setBrush(QBrush(Card::kColor));

    drawLines();

    // Reserve space for rows
    m_rows.reserve(Card::kNumRows);
    for (int i = 0; i < Card::kNumRows; ++i)
    {
        m_rows.emplaceBack(new RowItem(static_cast<Row>(i), this));
    }

    updateLastRow();
}

void CardItem::setChar(const QChar c, Row row, Col col)
{
    Q_ASSERT(row < Card::kNumRows);
    m_rows[row]->setChar(c, row, col);
}

void CardItem::setText(Row row, const QString& text)
{
    m_rows[row]->setText(text);
}

Col CardItem::colPerRow(Row row) const
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row]->colPerRow();
}

qreal CardItem::rowLineY_scn(Row row) const
{
    qreal y_scn =
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

void CardItem::setThreadStart(CardItem* threadStart)
{
    m_threadStart = threadStart;
    bool titleReadOnly = threadStart != this;
    firstRow()->setReadOnly(titleReadOnly);
}

CardItem* CardItem::threadStart() const
{
    return m_threadStart;
}

CardNum CardItem::cardNum() const
{
    return m_cardNum;
}

Year CardItem::year() const
{
    return m_year;
}

void CardItem::setThreadPrev(CardItem* card)
{
    m_threadPrev = card;
    updateLastRow();
}

CardItem* CardItem::threadPrev()
{
    return m_threadPrev;
}

void CardItem::setThreadNext(CardItem* card)
{
    m_threadNext = card;
    updateLastRow();
}

CardItem* CardItem::threadNext()
{
    return m_threadNext;
}

Row CardItem::firstEditableRow() const
{
    if (m_threadStart == this)
        return 0;
    else
        return 1;
}

Row CardItem::lastEditableRow() const
{
    Row lastRow = Card::kNumRows - 1;
    return lastRow - 1;
}

Col CardItem::lastCol(Row row) const
{
    return colPerRow(row) - 1;
}

Col CardItem::firstCol(Row row) const
{
    return 0;
}

bool CardItem::isIndex() const
{
    return m_isTOC;
}

CardItem* CardItem::toc()
{
    Q_ASSERT(m_threadStart);
    return m_threadStart->threadPrev();
}

QVariant CardItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemVisibleHasChanged)
    {
        if (value.toBool())
        { // <-- just became visible
            Q_ASSERT(threadStart());
            const QString title = threadStart()->firstRow()->text();
            firstRow()->setText(title);
            update(); // request repaint
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

QString CardItem::threadStr(CardItem* card)
{
    QString str;

    if (!card)
        return str;

    if (m_threadStart == this && card == m_threadPrev)
        str = "↑"; // Parent TOC
    else
        str = "→"; // Thread

    if (m_year != card->year())
        str += QString::number(card->year()) + ":";

    str += QString::number(card->cardNum() + 1);

    return str;
}

void CardItem::updateLastRow()
{
    RowItem* row = lastRow();
    row->setReadOnly(true);

    // Last line: Prev thread       card num         next thread
    Row lastRow = Card::kNumRows - 1;
    Col cols = row->colPerRow();
    QString text(cols, ' ');
    qsizetype pos;
    qsizetype n;

    // prev
    const QString prev = threadStr(m_threadPrev);
    pos = 0;
    n = prev.length();
    text.replace(pos, n, prev);

    // card num
    const QString cardNumStr = QString::number(m_cardNum + 1);
    pos = (cols - cardNumStr.length()) / 2;
    n = cardNumStr.length();
    text.replace(pos, n, cardNumStr);

    // next
    const QString next = threadStr(m_threadNext);
    pos = cols - next.length();
    n = next.length();
    text.replace(pos, n, next);

    Q_ASSERT(text.length() == cols);
    row->setText(text);
}

void CardItem::drawLines()
{
    for (int i = 0; i < Card::kNumRows - 1; ++i)
    {
        auto* line = new QGraphicsLineItem(this);
        qreal y_scn = rowLineY_scn(i);
        line->setLine(Card::kLeft_scn, y_scn, Card::kRight_scn, y_scn);

        QPen pen(i == 0 ? Title::kLineColor : Body::kLineColor);
        pen.setWidthF(3.0);
        pen.setCosmetic(true);
        line->setPen(pen);
    }
}

const RowItem* CardItem::rowItem(Row row) const
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row];
}

RowItem* CardItem::rowItem(Row row)
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row];
}
