#include "squareGraphicsView.h"
#include <QFontDatabase>
#include <QGraphicsRectItem>
#include <QTextCursor>

QGraphicsRectItem* g_blueSquare = nullptr;
QGraphicsRectItem* g_yellowSquare = nullptr;
namespace
{
const qreal PHYSICAL_SIDE_IN = 8.0;
}

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    Q_ASSERT(scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(AnchorViewCenter);
    setRenderHint(QPainter::Antialiasing);

    // Create a blue square
    g_blueSquare = scene->addRect(0.0, 0.0, 4.0, 4.0);
    g_blueSquare->setBrush(Qt::blue); // Set fill color to blue    
    g_blueSquare->setPen(QPen(QBrush(Qt::white), 0.0));
    
    // Create a yellow square
    g_yellowSquare = scene->addRect(4.0, 4.0, 4.0, 4.0);
    g_yellowSquare->setBrush(Qt::yellow); // Set fill color to blue
    g_yellowSquare->setPen(QPen(QBrush(Qt::gray), 0.0));

#if 0
    // Load font
    QFont font = getFont("Hack-Regular.ttf");

    // Text item
    if(0)
    {
        auto* textItem = new QGraphicsTextItem;
        const QString text = "1234567890123456789012345678901234567890123456789"
                             "0123456789012345678901234567890123456789012345\n"
                             "         10        20        30        40        "
                             "50        60        70        80        90";
        textItem->setPlainText(text);
        textItem->setFont(font);
        textItem->setDefaultTextColor(Qt::red);
        textItem->setPos(0, 0);
        textItem->setScale(20.0);
        m_scene->addItem(textItem);
    }
#endif    
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    // Get latest view size
    QGraphicsView::resizeEvent(event);

    // Map view to scene
    Q_ASSERT(sceneRect() == scene()->sceneRect());
    Q_ASSERT(sceneRect().topLeft() == QPointF(0.0, 0.0));
    Q_ASSERT(sceneRect().width() == sceneRect().height());
    Q_ASSERT(width() == height());
    fitInView(sceneRect());
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

    // Draw viewport border
    {
        QPainter painter(viewport());
    
        // Disable anti-aliasing for crisp lines
        painter.setRenderHint(QPainter::Antialiasing, false);
    
        // Draw a red border around the viewport
        {
            QPen pen(Qt::red);
            const int penWidth = 10;
            pen.setWidth(penWidth); 
            painter.setPen(pen);
            QRect rect = viewport()->rect();
            rect.adjust(penWidth, penWidth, -penWidth - 1, -penWidth - 1);
            painter.drawRect(rect);
        }

        // Draw a red border around the view
        {
            QPen pen(Qt::magenta);
            const int penWidth = 6;
            pen.setWidth(penWidth); 
            painter.setPen(pen);
            QRect r = rect();
            r.adjust(penWidth, penWidth, -penWidth - 1, -penWidth - 1);
            painter.drawRect(r);
        }

        // Draw a green border around the scene
        {
            QPen pen(Qt::green);
            const int penWidth = 2;
            pen.setWidth(penWidth); 
            painter.setPen(pen);
            const QRectF rect_scene = scene()->sceneRect();
            const QPointF topLeft_scene = rect_scene.topLeft();
            const QPointF bottomRight_scene = rect_scene.bottomRight();
            const QPoint topLeft_view = mapFromScene(topLeft_scene);
            const QPoint bottomRight_view = mapFromScene(bottomRight_scene);
            QRect rect_view(topLeft_view, bottomRight_view);

            rect_view.adjust(penWidth, penWidth, -penWidth - 1, -penWidth - 1);
            painter.drawRect(rect_view);
        }
    }
}
