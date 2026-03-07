#include "rowItem.h"

#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>

RowItem::RowItem(RowType rowType)
    : QGraphicsSimpleTextItem(), kFont(getFont()),
      kCharsPerRow(rowType == RowType::Title ? kTitleCharsPerRow
                                              : kBodyCharsPerRow),
      kCharWidth_fnt(getCharWidth()), kCharHeight_fnt(getCharHeight()),
      kCardTopLeftPt_scn(kCardLeft_scn, kCardTop_scn),
      kCardBottomRightPt_scn(kCardRight_scn, kCardBottom_scn),
      kCardRect_scn(kCardTopLeftPt_scn, kCardBottomRightPt_scn),
      m_rowType(rowType)
{
    // Calc font to scene scale
    const qreal rowWidth_fnt = kCharWidth_fnt * kCharsPerRow;
    const qreal fntToScn_scale = kUseableCardWidth_scn / rowWidth_fnt;
    setScale(fntToScn_scale);

    // Calc y offset to center text
    const qreal fontHeight_scn = kCharHeight_fnt * fntToScn_scale;
    const qreal yOffset_scn = (kTitleRowHeight_scn - fontHeight_scn) / 2.0;
    setPos(kCardLeft_scn + kCardBorder_scn, yOffset_scn);

}

void RowItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                    QWidget* widget)
{
    QGraphicsSimpleTextItem::paint(painter, option, widget);
}

// static
QFont RowItem::getFont()
{
    static const QFont font = []
    {
        // Load the font from the resource
        QFontDatabase fontDatabase;
        const int fontId =
            fontDatabase.addApplicationFont(":/fonts/Hack-Regular.ttf");
        Q_ASSERT(fontId != -1);

        // Get the font family name
        QStringList fontFamilies = fontDatabase.applicationFontFamilies(fontId);
        Q_ASSERT(!fontFamilies.isEmpty());

        QString fontFamily = fontFamilies.at(0); // e.g., "Hack"
        QFont f(fontFamily);

        // Verify the font is loaded correctly
        Q_ASSERT(QFontInfo(f).exactMatch());

        return f;
    }();

    return font;
}

// static
qreal RowItem::getCharWidth()
{
    const QFontMetricsF fm(getFont());
    return fm.maxWidth();
}

// static
qreal RowItem::getCharHeight()
{
    const QFontMetricsF fm(getFont());
    return fm.height();
}