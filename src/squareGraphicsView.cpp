#include "squareGraphicsView.h"

#include <QFontDatabase>
#include <QGraphicsRectItem>
#include <QTextCursor>

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(parent), m_scene(scene)
{
    Q_ASSERT(m_scene);

    // Want view to always show, no scroll bars needed
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Load font
    m_font = getFont("Hack-Regular.ttf");

    // Text item
    {
        auto* textItem = new QGraphicsTextItem;
        const QString text = "1234567890123456789012345678901234567890123456789"
                             "0123456789012345678901234567890123456789012345\n"
                             "         10        20        30        40        "
                             "50        60        70        80        90";
        textItem->setPlainText(text);
        textItem->setFont(m_font);
        textItem->setDefaultTextColor(Qt::red);
        textItem->setPos(100, 100);
        textItem->setScale(2.0);
        m_scene->addItem(textItem);
    }

    // Rect item
    {
        auto blueRect = new QGraphicsRectItem();
        QPen bluePen = QPen(Qt::blue);
        blueRect->setPen(bluePen);
        blueRect->setBrush(Qt::blue);
        blueRect->setRect(500, 500, 100, 100);
        m_scene->addItem(blueRect);
    }
}

QFont SquareGraphicsView::getFont(const QString& fontFilename)
{
    QFont font;

    // Load the font from the resource
    QFontDatabase fontDatabase;
    const int fontId =
        fontDatabase.addApplicationFont(":/fonts/" + fontFilename);
    Q_ASSERT(fontId != -1);

    // Get the font family name
    QStringList fontFamilies = fontDatabase.applicationFontFamilies(fontId);
    Q_ASSERT(!fontFamilies.isEmpty());

    QString fontFamily = fontFamilies.at(0); // e.g., "Hack"
    font.setFamily(fontFamily);

    // Verify the font is loaded correctly
    Q_ASSERT(QFontInfo(font).exactMatch());

    return font;
}

void SquareGraphicsView::paintEvent(QPaintEvent* event)
{
    // Call the base class implementation to draw the view's content
    QGraphicsView::paintEvent(event);

    QPainter painter(viewport());

    // Disable anti-aliasing for crisp lines
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Draw a red border around the viewport
    {
        QPen pen(Qt::red);
        int penWidth = 5;
        pen.setWidth(penWidth); 
        painter.setPen(pen);
        QRect rect = viewport()->rect();
        rect.adjust(penWidth, penWidth, -penWidth - 1, -penWidth - 1);
        painter.drawRect(rect);
    }

    // Draw a green border around the scene
    {
        QPen pen(Qt::green);
        int penWidth = 2;
        pen.setWidth(penWidth); 
        painter.setPen(pen);
        Q_ASSERT(m_scene);
        QRectF rect = m_scene->sceneRect();
        rect.adjust(penWidth, penWidth, -penWidth - 1, -penWidth - 1);
        painter.drawRect(rect);
    }
}