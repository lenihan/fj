#include "cardItem.h"
#include "rowItem.h"
#include <QPen>

CardItem::CardItem(QGraphicsItem* parent)
    : QGraphicsRectItem(parent)
    // ,
    //   kCardTopLeftPt_scn(Card::kLeft_scn, Card::kTop_scn),
    //   kCardBottomRightPt_scn(Card::kRight_scn, Card::kBottom_scn),
    //   Card::kRect_scn(kCardTopLeftPt_scn, kCardBottomRightPt_scn)
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

    for (int i = 0; i < Card::kNumRows; ++i)
    {
        auto* bodyRow = new RowItem(i, this);
        bodyRow->setText(rowText[i]);
    }
}