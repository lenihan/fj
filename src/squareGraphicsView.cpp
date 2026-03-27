#include "squareGraphicsView.h"
#include "cardItem.h"
#include "constants.h"
#include "rowItem.h"

#include <QResizeEvent>

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene)
    : QGraphicsView(scene)
{
    Q_ASSERT(scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(AnchorViewCenter);
    setRenderHint(QPainter::Antialiasing);

    m_cursor.m_year = 2026;
    m_cursor.m_cardNum = 0;
    m_cursor.m_row = 0;
    m_cursor.m_col = 0;

    // 3x5 Card
    // Card 1: Year w/ first/last line read-only
    // Card 2: Index w/ card read-only
    // Card 3: Reoccuring appointments for year (Birthday's, anniversaries,
    // holidays, etc.) Card 4: Future with list of Future Months links Card 5:
    // continued for rest of year Card 6: Future April Card 7: April w/ list of
    // weeks of April and links Card 8: April Week 1 Card 9: April Week 2 Card
    // 10: April Week 3 Card 11: April Week 4 Card 12: April TODO Card 13: April
    // Daily
    auto& cardStack = m_yearToCardStack[m_cursor.m_year];
    CardItem*& card = cardStack.emplaceBack(new CardItem(m_cursor.m_cardNum));
    card->setText(0, QString::number(m_cursor.m_year) + " Index");
    m_cursor.m_row = 1;
    scene->addItem(card);

    // Dummy card
    // int i = 0;
    // QStringList rowText = QStringList(11);
    // rowText[i++] = "Example Card";
    // rowText[i++] = "Typing mode:       Caps+Space";
    // rowText[i++] = "  Cursor up:       Caps+I";
    // rowText[i++] = "  Cursor left:     Caps+J";
    // rowText[i++] = "  Cursor down:     Caps+K";
    // rowText[i++] = "  Cursor right:    Caps+L";
    // rowText[i++] = "  Delete:          Shift+Backspace";
    // rowText[i++] = "  Indent:          Tab";
    // rowText[i++] = "  Unindent:        Shift+Tab";
    // rowText[i++] = "Move cursor:       Caps,I|J|K|L";
    // rowText[i++] = "                            XXX ";
    // card->setText(rowText);

    // UI
    const QRectF uiRect(UI::kRect_scn);
    const QColor uiColor("#202020");
    scene->addRect(uiRect, QPen(Qt::NoPen), QBrush(uiColor));
}

void SquareGraphicsView::drawForeground(QPainter* painter, const QRectF& rect)
{
    // Draw cursor
    QPen pen(Qt::red);
    pen.setCosmetic(true);
    pen.setWidthF(2.0);
    painter->setPen(pen);
    painter->setBrush(Qt::red);

    CardStack& cardStack = m_yearToCardStack[m_cursor.m_year];
    CardItem* card = cardStack[m_cursor.m_cardNum];
    const RowItem* rowItem = card->rowItem(m_cursor.m_row);

    const qreal rowHeight_scn = rowItem->rowHeight_scn();
    const qreal charHeight_scn = rowItem->charHeight_scn();
    const qreal charWidth_scn = rowItem->charWidth_scn();
    const qreal lineY_scn = card->rowLineY_scn(m_cursor.m_row);

    // QRectF r(
    //     m_cursor.m_col * charWidth_scn + Card::kBorder_scn, // x
    //     lineY_scn - rowHeight_scn,      // y
    //     charWidth_scn,                  // width
    //     rowHeight_scn);                 // height
    // painter->drawRect(r);

    // const int x1 = m_cursor.m_col * charWidth_scn + Card::kBorder_scn;
    // const int x2 = x1;
    // const int y1 = lineY_scn - rowHeight_scn;
    // const int y2 = lineY_scn;
    // painter->drawLine(x1, y1, x2, y2);
    const QPointF points[3] = {
        QPointF(m_cursor.m_col * charWidth_scn + Card::kBorder_scn,
                lineY_scn - (rowHeight_scn - charHeight_scn) / 2.0),
        QPointF(m_cursor.m_col * charWidth_scn + Card::kBorder_scn -
                    charWidth_scn / 2.0,
                lineY_scn),
        QPointF(m_cursor.m_col * charWidth_scn + Card::kBorder_scn +
                    charWidth_scn / 2.0,
                lineY_scn)};

    painter->drawPolygon(points, 3);
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
    ,: Cycle through card links
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

    with 9 lines for custom data, could be used for keyword search 
    Enter: Go to first card (Index: <YEAR>) 
    t: todo/completed/no todo 
    e: edit - keyboard types 
    tab/Shift-tab: indent/unindent bullet 
    b: bullet/remove bullet 
    q: query - search 
    1-9,0: Favorites 
        - Hold to set current card as favorite 
        - Tap to go to favorite 
    p: Print to PDF
    /: Help - go to help card stack
    Backspace: Go to last card
    d: Delete card/undelete card (removes from thread/index as needed, draw with strikethrough via custom paint)

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

    CardStack& cardStack = m_yearToCardStack[m_cursor.m_year];
    CardItem* card = cardStack[m_cursor.m_cardNum];

    const int k = event->key();
    m_lastKeyPress = k;
    if (k == Qt::Key_CapsLock)
    {
        qDebug() << "CapsLock";
        m_capsDown = true;
    }
    else if (k == Qt::Key_Pause)
    {
        qDebug() << "Got Pause aka capslock!!!!";
        m_capsDown = true;
    }
    else if (k == Qt::Key_Shift)
    {
        qDebug() << "Shift";
        m_shiftDown = true;
    }
    else if (event->key() == Qt::Key_Return)
    {
        m_cursor.m_row++;
        m_cursor.m_col = 0;
    }
    else if (event->key() == Qt::Key_Backspace)
    {
        if (m_cursor.m_col == 0)
        {
            // TODO: Alert user you can't backspace, only can remove characters
            // from current row noop
        }
        else
        {
            m_cursor.m_col--;
            QString t = card->text(m_cursor.m_row);
            t.remove(m_cursor.m_col, 1);
            card->setText(m_cursor.m_row, t);
        }
    }
    else if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Delete ||
             event->key() == Qt::Key_Tab)
    {
        return; // noop
    }
    else
    {
        if (m_actionMode || m_capsDown)
        {
            if (k == Qt::Key_I) 
            {   
                // TODO: if on first row of card and not on card 0, go to prev card last non-readonly row.
                m_cursor.m_row--;
            }
            else if (k == Qt::Key_K) 
            {
                // TODO: if on last row of card, go to next cards first non-readonly row
                m_cursor.m_row++;
            }
            else if (k == Qt::Key_J)
            {
                // TODO: if on col 0, go to prev non-readonly (maybe on a prev card) row's last col.
                m_cursor.m_col--;
            } 
            else if (k == Qt::Key_L) 
            {
                // TODO: if on last col, go to next avail row, col 0
                m_cursor.m_col++;
            }
            else if (k == Qt::Key_E)
            {
                m_actionMode = false;
            }
        }
        else 
        {
            if (event->text().isEmpty())
            {
                return;
            }
            QChar c = event->text()[0];
            card->setChar(c, m_cursor.m_row, m_cursor.m_col);
            m_cursor.m_col = m_cursor.m_col + 1;
            if (m_cursor.m_col >= card->colPerRow(m_cursor.m_row))
            {
                m_cursor.m_col = 0;
                m_cursor.m_row++;
            }
            if (m_cursor.m_row >= (card->userRowsPerCard()))
            {
                m_cursor.m_row = 0;
                m_cursor.m_col = 0;
                m_cursor.m_cardNum++;
                card->hide();
                CardItem*& nextCard =
                    cardStack.emplaceBack(new CardItem(m_cursor.m_cardNum));
                scene()->addItem(nextCard);
                card = nextCard;
            }
        }
    }
    // Force redraw of cursor at new location
    scene()->invalidate(QRectF(), QGraphicsScene::ForegroundLayer);

#if 1
    switch (event->key())
    {
    case Qt::Key_Space:
        qDebug() << "Space";
        break;
    case Qt::Key_Exclam:
        qDebug() << "Exclamation";
        break;
    case Qt::Key_QuoteDbl:
        qDebug() << "Double Quote";
        break;
    case Qt::Key_NumberSign:
        qDebug() << "Number Sign";
        break;
    case Qt::Key_Dollar:
        qDebug() << "Dollar";
        break;
    case Qt::Key_Percent:
        qDebug() << "Percent";
        break;
    case Qt::Key_Ampersand:
        qDebug() << "Ampersand";
        break;
    case Qt::Key_Apostrophe:
        qDebug() << "Apostrophe";
        break;
    case Qt::Key_ParenLeft:
        qDebug() << "Left Parenthesis";
        break;
    case Qt::Key_ParenRight:
        qDebug() << "Right Parenthesis";
        break;
    case Qt::Key_Asterisk:
        qDebug() << "Asterisk";
        break;
    case Qt::Key_Plus:
        qDebug() << "Plus";
        break;
    case Qt::Key_Comma:
        qDebug() << "Comma";
        break;
    case Qt::Key_Minus:
        qDebug() << "Minus";
        break;
    case Qt::Key_Period:
        qDebug() << "Period";
        break;
    case Qt::Key_Slash:
        qDebug() << "Slash";
        break;
    case Qt::Key_0:
        qDebug() << "0";
        break;
    case Qt::Key_1:
        qDebug() << "1";
        break;
    case Qt::Key_2:
        qDebug() << "2";
        break;
    case Qt::Key_3:
        qDebug() << "3";
        break;
    case Qt::Key_4:
        qDebug() << "4";
        break;
    case Qt::Key_5:
        qDebug() << "5";
        break;
    case Qt::Key_6:
        qDebug() << "6";
        break;
    case Qt::Key_7:
        qDebug() << "7";
        break;
    case Qt::Key_8:
        qDebug() << "8";
        break;
    case Qt::Key_9:
        qDebug() << "9";
        break;
    case Qt::Key_Colon:
        qDebug() << "Colon";
        break;
    case Qt::Key_Semicolon:
        qDebug() << "Semicolon";
        break;
    case Qt::Key_Less:
        qDebug() << "Less";
        break;
    case Qt::Key_Equal:
        qDebug() << "Equal";
        break;
    case Qt::Key_Greater:
        qDebug() << "Greater";
        break;
    case Qt::Key_Question:
        qDebug() << "Question";
        break;
    case Qt::Key_At:
        qDebug() << "@";
        break;
    case Qt::Key_A:
        qDebug() << "A";
        break;
    case Qt::Key_B:
        qDebug() << "B";
        break;
    case Qt::Key_C:
        qDebug() << "C";
        break;
    case Qt::Key_D:
        qDebug() << "D";
        break;
    case Qt::Key_E:
        qDebug() << "E";
        break;
    case Qt::Key_F:
        qDebug() << "F";
        break;
    case Qt::Key_G:
        qDebug() << "G";
        break;
    case Qt::Key_H:
        qDebug() << "H";
        break;
    case Qt::Key_I:
        qDebug() << "I";
        break;
    case Qt::Key_J:
        qDebug() << "J";
        break;
    case Qt::Key_K:
        qDebug() << "K";
        break;
    case Qt::Key_L:
        qDebug() << "L";
        break;
    case Qt::Key_M:
        qDebug() << "M";
        break;
    case Qt::Key_N:
        qDebug() << "N";
        break;
    case Qt::Key_O:
        qDebug() << "O";
        break;
    case Qt::Key_P:
        qDebug() << "P";
        break;
    case Qt::Key_Q:
        qDebug() << "Q";
        break;
    case Qt::Key_R:
        qDebug() << "R";
        break;
    case Qt::Key_S:
        qDebug() << "S";
        break;
    case Qt::Key_T:
        qDebug() << "T";
        break;
    case Qt::Key_U:
        qDebug() << "U";
        break;
    case Qt::Key_V:
        qDebug() << "V";
        break;
    case Qt::Key_W:
        qDebug() << "W";
        break;
    case Qt::Key_X:
        qDebug() << "X";
        break;
    case Qt::Key_Y:
        qDebug() << "Y";
        break;
    case Qt::Key_Z:
        qDebug() << "Z";
        break;
    case Qt::Key_BracketLeft:
        qDebug() << "Bracket Left";
        break;
    case Qt::Key_Backslash:
        qDebug() << "Backslash";
        break;
    case Qt::Key_BracketRight:
        qDebug() << "Bracket Right";
        break;
    case Qt::Key_AsciiCircum:
        qDebug() << "Caret";
        break;
    case Qt::Key_Underscore:
        qDebug() << "Underscore";
        break;
    case Qt::Key_QuoteLeft:
        qDebug() << "Quote Left";
        break;
    case Qt::Key_BraceLeft:
        qDebug() << "Brace Left";
        break;
    case Qt::Key_Bar:
        qDebug() << "Pipe";
        break;
    case Qt::Key_BraceRight:
        qDebug() << "Brace Right";
        break;
    case Qt::Key_AsciiTilde:
        qDebug() << "Tilde";
        break;
    case Qt::Key_Tab:
        qDebug() << "Tab";
        break;
    case Qt::Key_Backtab:
        qDebug() << "Backtab";
        break;
    case Qt::Key_Backspace:
        qDebug() << "Backspace";
        break;
    case Qt::Key_Return:
        qDebug() << "Return";
        break;
    case Qt::Key_Shift:
        qDebug() << "Shift";
        break;
        // case Qt::Key_CapsLock:
        //     qDebug() << "CapsLock";
        //     m_capsDown = true;
        //     break;
    }

    // else
    // {
    //     const QString typed = event->text();
    //     qDebug() << "Got: " << typed;
    // }
#endif
    event->accept(); // Stop propagation if desired
}

void SquareGraphicsView::keyReleaseEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_CapsLock:
        qDebug() << "CapsLock up";
        m_capsDown = false;
        if (m_lastKeyPress == Qt::Key_CapsLock)
        {
            m_actionMode = true;
        }
        break;
    case Qt::Key_Shift:
        qDebug() << "Shift up";
        m_shiftDown = false;
        break;
    case Qt::Key_Pause:
        qDebug() << "Pause up (aka capslock up)";
        m_capsDown = false;
        if (m_lastKeyPress == Qt::Key_Pause)
        {
            m_actionMode = true;
        }
        break;
    }
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    const int width_px = viewport()->width();
    const int height_px = viewport()->height();

    const qreal scene_width_in = sceneRect().width();
    const qreal scene_height_in = sceneRect().height();

    const qreal scale =
        qMin(width_px / scene_width_in, height_px / scene_height_in);
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