#include "tocItem.h"
#include "rowItem.h"

TOCItem::TOCItem(CardNumber cardNumber, Year year, QGraphicsItem* parent)
    : CardItem(cardNumber, year, parent)
{
    m_content.reserve(Card::kNumUserBodyRows);
    setupBackground();
}

void TOCItem::addToTOC(CardItem* card)
{
    Q_ASSERT(card);
    Q_ASSERT(m_content.size() < Card::kNumUserBodyRows);
    m_content.push_back(card);
    Row row = m_content.size();
    setupRowAt(row);
    
    m_links.clear();
    Q_ASSERT(m_links.size() == 0);
    setupLinks();
    Q_ASSERT(m_links.size() >= 1);
    m_currentLinkIndex = 0;
    
    update();
}

QVariant TOCItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemVisibleHasChanged)
    {
        if (value.toBool())
        { // <-- just became visible

            Q_ASSERT(m_content.size() <= Card::kNumUserBodyRows);
            for (int i = 0; i < m_content.size(); ++i)
            {
                setupRowAt(i + 1);
            }
            update(); // request repaint
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

RowCount TOCItem::numberContent() const
{
    return m_content.size();
}

CardItem* TOCItem::cardAtRow(Row row)
{
    Q_ASSERT(row >= 1);
    Q_ASSERT(row <= m_content.size());
    return m_content[row - 1];
}

Row TOCItem::rowAtCard(CardItem* card) const
{
    Q_ASSERT(card);
    qsizetype index = m_content.indexOf(card);
    Q_ASSERT(index != -1);
    Row row = index + 1;
    return row;
}

bool TOCItem::isEmpty() const
{
    return m_content.empty();
}

bool TOCItem::isFull() const
{
    Q_ASSERT(m_content.size() <= Card::kNumUserBodyRows);
    return m_content.size() == Card::kNumUserBodyRows;
}

void TOCItem::setupLinks()
{
    for(CardItem* card : m_content)
    {
        QString cardLinkStr = linkStr(card);
        Row row = rowAtCard(card);
        Col col = colPerRow(row) - cardLinkStr.length();
        ColCount colCount = cardLinkStr.length();
        m_links.append({row, col, colCount, card});
    }
    CardItem::setupLinks();
}

void TOCItem::setupRowAt(Row row)
{
    Q_ASSERT(row >= 1);
    Q_ASSERT(row <= Card::kNumUserBodyRows);
    CardItem* card = m_content[row - 1];
    RowItem* rowItem = rowItemAt(row);
    ColCount totalCol = rowItem->colPerRow();

    rowItem->setReadOnly(true);

    QString title = card->firstRowItem()->text();
    title = rtrim(title);
    QString cardLinkStr = linkStr(card);
    int two_spaces = 2;
    int dotsNeeded = totalCol - title.length() - cardLinkStr.length() - two_spaces;
    QString dots = QString(dotsNeeded, '.');
    Col col = totalCol - cardLinkStr.length();
    ColCount colCount = totalCol - cardLinkStr.length();

    QString text = title + " " + dots + " " + cardLinkStr;
    Q_ASSERT(text.length() == totalCol);
    rowItem->setText(text);
}

QString TOCItem::rtrim(const QString& str) const
{
    if (str.isEmpty())
        return str;

    int i = str.size() - 1;
    while (i >= 0 && str.at(i).isSpace())
        --i;

    return str.left(i + 1);
}
