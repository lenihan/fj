#include "rowItem.h"

#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>

RowItem::RowItem(RowType rowType)
    : QGraphicsSimpleTextItem(), m_font(getFont()),
      m_charsPerRow(rowType == RowType::Title ? m_titleCharsPerRow
                                              : m_bodyCharsPerRow),
      m_charWidth_fnt(getCharWidth()), m_charHeight_fnt(getCharHeight()),
      m_cardTopLeftPt_scn(m_cardLeft_scn, m_cardTop_scn),
      m_cardBottomRightPt_scn(m_cardRight_scn, m_cardBottom_scn),
      m_cardRect_scn(m_cardTopLeftPt_scn, m_cardBottomRightPt_scn),
      m_rowType(rowType)
{
    // Calc font to scene scale
    const qreal rowWidth_fnt = m_charWidth_fnt * m_charsPerRow;
    const qreal fntToScn_scale = m_useableCardWidth_scn / rowWidth_fnt;
    setScale(fntToScn_scale);

    // Calc y offset to center text
    const qreal fontHeight_scn = m_charHeight_fnt * fntToScn_scale;
    const qreal yOffset_scn = (m_titleRowHeight_scn - fontHeight_scn) / 2.0;
    setPos(m_cardLeft_scn + m_cardBorder_scn, yOffset_scn);

    if (m_rowType == RowType::Title)
    {
    }
    if (rowType == RowType::Body)
    {
    }
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