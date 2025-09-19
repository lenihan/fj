#include "squareGraphicsView.h"

#include <QApplication>
#include <QFontDatabase>
#include <QGraphicsRectItem>
#include <QResizeEvent>
#include <QScreen>
#include <QTextCursor>

namespace
{
const qreal PHYSICAL_SIDE_IN = 8.0;
}

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(parent), m_scene(scene)
{
    Q_ASSERT(m_scene);

    // Want view to always show, no scroll bars needed
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Set a minimum size to avoid collapsing
    setMinimumSize(50, 50);

    // Set size policy to expand in both directions
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Load font
    m_font = getFont("Hack-Regular.ttf");

    // Get dpi
    {
        QWidget* mainWindow = window();
        Q_ASSERT(mainWindow);
        QScreen* screen = mainWindow->screen();
        Q_ASSERT(screen);
        m_dpiX = screen->physicalDotsPerInchX(); // 132 on Surface Pro 11,
                                                 // 109.22 34" Dell
        m_dpiY = screen->physicalDotsPerInchY(); // 129 on Surface Pro 11,
                                                 // 109.18 34" Dell
    }

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

    // Set initial window size
    {
        const qreal halfSize_in = PHYSICAL_SIDE_IN / 2.0;
        const int width_px = halfSize_in * m_dpiX; 
        const int height_px = halfSize_in * m_dpiY; 
        QWidget* mainWindow = window();
        Q_ASSERT(mainWindow);
        // mainWindow->resize(width_px, height_px);
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

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    {
        QWidget* mainWindow = window();
        Q_ASSERT(mainWindow);

        const int wh = mainWindow->size().height();
        const int ww = mainWindow->size().width();

        const int eh = event->size().height();
        const int ew = event->size().width();

        const int vh = viewport()->height();
        const int vw = viewport()->width();
        int i = 0;
        i++;
        
        int size = wh < ww ? wh : ww; 
        // resize(1024, 1024);
    }
    // Ensure the widget remains a square
    // const int size = qMin(event->size().width(), event->size().height());
    // if (size > 0)
    // {
    //     resize(size, size);
    // }

#if 0
    QWidget* mainWindow = window();
    Q_ASSERT(mainWindow);
    QScreen* screen = mainWindow->screen();
    Q_ASSERT(screen);
    const qreal dpiX = screen->physicalDotsPerInchX(); // 132 on Surface Pro 11
    const qreal dpiY = screen->physicalDotsPerInchY(); // 129 on Surface Pro 11

    // Scale view so that a monitor square is perceived as an actual square
    resetTransform();
    //scale(dpiX / dpiX, dpiY / dpiX);

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

    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    const qreal sx = width_px / PHYSICAL_SIDE_IN * dpiX;
    const qreal sy = height_px / PHYSICAL_SIDE_IN * dpiY;
    //scale(sx, sy);

    // Update items for change to scene's position and scale
    {
        const QRectF sceneRect = m_scene->sceneRect();
        // const qreal fudge = 1.34;
        // for (QGraphicsItem* item : m_scene->items())
        // {
        //     item->setPos(sceneRect.topLeft());
        //     item->setScale(scale * 1.05);
        // }
    }
#endif

    updateWindowTitle();

    QGraphicsView::resizeEvent(event);
}

void SquareGraphicsView::updateWindowTitle()
{
    // Get the view's size
    const int width_px = viewport()->width();
    const int height_px = viewport()->height();

    // Use the smaller dimension to keep the scene square
    const int side_px = qMin(width_px, height_px);
    const qreal dpi = side_px == width_px ? m_dpiX : m_dpiY;
    const qreal side_in = side_px / dpi;

    // Title text
    const int percent = side_in / PHYSICAL_SIDE_IN * 100.0;
    const QString title =
        QString("FJ - %1\"x%1\" %2%").arg(PHYSICAL_SIDE_IN).arg(percent);

    // Set title
    QWidget* mainWindow = window();
    Q_ASSERT(mainWindow);
    mainWindow->setWindowTitle(title);
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