#include "rowItem.h"
#include <QBrush>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>

RowItem::RowItem(Row row, QGraphicsItem* parent)
    : QGraphicsItem(parent), kFont(font()),
      kFontCharWidth_font(fontCharWidth_font()),
      kFontCharHeight_font(fontCharHeight_font()),
      kColsPerRow(row == 0 ? Title::kColsPerRow : Body::kColsPerRow),
      kMyRowHeight_scen(row == 0 ? Title::kRowHeight_scen : Body::kRowHeight_scen),
      m_row(row)
{
    // Calc font to scene scale
    qreal rowWidth_font = kFontCharWidth_font * kColsPerRow;
    m_fontToScnScale = Card::kUseabledWidth_scen / rowWidth_font;
    setScale(m_fontToScnScale);

    // Calc y offset to center text
    {
        qreal y = Title::kRowHeight_scen + ((m_row - 1) * kMyRowHeight_scen);
        qreal fontHeight_scen = kFontCharHeight_font * m_fontToScnScale;
        qreal yOffset_scen = (kMyRowHeight_scen - fontHeight_scen) / 2.0;
        setPos(Card::kLeft_scen + Card::kBorder_scen, y + yOffset_scen);
    }

    // Initialize row filled with spaces (empty)
    m_text = QString(kColsPerRow, ' ');
    setText(m_text);

    m_backgroundBrush = QBrush(Card::kColor);
    m_textPen = QPen(Colors::kBlack);
    
    // Bounding rect
    {
        qreal x_scen = Card::kBorder_scen;
        qreal y_scen = Title::kRowHeight_scen + ((m_row - 1) * kMyRowHeight_scen);
        qreal width_scen = Card::kUseabledWidth_scen; // TODO: Fix this so it works for title and body!!!!!!!
        qreal height_scen = kMyRowHeight_scen;
        QRectF boundingRect_scen = QRectF(x_scen, y_scen, width_scen, height_scen);
        m_boundingRect_locl = mapFromScene(boundingRect_scen).boundingRect();
    }
}

QRectF RowItem::boundingRect() const
{
    return m_boundingRect_locl;
}

void RowItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    // === Background / frame ===
    painter->setBrush(QBrush(Card::kColor, Qt::SolidPattern));
    painter->setPen(Qt::NoPen);          // or a light border if you want
    painter->drawRect(m_boundingRect_locl);

    // === Text ===
    painter->setFont(kFont);
    painter->setPen(m_textPen);          // or QPen(Qt::red) if you want red text

    // Draw text with a margin so it doesn't touch the edges
    QRectF textRect = m_boundingRect_locl.adjusted(4, 2, -4, -2);   // small padding

    painter->drawText(textRect, 
                      Qt::AlignLeft | Qt::AlignVCenter, 
                      m_text);
}

Row RowItem::row() const
{
    return m_row;
}

ColCount RowItem::colPerRow() const { return kColsPerRow; }

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
    update();
}

QString RowItem::text() const
{
    return m_text;
}

void RowItem::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    m_textPen = readOnly ? QPen(Colors::kLightGray) : QPen(Colors::kBlack);
    update();
}

bool RowItem::readOnly() const
{
    return m_readOnly;
}

qreal RowItem::rowHeight_scen() const { return kMyRowHeight_scen; }

qreal RowItem::charHeight_scen() const
{
    return kFontCharHeight_font * m_fontToScnScale;
}

qreal RowItem::charWidth_scen() const
{
    return kFontCharWidth_font * m_fontToScnScale;
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
qreal RowItem::fontCharHeight_font()
{
    QFontMetricsF fm(font());
    return fm.height();
}

// static
qreal RowItem::fontCharWidth_font()
{
    QFontMetricsF fm(font());
    return fm.maxWidth();
}
