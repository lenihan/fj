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
    QGraphicsView::resizeEvent(event);

    // resetTransform();
    // const QRect vr = rect();
    // // const QRect vr = viewport()->rect();
    // const QRectF sr = sceneRect();  // 0,0 to 8,8
    // // scale
    // const qreal sx = (vr.width() - 1) / sr.width();
    // const qreal sy = (vr.height() - 1) / sr.height();
    // // translate
    // const qreal dx = vr.topLeft().x() - sr.topLeft().x();
    // const qreal dy = vr.topLeft().y() - sr.topLeft().y();
    // Q_ASSERT(dx == 0.0);
    // Q_ASSERT(dy == 0.0);
    // translate(vr.width()/2.0, vr.height()/2.0);
    // scale(sx, sy);
    fitInView(sceneRect());
    // translate(-vr.width()/2.0, -vr.height()/2.0);
    // translate(dx, dy);
    // Q_ASSERT(g_square->rect().topLeft() == QPointF(0.0, 0.0));
    // Q_ASSERT(g_square->rect().bottomRight() == QPointF(4.0, 4.0));

    // Q_ASSERT(viewport()->map mapFromScene(0.0, 0.0) == QPoint(0, 0));  // ASSERTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // Q_ASSERT(mapFromScene(0.0, 0.0) == viewport()->rect().topLeft());
    // Q_ASSERT(mapFromScene(8.0, 8.0) == viewport()->rect().bottomRight());

#if 0
    QRectF ssr = scene()->sceneRect();
    Q_ASSERT(sceneRect() == scene()->sceneRect());
    resetTransform();
    QPoint s = mapFromScene(0.0, 0.0);
    resetTransform();
    fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);

    // Ensure scene (0,0) maps to viewport (0,0)
    QPointF sceneTopLeft = mapFromScene(0.0, 0.0);
    if (sceneTopLeft != QPointF(0.0, 0.0)) {
        // Apply a translation to align scene (0,0) with viewport (0,0)
        translate(-sceneTopLeft.x(), -sceneTopLeft.y());
    }

    QRect afterRect = rect();
    QRect afterViewportRect = viewport()->rect();

    Q_ASSERT(g_square->rect().topLeft() == QPointF(0.0, 0.0));
    Q_ASSERT(g_square->rect().bottomRight() == QPointF(4.0, 4.0));
    QPoint t = mapFromScene(0.0, 0.0);
    Q_ASSERT(mapFromScene(0.0, 0.0) == QPoint(0, 0));  // ASSERTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    Q_ASSERT(mapFromScene(0.0, 0.0) == viewport()->rect().topLeft());
    Q_ASSERT(mapFromScene(8.0, 8.0) == viewport()->rect().bottomRight());

    // QGraphicsView::resizeEvent(event);
#endif
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
