#include "squareGraphicsView.h"
#include <QFontDatabase>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QResizeEvent>
#include <QTextCursor>
#include <QVector>

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene)
    : QGraphicsView(scene)
{
    Q_ASSERT(scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(AnchorViewCenter);
    setRenderHint(QPainter::Antialiasing);

    const QRectF spreadRect = sceneRect();
    const qreal paperWidth = spreadRect.width();
    const qreal paperHeight = spreadRect.height();
    const qreal pageWidth = paperWidth / 2.0;

    const QColor paperColor("#fdf9f0");
    const QColor dotColor("#7c7f8b");
    const QColor indicatorColor("#a5896d");

    scene->addRect(spreadRect, QPen(Qt::NoPen), QBrush(paperColor));

    auto addPage = [&](qreal xOffset, const QString& heading, int pageNumber) {
        const qreal topMargin = 6.0 / 16.0;      // 6/16 inch
        const qreal gutterMargin = 1.0 / 4.0;    // 1/4 inch
        const qreal outerMargin = 1.0 / 2.0;      // 1/2 inch
        const qreal bottomMargin = 1.0 / 2.0;     // 1/2 inch

        const bool isLeftPage = qFuzzyIsNull(xOffset);
        const qreal left = isLeftPage ? xOffset + outerMargin : xOffset + gutterMargin;
        const qreal right = isLeftPage ? xOffset + pageWidth - gutterMargin : xOffset + pageWidth - outerMargin;
        const qreal top = spreadRect.top() + topMargin;
        const qreal bottom = spreadRect.bottom() - bottomMargin;

        const int squaresWide = 24;
        const int squaresHigh = 36;
        const qreal dotSpacingX = (right - left) / squaresWide;
        const qreal dotSpacingY = (bottom - top) / squaresHigh;
        const qreal dotDiameter =
            qMin(dotSpacingX, dotSpacingY) * 0.10; // subtle bullet dot
        const qreal charHeight = dotSpacingY / 3.0;
        QBrush dotBrush(dotColor);
        for (int row = 0; row <= squaresHigh; ++row)
        {
            const qreal y = top + row * dotSpacingY;
            for (int col = 0; col <= squaresWide; ++col)
            {
                const qreal x = left + col * dotSpacingX;
                scene->addEllipse(x - dotDiameter / 2.0,
                                  y - dotDiameter / 2.0,
                                  dotDiameter,
                                  dotDiameter,
                                  QPen(Qt::NoPen),
                                  dotBrush);
            }
        }

        const qreal minSpacing = qMin(dotSpacingX, dotSpacingY);
        const qreal indicatorSpacing = minSpacing * 0.225;
        const qreal indicatorOffset =
            indicatorSpacing; // grid-to-marker gap matches marker spacing
        QBrush indicatorBrush(indicatorColor);

        const auto addIndicatorEllipse = [&](qreal cx, qreal cy) {
            scene->addEllipse(cx - dotDiameter / 2.0,
                              cy - dotDiameter / 2.0,
                              dotDiameter,
                              dotDiameter,
                              QPen(Qt::NoPen),
                              indicatorBrush);
        };

        const auto addVerticalIndicatorDots =
            [&](qreal xPos, int dotCount, bool includeTop, bool includeBottom) {
                for (int i = 0; i < dotCount; ++i)
                {
                    const qreal offset = i * indicatorSpacing;
                    if (includeTop)
                    {
                        const qreal topCenter = top - indicatorOffset - offset;
                        addIndicatorEllipse(xPos, topCenter);
                    }
                    if (includeBottom)
                    {
                        const qreal bottomCenter =
                            bottom + indicatorOffset + offset;
                        addIndicatorEllipse(xPos, bottomCenter);
                    }
                }
            };

        const auto addHorizontalIndicatorDots =
            [&](qreal yPos, int dotCount, bool includeLeft, bool includeRight) {
                for (int i = 0; i < dotCount; ++i)
                {
                    const qreal offset = i * indicatorSpacing;
                    if (includeLeft)
                    {
                        const qreal leftCenter = left - indicatorOffset - offset;
                        addIndicatorEllipse(leftCenter, yPos);
                    }
                    if (includeRight)
                    {
                        const qreal rightCenter =
                            right + indicatorOffset + offset;
                        addIndicatorEllipse(rightCenter, yPos);
                    }
                }
            };

        const auto columnToX = [&](int columnIndex) {
            return left + columnIndex * dotSpacingX;
        };
        const auto rowToY = [&](int rowIndex) {
            return top + rowIndex * dotSpacingY;
        };

        const bool includeLeftEdge = !isLeftPage;
        const bool includeRightEdge = isLeftPage;

        const int halfColumn = squaresWide / 2;
        const int halfRow = squaresHigh / 2;
        addVerticalIndicatorDots(columnToX(halfColumn), 1, false, true);
        addHorizontalIndicatorDots(
            rowToY(halfRow), 1, includeLeftEdge, includeRightEdge);

        const QVector<int> thirdColumns = { squaresWide / 3,
                                            2 * squaresWide / 3 };
        const QVector<int> thirdRows = { squaresHigh / 3,
                                         2 * squaresHigh / 3 };
        for (int column : thirdColumns)
        {
            addVerticalIndicatorDots(columnToX(column), 2, false, true);
        }
        for (int row : thirdRows)
        {
            addHorizontalIndicatorDots(
                rowToY(row), 2, includeLeftEdge, includeRightEdge);
        }

        auto setTextHeight = [&](QGraphicsTextItem* item, qreal targetHeight) {
            Q_ASSERT(item);
            const qreal baseHeight = item->boundingRect().height();
            if (baseHeight > 0.0)
            {
                const qreal scale = targetHeight / baseHeight;
                item->setScale(scale);
            }
        };

        const qreal halfIndicatorX = columnToX(halfColumn);
        const qreal bottomGridY = bottom;
        const qreal pageBottom = spreadRect.bottom();

        const qreal statusDotDiameter = dotDiameter;
        const qreal availableSpace = pageBottom - bottomGridY - statusDotDiameter / 2.0;
        const qreal spacing = availableSpace / 3.0;

        qreal numberHeight = 0.0;
        QFont pageNumberFont = getFont("Hack-Regular.ttf");
        auto* pageNumberItem =
            scene->addText(QString::number(pageNumber), pageNumberFont);
        pageNumberItem->setDefaultTextColor(Qt::darkGray);
        setTextHeight(pageNumberItem, charHeight * 2.0);
        {
            const QRectF numberRect = pageNumberItem->boundingRect();
            const qreal numberWidth =
                numberRect.width() * pageNumberItem->scale();
            numberHeight =
                numberRect.height() * pageNumberItem->scale();
            const qreal numberX = halfIndicatorX - numberWidth / 2.0;
            const qreal numberCenterY = bottomGridY + spacing + 1.0 / 8.0 - 0.03937; // moved up 1 mm
            const qreal numberY = numberCenterY - numberHeight / 2.0;
            pageNumberItem->setPos(numberX, numberY);
        }

        const qreal statusCenterX = halfIndicatorX;
        const qreal statusCenterY = bottomGridY + 2.0 * spacing + 0.11811; // moved down 3 mm
        scene->addEllipse(statusCenterX - statusDotDiameter / 2.0,
                          statusCenterY - statusDotDiameter / 2.0,
                          statusDotDiameter,
                          statusDotDiameter,
                          QPen(Qt::NoPen),
                          QBrush(dotColor));
    };

    addPage(0.0, "Left Page", 12);
    addPage(pageWidth, "Right Page", 13);
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

        const int percent = view_side_in / limitingSceneIn * 100.0;
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
