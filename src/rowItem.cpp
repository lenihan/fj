#include "rowItem.h"
#include <QBrush>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>

RowItem::RowItem(Row row, QGraphicsItem* parent)
    : QGraphicsSimpleTextItem(parent), kFont(font()),
      kFontCharWidth_fnt(fontCharWidth_fnt()),
      kFontCharHeight_fnt(fontCharHeight_fnt()),
      kColsPerRow(row == 0 ? Title::kColsPerRow : Body::kColsPerRow),
      kRowHeight_scn(row == 0 ? Title::kRowHeight_scn : Body::kRowHeight_scn),
      m_row(row)
{
    setFont(kFont);
    setBrush(Qt::black);
    setPen(Qt::NoPen);

    // Calc font to scene scale
    qreal rowWidth_fnt = kFontCharWidth_fnt * kColsPerRow;
    m_fontToScnScale = Card::kUseabledWidth_scn / rowWidth_fnt;
    setScale(m_fontToScnScale);

    // Calc y offset to center text
    qreal y = Title::kRowHeight_scn + ((m_row - 1) * kRowHeight_scn);
    qreal fontHeight_scn = kFontCharHeight_fnt * m_fontToScnScale;
    qreal yOffset_scn = (kRowHeight_scn - fontHeight_scn) / 2.0;
    setPos(Card::kLeft_scn + Card::kBorder_scn, y + yOffset_scn);

    // Initialize row filled with spaces (empty)
    m_text = QString(kColsPerRow, ' ');
    setText(m_text);
}

Col RowItem::colPerRow() const { return kColsPerRow; }

void RowItem::setChar(QChar c, Row row, Col col)
{
    Q_ASSERT(col < kColsPerRow);
    m_text[col] = c;
    setText(m_text);
}

void RowItem::setText(const QString& text)
{
    m_text.replace(0, text.length(), text);
    Q_ASSERT(m_text.length() == kColsPerRow);
    QGraphicsSimpleTextItem::setText(text);
}

void RowItem::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    if (m_readOnly)
    {
        setBrush(Qt::lightGray);
    }
    else
    {
        setBrush(Qt::black);
    }
}

bool RowItem::readOnly() const
{
    return m_readOnly;
}

qreal RowItem::rowHeight_scn() const { return kRowHeight_scn; }

qreal RowItem::charHeight_scn() const
{
    return kFontCharHeight_fnt * m_fontToScnScale;
}

qreal RowItem::charWidth_scn() const
{
    return kFontCharWidth_fnt * m_fontToScnScale;
}

// static
QFont RowItem::font()
{
    static const QFont font = []
    {
        // Load the font from the resource
        QFontDatabase fontDatabase;
        int fontId =
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
qreal RowItem::fontCharHeight_fnt()
{
    QFontMetricsF fm(font());
    return fm.height();
}

// static
qreal RowItem::fontCharWidth_fnt()
{
    QFontMetricsF fm(font());
    return fm.maxWidth();
}
