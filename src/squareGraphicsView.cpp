#include "squareGraphicsView.h"
#include <QResizeEvent>
#include <QGraphicsSimpleTextItem>
#include "cardItem.h"
#include "rowItem.h"

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene)
    : QGraphicsView(scene)
{
    Q_ASSERT(scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(AnchorViewCenter);
    setRenderHint(QPainter::Antialiasing);

    // Dummy card
    int i = 0;
    m_rows[i++] = "Example Card";
    m_rows[i++] = "Typing mode:       Caps+Space";
    m_rows[i++] = "  Cursor up:       Caps+I";
    m_rows[i++] = "  Cursor left:     Caps+J";
    m_rows[i++] = "  Cursor down:     Caps+K";
    m_rows[i++] = "  Cursor right:    Caps+L";
    m_rows[i++] = "  Delete:          Shift+Backspace";
    m_rows[i++] = "  Indent:          Tab";
    m_rows[i++] = "  Unindent:        Shift+Tab";
    m_rows[i++] = "Move cursor:       Caps,I|J|K|L";
    m_rows[i++] = "                             1                            ";

    // 3x5 Card
    auto* cardItem = new CardItem();
    scene->addItem(cardItem);

    // Title text item
    auto *titleRow = new RowItem(0);
    titleRow->setText(m_rows[0]);
    scene->addItem(titleRow);

    // Body text
    {
        for( int i = 0; i < 10; ++i)
        {
            // Body Text Item
            {
                auto *bodyRow = new RowItem(i+1);
                bodyRow->setText(m_rows[i+1]);
                scene->addItem(bodyRow);
            }
        }
    }

    // UI
    const QRectF uiRect(0.0, 3.0, 5.0, 2.0);
    const QColor uiColor("#202020");
    scene->addRect(uiRect, QPen(Qt::NoPen), QBrush(uiColor));
}


void SquareGraphicsView::keyPressEvent(QKeyEvent *event)
{
    switch(event->key())
    {
        case Qt::Key_Space: qDebug() << "Space"; break;
        case Qt::Key_Exclam: qDebug() << "Exclamation"; break;
        case Qt::Key_QuoteDbl: qDebug() << "Double Quote"; break;
        case Qt::Key_NumberSign: qDebug() << "Number Sign"; break;
        case Qt::Key_Dollar: qDebug() << "Dollar"; break;
        case Qt::Key_Percent: qDebug() << "Percent"; break; 
        case Qt::Key_Ampersand: qDebug() << "Ampersand"; break;
        case Qt::Key_Apostrophe: qDebug() << "Apostrophe"; break;
        case Qt::Key_ParenLeft: qDebug() << "Left Parenthesis"; break;
        case Qt::Key_ParenRight: qDebug() << "Right Parenthesis"; break;
        case Qt::Key_Asterisk: qDebug() << "Asterisk"; break;
        case Qt::Key_Plus: qDebug() << "Plus"; break;
        case Qt::Key_Comma: qDebug() << "Comma"; break;
        case Qt::Key_Minus: qDebug() << "Minus"; break; 
        case Qt::Key_Period: qDebug() << "Period"; break;
        case Qt::Key_Slash: qDebug() << "Slash"; break;
        case Qt::Key_0: qDebug() << "0"; break;
        case Qt::Key_1: qDebug() << "1"; break;
        case Qt::Key_2: qDebug() << "2"; break;
        case Qt::Key_3: qDebug() << "3"; break;
        case Qt::Key_4: qDebug() << "4"; break;
        case Qt::Key_5: qDebug() << "5"; break;
        case Qt::Key_6: qDebug() << "6"; break;
        case Qt::Key_7: qDebug() << "7"; break;
        case Qt::Key_8: qDebug() << "8"; break;
        case Qt::Key_9: qDebug() << "9"; break;
        case Qt::Key_Colon: qDebug() << "Colon"; break;
        case Qt::Key_Semicolon: qDebug() << "Semicolon"; break;
        case Qt::Key_Less: qDebug() << "Less"; break;
        case Qt::Key_Equal: qDebug() << "Equal"; break;
        case Qt::Key_Greater: qDebug() << "Greater"; break;
        case Qt::Key_Question: qDebug() << "Question"; break;
        case Qt::Key_At: qDebug() << "@"; break;
        case Qt::Key_A: qDebug() << "A"; break;
        case Qt::Key_B: qDebug() << "B"; break;
        case Qt::Key_C: qDebug() << "C"; break;
        case Qt::Key_D: qDebug() << "D"; break;
        case Qt::Key_E: qDebug() << "E"; break;
        case Qt::Key_F: qDebug() << "F"; break;
        case Qt::Key_G: qDebug() << "G"; break;
        case Qt::Key_H: qDebug() << "H"; break;
        case Qt::Key_I: qDebug() << "I"; break;
        case Qt::Key_J: qDebug() << "J"; break;
        case Qt::Key_K: qDebug() << "K"; break;
        case Qt::Key_L: qDebug() << "L"; break;
        case Qt::Key_M: qDebug() << "M"; break;
        case Qt::Key_N: qDebug() << "N"; break;
        case Qt::Key_O: qDebug() << "O"; break;
        case Qt::Key_P: qDebug() << "P"; break;
        case Qt::Key_Q: qDebug() << "Q"; break;
        case Qt::Key_R: qDebug() << "R"; break;
        case Qt::Key_S: qDebug() << "S"; break;
        case Qt::Key_T: qDebug() << "T"; break;
        case Qt::Key_U: qDebug() << "U"; break;
        case Qt::Key_V: qDebug() << "V"; break;
        case Qt::Key_W: qDebug() << "W"; break;
        case Qt::Key_X: qDebug() << "X"; break;
        case Qt::Key_Y: qDebug() << "Y"; break;
        case Qt::Key_Z: qDebug() << "Z"; break;
        case Qt::Key_BracketLeft: qDebug() << "Bracket Left"; break;
        case Qt::Key_Backslash: qDebug() << "Backslash"; break;
        case Qt::Key_BracketRight: qDebug() << "Bracket Right"; break;
        case Qt::Key_AsciiCircum: qDebug() << "Caret"; break;
        case Qt::Key_Underscore: qDebug() << "Underscore"; break;
        case Qt::Key_QuoteLeft: qDebug() << "Quote Left"; break;
        case Qt::Key_BraceLeft: qDebug() << "Brace Left"; break;
        case Qt::Key_Bar: qDebug() << "Pipe"; break;
        case Qt::Key_BraceRight: qDebug() << "Brace Right"; break;
        case Qt::Key_AsciiTilde: qDebug() << "Tilde"; break;
        case Qt::Key_Tab: qDebug() << "Tab"; break;
        case Qt::Key_Backtab: qDebug() << "Backtab"; break;
        case Qt::Key_Backspace: qDebug() << "Backspace"; break;
        case Qt::Key_Return: qDebug() << "Return"; break;
        case Qt::Key_Shift:     qDebug() << "Shift"; break;
        case Qt::Key_CapsLock: qDebug() << "CapsLock"; break;
        

    }
    // if (event->key() == Qt::Key_CapsLock) qDebug() << "Caps"; break;
    // {
    //     qDebug() << "Caps Lock pressed";
    // } 
    // else 
    // {
    //     const QString typed = event->text();
    //     qDebug() << "Got: " << typed;
    // }
    event->accept(); // Stop propagation if desired
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
        if(0)
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
