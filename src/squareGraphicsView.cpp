#include "squareGraphicsView.h"
#include "cardItem.h"
#include "constants.h"
#include "cursor.h"
#include "rowItem.h"
#include <QResizeEvent>

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene)
    : QGraphicsView(scene), m_cursor(scene)
{
    Q_ASSERT(scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(AnchorViewCenter);
    setRenderHint(QPainter::Antialiasing);

    // UI
    const QRectF uiRect(UI::kRect_scn);
    const QColor uiColor("#202020");
    scene->addRect(uiRect, QPen(Qt::NoPen), QBrush(uiColor));
}

void SquareGraphicsView::drawForeground(QPainter* painter, const QRectF& rect)
{
    m_cursor.draw(painter, rect);
}

void SquareGraphicsView::keyPressEvent(QKeyEvent* event)
{
    /*
    First card is year/index card

    i: up row
    k: down row
    j: left char
    l: right char
    u: prev card
    o: next card
    m: prev thread card
    .: next thread card
    Space: Go to selected card link
    n: Create Index card:
        - Does NOT have lines after titles
        - Cursor goes to first blank line
        - Only options:
            - Modify title
            - up/down to select a row
            - , to follow card for that row
        - Index row will update to point to new collection/row
        - Left thread points to parent index
    c: New collection card
        - Has lines
        - Title is editable on first card of collection
        - Continue to new card by pressing return until you pass last row
        - left thread points to parent iIndex

    Enter: Go to first card (Index: <YEAR>)
    t: todo/completed/no todo
    e: edit - keyboard types
    r: reading - up/down/left/right are for links, enter to go
    q: query - search
    1-9,0: Favorites
        - Hold to set current card as favorite
        - Tap to go to favorite
    p: Print to PDF
    /: Help - go to help card stack
    Backspace: Go to last card
    d: Delete card/undelete card (removes from thread/index as needed, draw with
    strikethrough via custom paint)

    ,: Future
    tab: Future
    b: Future
    ~: Future
    -: Future
    =: Future
    w: Future
    r: Future
    [: Future
    ]: Future
    \: Future
    a: Future
    g: Future
    ;: Future
    ': Future
    y: Future
    z: Future
    x: Future
    v: Future
    */
    event->accept(); // Stop propagation if desired

    const int k = event->key();
    m_lastKeyPress = k;
    switch(k)
    {
        case Qt::Key_CapsLock: m_capsDown = true; break;
        case Qt::Key_Shift: m_shiftDown = true; break;
        case Qt::Key_Return: m_cursor.enter(); break;
        case Qt::Key_Backspace: m_cursor.backspace(); break;
        case Qt::Key_Escape: 
        case Qt::Key_Delete: 
        case Qt::Key_Tab: return; break; // noop
        default:
        {
            if (m_actionMode || m_capsDown)
            {
                switch (k)
                {
                    case Qt::Key_I: m_cursor.up(); break;
                    case Qt::Key_K: m_cursor.down(); break;
                    case Qt::Key_J: m_cursor.left(); break;
                    case Qt::Key_L: m_cursor.right(); break;
                    case Qt::Key_E: m_actionMode = false; break;
                    case Qt::Key_U: m_cursor.prevCard(); break;
                    case Qt::Key_O: m_cursor.nextCard(); break;
                    case Qt::Key_C: m_cursor.newCollection(); break;
                    case Qt::Key_M: m_cursor.prevThread(); break;
                    case Qt::Key_Period: m_cursor.nextThread(); break;
                }
            }
            else
            {
                if (event->text().isEmpty())
                    return;
                const QChar c = event->text()[0];
                m_cursor.charTyped(c);
            }
        }
    }
    
    // Force redraw of cursor at new location
    scene()->invalidate(QRectF(), QGraphicsScene::ForegroundLayer);
}

void SquareGraphicsView::keyReleaseEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_CapsLock:
            m_capsDown = false;
            if (m_lastKeyPress == Qt::Key_CapsLock)
                m_actionMode = true;
            break;
        case Qt::Key_Shift: m_shiftDown = false; break;
    }
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    const int width_px = viewport()->width();
    const int height_px = viewport()->height();

    const qreal scene_width_in = sceneRect().width();
    const qreal scene_height_in = sceneRect().height();

    const qreal scale = qMin(width_px / scene_width_in, height_px / scene_height_in);
    const auto transform = QTransform::fromScale(scale, scale);
    setTransform(transform);

    // Track limiting dimension to report scale as percent of actual size
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
        qreal limitingSceneIn = scene_width_in;
        int limitingPx = width_px;
        qreal dpi = dpiX;

        const qreal widthScale = width_px / scene_width_in;
        const qreal heightScale = height_px / scene_height_in;
        if (heightScale < widthScale)
        {
            limitingSceneIn = scene_height_in;
            limitingPx = height_px;
            dpi = dpiY;
        }

        view_side_in = limitingPx / dpi;

        const int percent = qCeil(view_side_in / limitingSceneIn * 100.0);
        const auto title = QString("FJ - %1\"x%2\" %3%")
                               .arg(scene_width_in, 0, 'f', 2)
                               .arg(scene_height_in, 0, 'f', 2)
                               .arg(percent);
        setWindowTitle(title);
    }

    QGraphicsView::resizeEvent(event);
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
        if (0)
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
        if (0)
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
        if (0)
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