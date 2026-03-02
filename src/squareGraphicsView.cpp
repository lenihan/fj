#include "squareGraphicsView.h"
#include <QFontDatabase>
#include <QResizeEvent>
#include <QGraphicsSimpleTextItem>

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene)
    : QGraphicsView(scene)
{
    Q_ASSERT(scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(AnchorViewCenter);
    setRenderHint(QPainter::Antialiasing);

    // Globals
    const qreal CARD_LEFT_SCN = 0.0;
    const qreal CARD_RIGHT_SCN = 5.0;
    const qreal CARD_TOP_SCN = 0.0;
    const qreal CARD_BOTTOM_SCN = 3.0;
    const QPointF CARD_TOP_LEFT_PT_SCN(CARD_LEFT_SCN, CARD_TOP_SCN);
    const QPointF CARD_BOTTOM_RIGHT_PT_SCN(CARD_RIGHT_SCN, CARD_BOTTOM_SCN);
    const QRectF CARD_RECT_SCN(CARD_TOP_LEFT_PT_SCN, CARD_BOTTOM_RIGHT_PT_SCN);
    const qreal CARD_WIDTH_SCN = CARD_RECT_SCN.width();

    const QFont FONT = getFont("Hack-Regular.ttf");
    const QFontMetricsF fm(FONT);
    const qreal CHAR_WIDTH_FNT = fm.maxWidth();
    const qreal CHAR_HEIGHT_FNT = fm.height();

    const qreal TITLE_ROW_HEIGHT_SCN = 0.5;
    const qreal BODY_ROW_HEIGHT_SCN = 0.25;

    // 3x5 Card
    {
        const QRectF cardRect_scn(CARD_TOP_LEFT_PT_SCN, CARD_BOTTOM_RIGHT_PT_SCN);
        const QColor cardColor("#fdf9f0"); 
        scene->addRect(cardRect_scn, QPen(Qt::NoPen), QBrush(cardColor));
    }
    
    // Title line
    {
        const qreal titleRow_y_scn = CARD_TOP_SCN + TITLE_ROW_HEIGHT_SCN;
        const QPointF leftPoint_scn(CARD_LEFT_SCN, titleRow_y_scn);
        const QPointF rightPoint_scn(CARD_RIGHT_SCN, titleRow_y_scn);
        const QLineF titleLine_scn(leftPoint_scn, rightPoint_scn);
        
        const QColor titleLineColor("#C9A1AE");
        QPen titleLinePen(titleLineColor);
        titleLinePen.setWidthF(3.0);
        titleLinePen.setCosmetic(true);
        
        scene->addLine(titleLine_scn, titleLinePen);
    }

    // Title text item
    QGraphicsSimpleTextItem* titleText = scene->addSimpleText("123456789012345678901234567890", FONT);
    {
        // Calc font to scene scale
        const qreal charPerRow = 29.0;
        const qreal rowWidth_fnt = CHAR_WIDTH_FNT * charPerRow;
        const qreal fntToScn_scale = CARD_WIDTH_SCN / rowWidth_fnt;
        titleText->setScale(fntToScn_scale);
        
        // Calc y offset to center text
        const qreal fontHeight_scn = CHAR_HEIGHT_FNT * fntToScn_scale;
        const qreal yOffset_scn = (TITLE_ROW_HEIGHT_SCN - fontHeight_scn) / 2.0;
        titleText->setPos(CARD_LEFT_SCN, yOffset_scn);
    }

    // Body text
    {
        // Calc font to scene scale
        const qreal charPerRow = 59.0;
        const qreal rowWidth_fnt = CHAR_WIDTH_FNT * charPerRow;
        const qreal fntToScn_scale = CARD_WIDTH_SCN / rowWidth_fnt;
        
        for( int i = 0; i < 9; ++i)
        {
            // Body line
            {
                const qreal y = (BODY_ROW_HEIGHT_SCN + TITLE_ROW_HEIGHT_SCN) + (i * BODY_ROW_HEIGHT_SCN);
                const QLineF bodyLine(0.0, y, 5.0, y);
                const QColor bodyLineColor("#7d93eaff");
                QPen bodyLinePen(bodyLineColor);
                bodyLinePen.setWidthF(3.0);
                bodyLinePen.setCosmetic(true);
                scene->addLine(bodyLine, bodyLinePen);
            }
        }
            
        for( int i = 0; i < 10; ++i)
        {
            // Body Text Item
            {
                QGraphicsSimpleTextItem* bodyText = scene->addSimpleText("123456789012345678901234567890123456789012345678901234567890", FONT);
                bodyText->setScale(fntToScn_scale);

                // Calc y offset to center text
                const qreal y = TITLE_ROW_HEIGHT_SCN + (i * BODY_ROW_HEIGHT_SCN);
                const qreal fontHeight_scn = CHAR_HEIGHT_FNT * fntToScn_scale;
                const qreal yOffset_scn = (BODY_ROW_HEIGHT_SCN - fontHeight_scn) / 2.0;
                bodyText->setPos(CARD_LEFT_SCN, y + yOffset_scn);
            }
        }
    }

    // UI
    const QRectF uiRect(0.0, 3.0, 5.0, 2.0);
    const QColor uiColor("#202020");
    scene->addRect(uiRect, QPen(Qt::NoPen), QBrush(uiColor));

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
