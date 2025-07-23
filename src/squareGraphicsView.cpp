#include "squareGraphicsView.h"

#include <QApplication>
#include <QGraphicsRectItem>
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

    setRenderHint(QPainter::Antialiasing);
    setAlignment(Qt::AlignCenter);

    // Want view to always show, no scroll bars needed
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    {
        auto blueRect = new QGraphicsRectItem();
        QPen bluePen = QPen(Qt::blue);
        blueRect->setPen(bluePen);
        blueRect->setBrush(Qt::blue);
        blueRect->setRect(500, 500, 100, 100);
        m_scene->addItem(blueRect);
    }

    m_text = new QGraphicsTextItem();
    QFont font("Hack", 6);
    m_text->setFont(font);
    {
        // Get the underlying QTextDocument
        QTextDocument* doc = m_text->document();

        // Insert text segments with different font sizes
        QTextCursor cursor(doc);

        // CSS XX-Small
        QTextCharFormat xxSmallFormat;
        xxSmallFormat.setFontPointSize(6.75);
        cursor.insertText("CSS XX-Small (9px, 6.75pt)\n", xxSmallFormat);
        
        // CSS X-Small, Markdown Heading 6
        QTextCharFormat xSmallFormat;
        xSmallFormat.setFontPointSize(7.5);
        cursor.insertText("CSS X-Small, Markdown Heading 6 (10px, 7.5pt)\n", xSmallFormat);
        
        // CSS Small, Markdown Heading 5
        QTextCharFormat smallFormat;
        smallFormat.setFontPointSize(9.75);
        cursor.insertText("CSS Small, Markdown Heading 5 (13px, 9.75pt)\n", smallFormat);

        // CSS Medium, Markdown Body Text, Markdown Heading 4
        QTextCharFormat mediumFormat;
        mediumFormat.setFontPointSize(12);
        cursor.insertText("CSS Medium, Markdown Body Text, Markdown Heading 4 (16px, 12pt)\n", mediumFormat);

        // CSS Large, Markdown Heading 3
        QTextCharFormat largeFormat;
        largeFormat.setFontPointSize(13.5);
        cursor.insertText("CSS Large, Markdown Heading 3 (18px, 13.5pt)\n", largeFormat);

        // CSS X-Large, Markdown Heading 2
        QTextCharFormat xLargeFormat;
        xLargeFormat.setFontPointSize(18);
        cursor.insertText("CSS X-Large, Markdown Heading 2 (24px, 18pt)\n", xLargeFormat);
        
        // CSS XX-Large, Markdown Heading 1
        QTextCharFormat xxLargeFormat;
        xxLargeFormat.setFontPointSize(24);
        cursor.insertText("CSS XX-Large, Markdown Heading 1 (32px, 24pt)\n", xxLargeFormat);

        // CSS XXX-Large
        QTextCharFormat xxxLargeFormat;
        xxxLargeFormat.setFontPointSize(36);
        cursor.insertText("CSS XXX-Large (48px, 36pt)\n", xxxLargeFormat);

        // Maximum characters
        cursor.insertText("\nXX-Small Max chars: 190\n1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n", xxSmallFormat);
        cursor.insertText("X-Small Max chars: 170\n12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n", xSmallFormat);
        cursor.insertText("Small Max chars: 131\n12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901\n", smallFormat);
        cursor.insertText("Medium Max chars: 106\n1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456\n", mediumFormat);

        // All Characters
        cursor.insertText("\nAll Characters in Medium\nABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz`1234567890-=~!@#$%^&*()_+[]\\{}|;':\",./<>?\n", mediumFormat);

    }

    m_scene->addItem(m_text);
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    QScreen* screen = QApplication::primaryScreen();
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
        const qreal scale = side_in / PHYSICAL_SIDE_IN;
        for (QGraphicsItem* item : m_scene->items())
        {
            item->setPos(sceneRect.topLeft());
            item->setScale(scale);
        }
    }

    // Set title
    {
        QWidget* mainWindow = window();
        Q_ASSERT(mainWindow);

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
        viewportRect.adjust(0, 0, -1, -1);
        painter.drawRect(viewportRect);
    }

    // Draw a green border around the scene
    {
        painter.setPen(Qt::green);
        Q_ASSERT(m_scene);
        QRectF sceneRect = m_scene->sceneRect();
        sceneRect.adjust(0.0, 0.0, -1.0, -1.0);
        painter.drawRect(sceneRect);
    }
}