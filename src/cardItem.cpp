#include "cardItem.h"
#include "rowItem.h"
#include <QPen>

CardItem::CardItem(CardNum cardNum, Year year, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), m_cardNum(cardNum), m_year(year)
{
    // Reserve space for rows
    m_rows.reserve(Card::kNumRows);

    for (int i = 0; i < Card::kNumRows; ++i)
        m_rows.emplaceBack(new RowItem(static_cast<RowNum>(i), this));

    setupLastRow();
}

void CardItem::setChar(const QChar c, RowNum row, ColNum col)
{
    Q_ASSERT(row < Card::kNumRows);
    m_rows[row]->setChar(c, row, col);
}

void CardItem::setText(RowNum row, const QString& text)
{
    m_rows[row]->setText(text);
}

ColNum CardItem::colPerRow(RowNum row) const
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row]->colPerRow();
}

qreal CardItem::rowLineY_scn(RowNum row) const
{
    qreal y_scn =
        Card::kTop_scn + Title::kRowHeight_scn + (row * Body::kRowHeight_scn);
    return y_scn;
}

RowItem* CardItem::firstRowItem()
{
    return m_rows[0];
}

RowItem* CardItem::lastRowItem()
{
    return m_rows[Card::kNumRows - 1];
}

void CardItem::setThreadStart(CardItem* threadStart)
{
    m_threadStart = threadStart;
    bool titleReadOnly = threadStart != this;
    firstRowItem()->setReadOnly(titleReadOnly);
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
    setupLastRow();
}

CardItem* CardItem::threadPrev()
{
    return m_threadPrev;
}

void CardItem::setThreadNext(CardItem* card)
{
    m_threadNext = card;
    setupLastRow();
}

CardItem* CardItem::threadNext()
{
    return m_threadNext;
}

RowNum CardItem::firstEditableRowNum() const
{
    if (m_threadStart == this)
        return 0;
    else
        return 1;
}

RowNum CardItem::lastEditableRowNum() const
{
    RowNum lastRow = Card::kNumRows - 1;
    return lastRow - 1;
}

ColNum CardItem::lastColNum(RowNum row) const
{
    return colPerRow(row) - 1;
}

ColNum CardItem::firstColNum(RowNum row) const
{
    return 0;
}

CardItem* CardItem::TOC()
{
    Q_ASSERT(m_threadStart);
    CardItem* toc = m_threadStart->isTOC() ? m_threadStart : m_threadStart->threadPrev();
    Q_ASSERT(toc);
    return toc;
}

void CardItem::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
}

bool CardItem::readOnly() const
{
    return m_readOnly;
}

QVariant CardItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemVisibleHasChanged)
    {
        if (value.toBool())
        { // <-- just became visible
            Q_ASSERT(threadStart());
            const QString title = threadStart()->firstRowItem()->text();
            firstRowItem()->setText(title);
            update(); // request repaint
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

void CardItem::setupBackground()
{
    setRect(Card::kRect_scn);
    setPen(Qt::NoPen);
    setBrush(QBrush(Card::kColor));
}

void CardItem::setupLines()
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

void CardItem::setupLastRow()
{
    RowItem* lastRow = lastRowItem();
    lastRow->setReadOnly(true);

    // Last line: Prev thread       card num         next thread
    ColNum colTotal = lastRow->colPerRow();
    QString text(colTotal, ' ');
    qsizetype pos;
    qsizetype n;

    // prev
    QString prev = threadStr(m_threadPrev);
    pos = 0;
    n = prev.length();
    text.replace(pos, n, prev);

    // card num
    QString cardNumStr = QString::number(m_cardNum + 1);
    pos = (colTotal - cardNumStr.length()) / 2;
    n = cardNumStr.length();
    text.replace(pos, n, cardNumStr);

    // next
    QString next = threadStr(m_threadNext);
    pos = colTotal - next.length();
    n = next.length();
    text.replace(pos, n, next);

    Q_ASSERT(text.length() == colTotal);
    lastRow->setText(text);
}

const RowItem* CardItem::rowItem(RowNum row) const
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row];
}

RowItem* CardItem::rowItem(RowNum row)
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row];
}
