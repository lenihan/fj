#include "squareGraphicsView.h"

#include <QApplication>
#include <QFontDatabase>
#include <QGraphicsRectItem>
#include <QScreen>
#include <QTextCursor>

namespace
{
const qreal PHYSICAL_SIDE_IN = 8.0;

qreal findSmallestK(qreal desiredWidth_in, QString fontFamily, qreal dpiY)
{
    QFont font(fontFamily);

    // Lambda
    auto getDeltaWidth_in = [desiredWidth_in, &font, dpiY](qreal k) {
        qreal pointSize = desiredWidth_in * 72.0 / k;    
        font.setPointSizeF(pointSize);
        QFontMetricsF metrics(font);
        const qreal actualWidth_in = metrics.averageCharWidth() / dpiY;
        const qreal delta_in = desiredWidth_in - actualWidth_in;
        return delta_in;
    };

    qreal k = 10.0;
    qreal lastValidK = k;
    qreal steps[] = {1.0, 0.1, 0.01, 0.001};
    for (qreal step : steps)
    {
        while (k > 0.0 && getDeltaWidth_in(k) >= 0)
        {
            lastValidK = k;
            k -= step;
        }
        k = lastValidK; // Revert to last non-negative k
    }
    return k;
}

}

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(parent), m_scene(scene)
{
    Q_ASSERT(m_scene);

    // Want view to always show, no scroll bars needed
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);


    //////////////////////////////////////////////////////////
    // Load font

    QFontDatabase fontDatabase;

    // Load the font from the resource
    int fontId = fontDatabase.addApplicationFont(":/fonts/Hack-Regular.ttf");
    Q_ASSERT(fontId != -1);

    // Get the font family name
    QStringList fontFamilies = fontDatabase.applicationFontFamilies(fontId);
    Q_ASSERT(!fontFamilies.isEmpty());

    m_fontFamily = fontFamilies.at(0); // e.g., "Hack"
    QFont font(m_fontFamily);

    // Verify the font is loaded correctly
    Q_ASSERT(QFontInfo(font).exactMatch());

    //////////////////////////////////////////////////////////
    // Calculate font size in points
    
    // Goal: Match Word's Hack 10 Point: 95 chars in 8in
    const qreal desiredWidth_in = PHYSICAL_SIDE_IN/95.0;

    // DPI
    QWidget* mainWindow = window();
    Q_ASSERT(mainWindow);
    QScreen* screen = mainWindow->screen();
    Q_ASSERT(screen);
    const qreal dpiX = screen->physicalDotsPerInchX(); // 132 on Surface Pro 11, 109.22 34" Dell
    const qreal dpiY = screen->physicalDotsPerInchY(); // 129 on Surface Pro 11, 109.18 34" Dell
  
    {
        const qreal k = findSmallestK(desiredWidth_in, m_fontFamily, dpiY);
        const qreal points_per_inch = 1.0/72.0;
        //const qreal pointSize = desiredWidth_in * points_per_inch / k;
        {
            //  const qreal pointSize = 13.15; // at line
            //  const qreal pointSize = 13.14; // at line
            //  const qreal pointSize = 13.13; // at line
             const qreal pointSize = 13.12; // too small by 5 chars
             // font.setPointSizeF(pointSize);
        }

        {
            // font.setPixelSize(18); // at line
            font.setPixelSize(17); // too small by 5 chars
        }
        // font.setLetterSpacing(QFont::AbsoluteSpacing, 1);
        



        auto *textItem = new QGraphicsTextItem;
        const QString text =
            "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345\n"
            "         10        20        30        40        50        60        70        80        90";


        textItem->setPlainText(text);
        textItem->setFont(font);
        textItem->setDefaultTextColor(Qt::red); 
        textItem->setPos(100, 100);
        textItem->setScale(2.0);
        m_scene->addItem(textItem);
    }
