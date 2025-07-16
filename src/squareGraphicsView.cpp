#include "squareGraphicsView.h"

#include <QApplication>
#include <QGraphicsRectItem>
#include <QScreen>

SquareGraphicsView::SquareGraphicsView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(parent), m_scene(scene)
{
    Q_ASSERT(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setAlignment(Qt::AlignCenter);

    // Want view to always show, no scroll bars needed
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Add a QGraphicsRectItem to outline the scene rectangle
    m_sceneBorder = new QGraphicsRectItem(m_scene->sceneRect());
    QPen pen(Qt::green, 2); // Green pen with 2-pixel width
    m_sceneBorder->setPen(pen);
    m_sceneBorder->setBrush(Qt::NoBrush); // No fill, just the outline
    m_scene->addItem(m_sceneBorder);
}

void SquareGraphicsView::resizeEvent(QResizeEvent* event)
{
    QScreen *screen = QApplication::primaryScreen();
    Q_ASSERT(screen);
    const qreal dpiX = screen->physicalDotsPerInchX();
    const qreal dpiY = screen->physicalDotsPerInchY();

    // Get the view's size
    const int width_px = viewport()->width();
    const int height_px = viewport()->height();
 
    // Convert size to inches
    const qreal width_in = width_px / dpiX;
    const qreal height_in = height_px / dpiY;
    
    // Use the smaller dimension to keep the scene square
    const qreal smallerSide_in = width_in <= height_in ? width_in : height_in;
    
    // Set the scene's rectangle to be square and centered
    qreal offsetX_in = (width_in - smallerSide_in) / 2.0;
    qreal offsetY_in = (height_in - smallerSide_in) / 2.0;
    
    const int offsetX_px = offsetX_in * dpiX;
    const int offsetY_px = offsetY_in * dpiY;
    const int sceneWidth_px = smallerSide_in * dpiX;
    const int sceneHeight_px = smallerSide_in * dpiY;
    
    m_scene->setSceneRect(offsetX_px, offsetY_px, sceneWidth_px, sceneHeight_px);
    m_sceneBorder->setRect(m_scene->sceneRect());
    
    // Set title
    QWidget* mainWindow = window();
    Q_ASSERT(mainWindow);
    const qreal desired_in = 8.0;
    const int percent = smallerSide_in / desired_in * 100.0;
    const QString title = QString("fj - %1%").arg(percent);
    mainWindow->setWindowTitle(title);

    QGraphicsView::resizeEvent(event);
}


#if 0
    QScreen *screen = m_view->screen();
    Q_ASSERT(screen);
    qreal dpiX = screen->physicalDotsPerInchX();
    qreal dpiY = screen->physicalDotsPerInchY();
    QSize sizePx = screen->availableSize();
    int minSidePx = qMin(sizePx.width(), sizePx.height());
    int sidePx = minSidePx;

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QScreen *screen = m_view->screen();
    Q_ASSERT(screen);
    QSize sizePx = screen->availableSize();
    int minSidePx = qMin(size.width(), size.height());
    int sidePx = minSidePx;


    auto a = screen->physicalDotsPerInch();
    auto b = screen->physicalDotsPerInchX();
    auto c = screen->physicalDotsPerInchY();
    auto d = m_view->physicalDpiX();
    auto e = m_view->physicalDpiY();


    double value = 0;
    QString title = QString("fj - 8\" square represented by %1\" square").arg(value, 0, 'f', 1);
    setWindowTitle(title);
}

#endif


void SquareGraphicsView::paintEvent(QPaintEvent* event)
{
    // Call the base class implementation to draw the view's content
    QGraphicsView::paintEvent(event);

    // Draw a red border around the viewport
    QPainter painter(viewport());
    QPen pen(Qt::red, 1); // Red pen with 2-pixel width
    painter.setPen(pen);
    painter.drawRect(viewport()->rect().adjusted(
        1, 1, -1, -1)); // Slightly inset to avoid clipping
}