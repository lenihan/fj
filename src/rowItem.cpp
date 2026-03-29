#include "rowItem.h"
#include <QBrush>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>

RowItem::RowItem(uint8_t row, QGraphicsItem* parent)
    : QGraphicsSimpleTextItem(parent), kFont(getFont()),
      kFontCharWidth_fnt(getFontCharWidth_fnt()),
      kFontCharHeight_fnt(getFontCharHeight_fnt()),
      kCharsPerRow(row == 0 ? Title::kCharsPerRow : Body::kCharsPerRow),
      kRowHeight_scn(row == 0 ? Title::kRowHeight_scn : Body::kRowHeight_scn),
      m_row(row)
{
    setFont(kFont);

    // setBrush(QColor(64, 64, 64));
    setPen(QPen(Qt::black));
    setPen(Qt::NoPen);
    // setPen(QColor(128, 128, 128));

    // Calc font to scene scale
    const qreal rowWidth_fnt = kFontCharWidth_fnt * kCharsPerRow;
    m_fontToScnScale = Card::kUseabledWidth_scn / rowWidth_fnt;
    setScale(m_fontToScnScale);

    // Calc y offset to center text
    const qreal y = Title::kRowHeight_scn + ((m_row - 1) * kRowHeight_scn);
    const qreal fontHeight_scn = kFontCharHeight_fnt * m_fontToScnScale;
    const qreal yOffset_scn = (kRowHeight_scn - fontHeight_scn) / 2.0;
    setPos(Card::kLeft_scn + Card::kBorder_scn, y + yOffset_scn);

    // Initialize row filled with spaces (empty)
    QString emptyRow(kCharsPerRow, ' ');
    // TODO: keep a m_text for faster updating, setText() to pass on to UI
    setText(emptyRow);
}

uint8_t RowItem::colPerRow() const { return kCharsPerRow; }

void RowItem::setText(const QString& text)
{
    QString fullRow(kCharsPerRow, ' ');
    fullRow.replace(0, text.length(), text);
    Q_ASSERT(fullRow.length() == kCharsPerRow);
    QGraphicsSimpleTextItem::setText(text);
}

qreal RowItem::rowHeight_scn() const 
{ 
    return kRowHeight_scn; 
}

qreal RowItem::charHeight_scn() const 
{ 
    return kFontCharHeight_fnt * m_fontToScnScale; 
}

qreal RowItem::charWidth_scn() const 
{ 
    return kFontCharWidth_fnt * m_fontToScnScale; 
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
