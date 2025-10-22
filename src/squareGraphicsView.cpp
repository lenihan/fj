#include "squareGraphicsView.h"
#include <QFontDatabase>
#include <QGraphicsRectItem>
#include <QResizeEvent>
#include <QTextCursor>

QGraphicsRectItem* g_blueSquare = nullptr;
QGraphicsRectItem* g_yellowSquare = nullptr;

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene)
    : QGraphicsView(scene)
{
    Q_ASSERT(scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(AnchorViewCenter);
    setRenderHint(QPainter::Antialiasing);

    // Create a blue square
    const qreal sceneSize = sceneRect().width();
    const qreal halfSceneSize = sceneSize / 2.0;
    g_blueSquare = scene->addRect(0.0, 0.0, halfSceneSize, halfSceneSize);
    g_blueSquare->setBrush(Qt::blue); // Set fill color to blue
    g_blueSquare->setPen(QPen(QBrush(Qt::white), 0.0));

    // Create a yellow square
    g_yellowSquare = scene->addRect(halfSceneSize, halfSceneSize, halfSceneSize, halfSceneSize);
    g_yellowSquare->setBrush(Qt::yellow); // Set fill color to blue
    g_yellowSquare->setPen(QPen(QBrush(Qt::gray), 0.0));

#if 1

// Text item
{
        // Load font

        // MS Word Hack font pt sizes, characters per row
        //    6 Pt, 159 chars/row
        //    7 Pt, 136 chars/row
        //    8 Pt, 119 chars/row
        //   10 Pt,  95 chars/row
        //   12 Pt,  79 chars/row
        QHash<int, int> hash;
        hash[6] = 159;
        hash[7] = 136;
        hash[8] = 119;
        hash[10] = 95;
        hash[12] = 79;

        QFont font = getFont("Hack-Regular.ttf");
        const int fontSize = 6;
        font.setPointSize(fontSize);
        qreal chars_per_line = hash[fontSize];

        auto* textItem = new QGraphicsTextItem;
        const QString text6pt = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n"
                                "         10        20        30        40        50        60        70        80        90        100       110       120       130       140       150";
        const QString text7pt = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456\n"
                                "         10        20        30        40        50        60        70        80        90        100       110       120       130";
        const QString text8pt = "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n"
                                "         10        20        30        40        50        60        70        80        90        100       110";
        const QString text10pt = "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345\n"
                                 "         10        20        30        40        50        60        70        80        90";
        const QString text12pt = "1234567890123456789012345678901234567890123456789012345678901234567890123456789\n"
                                 "         10        20        30        40        50        60        70";

        textItem->setPlainText(text10pt);
        textItem->setFont(font);
        textItem->setDefaultTextColor(Qt::red);

        const int view_width_px = viewport()->rect().width();
        const qreal scene_width_in = sceneRect().width();
        const qreal view_to_scene_scale = scene_width_in / view_width_px;
        QRectF r1 = textItem->boundingRect();
        qreal fudge = view_width_px/r1.width();
        const qreal scale = view_to_scene_scale * fudge;
        textItem->setScale(scale);
        
        textItem->setPos(0, 0);
        scene->addItem(textItem);
    }
#endif
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    const int width_px = viewport()->width();
    const int height_px = viewport()->height();

    Q_ASSERT(sceneRect().width() == sceneRect().height());
    const qreal scene_side_in = sceneRect().width();

    // Use the smaller dimension to maintain square aspect ratio
    const qreal scale = qMin(width_px, height_px) / scene_side_in;
    const auto transform = QTransform::fromScale(scale, scale);
    setTransform(transform);

    // Calculate square side in inches
    qreal view_side_in;
    {
        // Get dpi
        qreal dpiX = 0.0;
        qreal dpiY = 0.0;
        {
            QWidget* mainWindow = window();
            Q_ASSERT(mainWindow);
            QScreen* screen = mainWindow->screen();
            Q_ASSERT(screen);
            dpiX = screen->physicalDotsPerInchX(); // 132 on Surface Pro 11,
                                                   // 109.22 34" Dell
            dpiY = screen->physicalDotsPerInchY(); // 129 on Surface Pro 11,
                                                   // 109.18 34" Dell
        }
        
        // Set side in pixels, dpi based on shorter width/height
        int view_side_px = 0;
        qreal dpi = 0;
        if (width_px < height_px)
        {
            view_side_px = width_px;
            dpi = dpiX;
        }
        else
        {
            view_side_px = height_px;
            dpi = dpiY;
        }

        // Calculate side in inches
        view_side_in = view_side_px / dpi;
    }

    // Set title with percent of actual size
    {
        const int percent = view_side_in / scene_side_in * 100.0;
        const auto title =
            QString("FJ - %1\"x%1\" %2%").arg(scene_side_in).arg(percent);
        setWindowTitle(title);
    }

    QGraphicsView::resizeEvent(event);
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
        if(0)
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
        if(0)
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
        if(1)
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
