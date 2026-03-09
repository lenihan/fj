#include "CardItem.h"
#include "rowItem.h"
#include <QPen>

CardItem::CardItem(QGraphicsItem* parent)
    : QGraphicsRectItem(parent),
      kCardTopLeftPt_scn(kCardLeft_scn, kCardTop_scn),
      kCardBottomRightPt_scn(kCardRight_scn, kCardBottom_scn),
      kCardRect_scn(kCardTopLeftPt_scn, kCardBottomRightPt_scn)
{
    // Card background
    setRect(kCardRect_scn);
    setPen(Qt::NoPen);
    setBrush(QBrush(kCardColor));

    // Row lines
    for (int i = 0; i < kNumRows - 1; ++i)
    {
        auto* line = new QGraphicsLineItem(this);
        const qreal y = kCardTop_scn + kTitleRowHeight_scn + (i * kBodyRowHeight_scn);
        line->setLine(kCardLeft_scn, y, kCardRight_scn, y);

        QPen pen(i == 0 ? kTitleLineColor : kBodyLineColor);
        pen.setWidthF(3.0);
        pen.setCosmetic(true);
        line->setPen(pen);
    }


    // Dummy card
    int i = 0;
    QStringList rowText = QStringList(11);
    rowText[i++] = "Example Card";
    rowText[i++] = "Typing mode:       Caps+Space";
    rowText[i++] = "  Cursor up:       Caps+I";
    rowText[i++] = "  Cursor left:     Caps+J";
    rowText[i++] = "  Cursor down:     Caps+K";
    rowText[i++] = "  Cursor right:    Caps+L";
    rowText[i++] = "  Delete:          Shift+Backspace";
    rowText[i++] = "  Indent:          Tab";
    rowText[i++] = "  Unindent:        Shift+Tab";
    rowText[i++] = "Move cursor:       Caps,I|J|K|L";
    rowText[i++] = "                             1                            ";

    // Row text
        // Title text item
    // auto *titleRow = new RowItem(0);
    // titleRow->setText(m_rows[0]);
    // scene->addItem(titleRow);

    // Body text
    {
        for( int i = 0; i < kNumRows; ++i)
        {
            // Body Text Item
            {
                auto *bodyRow = new RowItem(i, this);
                bodyRow->setText(rowText[i]);
                // scene->addItem(bodyRow);
            }
        }
    }
}