#include "rowItem.h"

#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>

RowItem::RowItem(uint8_t row, QGraphicsItem* parent)
    : QGraphicsSimpleTextItem(parent), kFont(getFont()),
      kFontCharWidth_fnt(getFontCharWidth_fnt()),
      kFontCharHeight_fnt(getFontCharHeight_fnt()),
      kCharsPerRow(row == 0 ? Title::kCharsPerRow : Body::kCharsPerRow),
      kRowHeight_scn(row == 0 ? Title::kRowHeight_scn : Body::kRowHeight_scn),
      m_row(row)
{
    setFont(kFont);

    // Calc font to scene scale
    const qreal rowWidth_fnt = kFontCharWidth_fnt * kCharsPerRow;
    const qreal fntToScn_scale = Card::kUseabledWidth_scn / rowWidth_fnt;
    setScale(fntToScn_scale);

    // Calc y offset to center text
    const qreal y = Title::kRowHeight_scn + ((m_row - 1) * kRowHeight_scn);
    const qreal fontHeight_scn = kFontCharHeight_fnt * fntToScn_scale;
    const qreal yOffset_scn = (kRowHeight_scn - fontHeight_scn) / 2.0;
    setPos(Card::kLeft_scn + Card::kBorder_scn, y + yOffset_scn);
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
qreal RowItem::getFontCharHeight_fnt()
{
    const QFontMetricsF fm(getFont());
    return fm.height();
}

// static
qreal RowItem::getFontCharWidth_fnt()
{
    const QFontMetricsF fm(getFont());
    return fm.maxWidth();
}
