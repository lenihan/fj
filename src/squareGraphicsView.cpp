#include "squareGraphicsView.h"
#include "cardItem.h"
#include "common.h"
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
    QRectF uiRect(UI::kRect_scn);
    QColor uiColor("#202020");
    scene->addRect(uiRect, QPen(Qt::NoPen), QBrush(uiColor));
}

void SquareGraphicsView::drawForeground(QPainter* painter, const QRectF& rect)
{
    m_cursor.draw(painter, rect, m_capsDown);
}

void SquareGraphicsView::keyPressEvent(QKeyEvent* event)
{
    // TODO: Disable caps modifier when app does not have focus/not active
    /*
    
    `:         Future
    1-9,0:     Favorites, +Shift to set, press to go
    -:         Future
    =:         Future
    backspace: Future
    
    tab:       Future
    q:         Future
    w:         Future
    e:         Edit - typing
    r:         Future
    t:         New TOC card
    y:         Future
    u:         Prev card
    i:         Up
    o:         Next card
    p:         Future
    [:         Future
    ]:         Future
    \:         Future
    
    a:         Future
    s:         Search
    d:         Delete/undelete card
    f:         Future
    g:         Future
    h:         Future
    j:         Left
    k:         Down
    l:         Right
    ;:         Future
    ':         Future
    enter:     Continue content card, nothing on toc
    
    z:         Future
    x:         Todo/completed/no todo
    c:         New content card
    v:         Future
    b:         Future
    n:         Future
    m:         Prev thread card
    ,:         Future
    .:         Next thread card
    /:         Help - go to help card stack
 
    space:     Future
    
    */
   event->accept(); // Stop propagation if desired
   
   int k = event->key();
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
            if (m_cursor.isCommandMode() || m_capsDown)
            {
                switch (k)
                {
                    case Qt::Key_I: m_cursor.up(); break;
                    case Qt::Key_K: m_cursor.down(); break;
                    case Qt::Key_J: m_cursor.left(); break;
                    case Qt::Key_L: m_cursor.right(); break;
                    case Qt::Key_E: m_cursor.enterTypingMode(); break;
                    case Qt::Key_U: m_cursor.prevCard(); break;
                    case Qt::Key_O: m_cursor.nextCard(); break;
                    case Qt::Key_D: m_cursor.toggleDeleteCard(); break;
                    case Qt::Key_C: m_cursor.newContent(); break;
                    case Qt::Key_T: m_cursor.newTOC(); break;
                    case Qt::Key_M: m_cursor.prevThreadCard(); break;
                    case Qt::Key_Period: m_cursor.nextThreadCard(); break;
                }
            }
            else
            {
                if (event->text().isEmpty())
                    return;
                QChar c = event->text()[0];
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
                m_cursor.enterCommandMode();
            
            // Force redraw of cursor at new location
            scene()->invalidate(QRectF(), QGraphicsScene::ForegroundLayer);
            break;
        case Qt::Key_Shift: m_shiftDown = false; break;
    }
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    int width_px = viewport()->width();
    int height_px = viewport()->height();

    qreal scene_width_in = sceneRect().width();
    qreal scene_height_in = sceneRect().height();

    qreal scale = qMin(width_px / scene_width_in, height_px / scene_height_in);
    auto transform = QTransform::fromScale(scale, scale);
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

        qreal widthScale = width_px / scene_width_in;
        qreal heightScale = height_px / scene_height_in;
        if (heightScale < widthScale)
        {
            limitingSceneIn = scene_height_in;
            limitingPx = height_px;
            dpi = dpiY;
        }

        view_side_in = limitingPx / dpi;

        int percent = qCeil(view_side_in / limitingSceneIn * 100.0);
        auto title = QString("FJ - %1\"x%2\" %3%")
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
            int penWidth = 10;
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
            int penWidth = 6;
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
            int penWidth = 2;
            pen.setWidth(penWidth);
            painter.setPen(pen);
            QRectF rect_scene = scene()->sceneRect();
            QPointF topLeft_scene = rect_scene.topLeft();
            QPointF bottomRight_scene = rect_scene.bottomRight();
            QPoint topLeft_view = mapFromScene(topLeft_scene);
            QPoint bottomRight_view = mapFromScene(bottomRight_scene);
            QRect rect_view(topLeft_view, bottomRight_view);

            rect_view.adjust(penWidth, penWidth, -penWidth - 1, -penWidth - 1);
            painter.drawRect(rect_view);
        }
    }
}