/*
    desiredWidth_in    k      actualWidth_in    Delta (desired - actual)
    0.08421            1.0    0.037             0.047
    0.08421            0.5    0.088            -0.00379
    0.08421            0.25   0.149            -0.06479 
    0.08421            0.45   0.08392           0.00029
    0.08421            0.43   0.08865          -0.0044488
    0.08421            0.44   0.08392           0.00028


    y = mx + b

    m = (k1 - k2)/(d1 - d2)
      = (1.0 - 0.5)/(0.047 - -0.00379)
      = 0.5/0.05079
      = 9.84445

    0.047 = 9.84445 * 1.0 + b
    b = 0.047 - 9.84445
    b = -9.8914

    0 = 13.231*x -9.8914
    x = 9.8914/13.231
    x = 0.747

    Start at 1,
    Try 0.9-0.0, stop at first number that has positive delta, if zero then use 0.1
    Try 0.X9-0.X0, stop at first number 


*/
    
    setRenderHint(QPainter::Antialiasing);
    setAlignment(Qt::AlignCenter);

    {
        auto blueRect = new QGraphicsRectItem();
        QPen bluePen = QPen(Qt::blue);
        blueRect->setPen(bluePen);
        blueRect->setBrush(Qt::blue);
        blueRect->setRect(500, 500, 100, 100);
        m_scene->addItem(blueRect);
    } 
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    QWidget* mainWindow = window();
    Q_ASSERT(mainWindow);
    QScreen* screen = mainWindow->screen();
    Q_ASSERT(screen);
    const qreal dpiX = screen->physicalDotsPerInchX(); // 132 on Surface Pro 11
    const qreal dpiY = screen->physicalDotsPerInchY(); // 129 on Surface Pro 11

    // Scale view so that a monitor square is perceived as an actual square
    resetTransform();
    scale(dpiX / dpiX, dpiY / dpiX);

    // Get the view's size
    const int width_px = viewport()->width();
    const int height_px = viewport()->height();

    // Use the smaller dimension to keep the scene square
    const int side_px = width_px <= height_px ? width_px : height_px;
    const qreal dpi = width_px <= height_px ? dpiX : dpiY;
    const qreal side_in = side_px / dpi;

    // Set the scene's rectangle to be square and centered
    const qreal offsetX_px = (width_px - side_px) / 2.0;
    const qreal offsetY_px = (height_px - side_px) / 2.0;
    m_scene->setSceneRect(offsetX_px, offsetY_px, side_px, side_px);

    // Update items for change to scene's position and scale
    {
        const QRectF sceneRect = m_scene->sceneRect();
        // const qreal fudge = 1.34;
        const qreal fudge = 1.00;
        const qreal scale = side_in / PHYSICAL_SIDE_IN * fudge;
        for (QGraphicsItem* item : m_scene->items())
        {
            item->setPos(sceneRect.topLeft());
            item->setScale(scale * 1.05);
        }
    }

    // Set title
    {
        const int percent = side_in / PHYSICAL_SIDE_IN * 100.0;
        const QString title =
            QString("FJ - %1\"x%1\" %2%").arg(PHYSICAL_SIDE_IN).arg(percent);
        mainWindow->setWindowTitle(title);
    }

    QGraphicsView::resizeEvent(event);
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
        painter.setPen(Qt::red);
        QRect viewportRect = viewport()->rect();
        viewportRect.adjust(
            0, 0, -1,
            -1); // Fixes issue with left/bottom not drawing on some monitors
        painter.drawRect(viewportRect);
    }

    // Draw a green border around the scene
    {
        painter.setPen(Qt::green);
        Q_ASSERT(m_scene);
        QRectF sceneRect = m_scene->sceneRect();
        sceneRect.adjust(
            0.0, 0.0, -1.0,
            -1.0); // Fixes issue with left/bottom not drawing on some monitors
        painter.drawRect(sceneRect);
    }
}