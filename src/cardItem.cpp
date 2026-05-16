#include "cardItem.h"
#include "rowItem.h"
#include <QPen>

CardItem::CardItem(CardNumber cardNumber, Year year, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), m_cardNum(cardNumber), m_year(year)
{
    // Reserve space for rows
    m_rows.reserve(Card::kNumRows);

    for (int i = 0; i < Card::kNumRows; ++i)
        m_rows.emplaceBack(new RowItem(static_cast<Row>(i), this));

    setupLastRow();
        
    m_links.clear();
    setupLinks();
}

bool CardItem::isTOC() const { return cardType() == Type::TOC; }

bool CardItem::isContent() const { return cardType() == Type::Content; }

bool CardItem::isThreadStart() const
{
    return m_threadStart == this;
}

bool CardItem::isThreadEnd() const
{
    return m_threadNext == nullptr;
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

ColCount CardItem::colPerRow(Row row) const
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
}

CardItem* CardItem::threadStart() const
{
    return m_threadStart;
}

CardNumber CardItem::cardNumber() const
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

    m_links.clear();
    setupLinks();
}

CardItem* CardItem::threadPrev()
{
    return m_threadPrev;
}

void CardItem::setThreadNext(CardItem* card)
{
    m_threadNext = card;
    setupLastRow();
    
    m_links.clear();
    setupLinks();
}

CardItem* CardItem::threadNext()
{
    return m_threadNext;
}

void CardItem::setDeleted(bool deleted)
{
    if (m_threadStart->cardNumber() == 0)
    {
        // Don't allow deleting TOC for card stack
        return;
    }
    m_deleted = deleted;
}

bool CardItem::deleted() const
{
    return m_deleted;
}

Row CardItem::firstUserRow() const
{
    return 1;
}

Row CardItem::lastUserRow() const
{
    Row lastRow = Card::kNumRows - 1;
    return lastRow - 1;
}

Col CardItem::lastColAt(Row row) const
{
    return colPerRow(row) - 1;
}

Col CardItem::firstColAt(Row row) const
{
    return 0;
}

CardItem* CardItem::tableOfContents()
{
    Q_ASSERT(m_threadStart);
    CardItem* toc = m_threadStart->isTOC() ? m_threadStart : m_threadStart->threadPrev();
    Q_ASSERT(toc);
    Q_ASSERT(toc->isTOC());
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

CardItem::CardLink CardItem::currentLink() const
{
    Q_ASSERT(m_currentLinkIndex >= 0);
    Q_ASSERT(m_currentLinkIndex < m_links.size());
    return m_links[m_currentLinkIndex];
}

void CardItem::nextLink()
{
    Q_ASSERT(m_links.size() > 0);
    m_currentLinkIndex++;
    if (m_currentLinkIndex >= m_links.size())
        m_currentLinkIndex = 0; // Wrap around
}

void CardItem::prevLink()
{
    Q_ASSERT(m_links.size() > 0);
    m_currentLinkIndex--;
    if (m_currentLinkIndex < 0)
        m_currentLinkIndex = m_links.size() - 1; // Wrap around
}

void CardItem::setLastAsCurrentLink()
{
    // next thread if it exists, otherwise prev thread
    m_currentLinkIndex = m_links.size() - 1;
    Q_ASSERT(m_currentLinkIndex >= 0);
}

void CardItem::setupLinks()
{
    if (m_threadPrev)
    {
        QString prev = linkStr(m_threadPrev);
        Row lastRow = Card::kNumRows - 1;
        Col col = 0;
        ColCount colCount = prev.length();
        m_links.append({lastRow, col, colCount, m_threadPrev});
    }

    if (m_threadNext)
    {
        QString next = linkStr(m_threadNext);
        Row lastRow = Card::kNumRows - 1;
        Col col = lastRowItem()->colPerRow() - next.length();
        ColCount colCount = next.length();
        m_links.append({lastRow, col, colCount, m_threadNext});
    }
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

// Examples: ↑4, →2026-42
QString CardItem::linkStr(CardItem* card) const
{
    QString str;

    if (!card)
        return str;

    // Prefix
    if (m_threadStart == this && card == m_threadPrev)
        str = "↑"; // Parent TOC
    else
        str = "→"; // Thread

    // Year
    if (m_year != card->year())
    {
        str += (card->year() == Master::kYear) ? "Master" : QString::number(card->year());
        str += "-";
    }
    // Card number
    str += QString::number(card->cardNumber() + 1);

    return str;
}

void CardItem::setupLastRow()
{
    RowItem* lastRow = lastRowItem();
    lastRow->setReadOnly(true);

    // Last line: Prev thread       card num         next thread
    ColCount colCount = lastRow->colPerRow();
    QString text(colCount, ' ');
    qsizetype pos;
    qsizetype n;

    // prev (master card 1...has no prev thread)
    if (m_threadPrev)
    {
        QString prev = linkStr(m_threadPrev);
        pos = 0;
        n = prev.length();
        text.replace(pos, n, prev);
    }

    // card num
    QString cardNumStr = QString::number(m_cardNum + 1);
    pos = (colCount - cardNumStr.length()) / 2;
    n = cardNumStr.length();
    text.replace(pos, n, cardNumStr);

    // next
    if (m_threadNext)
    {
        QString next = linkStr(m_threadNext);
        pos = colCount - next.length();
        n = next.length();
        text.replace(pos, n, next);
        m_links.append({lastRow->row(), static_cast<Col>(pos), static_cast<ColCount>(n), m_threadNext});
    }
    if (m_links.size() >= 0)
        m_currentLinkIndex = 0; // prev thread if it exists, otherwise next thread

    Q_ASSERT(text.length() == colCount);
    lastRow->setText(text);
}

const RowItem* CardItem::rowItem(Row row) const
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row];
}

RowItem* CardItem::rowItemAt(Row row)
{
    Q_ASSERT(row <= Card::kNumRows);
    return m_rows[row];
}
