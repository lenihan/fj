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
    Q_ASSERT(m_content.size() <= Card::kNumUserBodyRows);
    if (m_content.size() == Card::kNumUserBodyRows)
    {
        auto* newTOC = new TOCItem(cardNumber(), year());
        newTOC->addToTOC(card);
        return;
    }
    m_content.push_back(card);

    Row row = m_content.size();
    setupRowAt(row);
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

void TOCItem::setupRowAt(Row row)
{
    CardItem* card = m_content[row - 1];
    RowItem* rowItem = rowItemAt(row);
    ColCount totalCol = rowItem->colPerRow();

    QString title = card->firstRowItem()->text();
    title = rtrim(title);

        bool includeYear = card->year() != year();
    QString fullCardNum = QString::number(card->cardNumber());
    if (includeYear)
    {
        QString yearStr = QString::number(card->year()).rightJustified(4, '0');
        fullCardNum = yearStr + "-" + fullCardNum;
    }
    int two_spaces = 2;
    int dotsNeeded = totalCol - title.length() - fullCardNum.length() - two_spaces;
    QString dots = QString(dotsNeeded, '.');

    QString text = title + " " + dots + " " + fullCardNum;
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
