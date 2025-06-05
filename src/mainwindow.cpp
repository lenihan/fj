#include "mainwindow.h"
#include <QApplication>
#include <QFontDatabase>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>

#include <QFile>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // Desired size in inches
    desiredInches = 5.0;

    // Create the scene
    scene = new QGraphicsScene(this);
    scene->setSceneRect(-200, -200, 400, 400); // Set scene size

    // Add a rectangle
    rectItem =
        new AnimatedGraphicsItem(AnimatedGraphicsItem::ShapeType::Rectangle,
                                 QRectF(-50, -50, 100, 50), Qt::blue);
    scene->addItem(rectItem);

    // Add a circle
    circleItem = new AnimatedGraphicsItem(
        AnimatedGraphicsItem::ShapeType::Circle, QRectF(0, 0, 50, 50), Qt::red);
    scene->addItem(circleItem);

    // Create and configure the view
    view = new QGraphicsView(scene, this);
    view->setRenderHint(QPainter::Antialiasing);
    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setCentralWidget(view);

    // Set window properties
    setWindowTitle("fj");
    updateWindowSize();

    // Connect to DPI change signal for the current screen
    connect(screen(), &QScreen::physicalDotsPerInchChanged, this,
            &MainWindow::onDpiChanged);

    // Defer connection to screenChanged until windowHandle is available
    QTimer::singleShot(0, this, &MainWindow::connectScreenChanged);

    // Load the font from the file
    QFontDatabase fontDatabase;
    int fontIdBold = fontDatabase.addApplicationFont(":/fonts/Monoid-Bold-Dollar.ttf");
    if (fontIdBold == -1) {
        qDebug() << "Failed to load font!";
    } 
    int fontIdItalic = fontDatabase.addApplicationFont(":/fonts/Monoid-Italic-Dollar.ttf");
    if (fontIdItalic == -1) {
        qDebug() << "Failed to load font!";
    }
    int fontIdRegular = fontDatabase.addApplicationFont(":/fonts/Monoid-Regular-Dollar.ttf");
    if (fontIdRegular == -1) {
        qDebug() << "Failed to load font!";
    }
    else {
        qDebug() << "Font loaded successfully, ID:" << fontIdRegular;
        qDebug() << "Loaded font families:" << fontDatabase.families();
    }
    int fontIdRetina = fontDatabase.addApplicationFont(":/fonts/Monoid-Retina-Dollar.ttf");
    if (fontIdRetina == -1) {
        qDebug() << "Failed to load font!";
    }

    // Create a QGraphicsTextItem
    QGraphicsTextItem *textItem = new QGraphicsTextItem("Hello, QGraphicsView! 0123456789 ABC");
    scene->addItem(textItem);
    textItem->setPos(-100, 0);
    QString fontFamily = "Monoid";
    QFont font(fontFamily, 12);
    textItem->setFont(font);


    // Animate the rectangle (move horizontally)
    rectAnimation = new QPropertyAnimation(rectItem, "pos");
    rectAnimation->setDuration(2000); // 2 seconds
    rectAnimation->setStartValue(QPointF(-150, -50));
    rectAnimation->setEndValue(QPointF(150, -50));
    rectAnimation->setLoopCount(-1);                        // Loop indefinitely
    rectAnimation->setEasingCurve(QEasingCurve::InOutSine); // Smooth motion
    rectAnimation->start();

    // Animate the circle (move vertically)
    circleAnimation = new QPropertyAnimation(circleItem, "pos");
    circleAnimation->setDuration(2000); // 2 seconds
    circleAnimation->setStartValue(QPointF(0, -150));
    circleAnimation->setEndValue(QPointF(0, 150));
    circleAnimation->setLoopCount(-1); // Loop indefinitely
    circleAnimation->setEasingCurve(QEasingCurve::InOutSine); // Smooth motion
    circleAnimation->start();
}

MainWindow::~MainWindow()
{
    delete rectAnimation;
    delete circleAnimation;
}

void MainWindow::connectScreenChanged()
{
    // Get the QWindow handle and connect to screenChanged signal
    QWindow *window = windowHandle();
    if (window && !screenChangeConnected)
    {
        connect(window, &QWindow::screenChanged, this,
                &MainWindow::onScreenChanged);
        screenChangeConnected = true;
        qDebug() << "Connected to screenChanged signal";
    }
}
void MainWindow::onScreenChanged(QScreen *newScreen)
{
    // Disconnect from the old screen's DPI change signal
    if (screen())
    {
        disconnect(screen(), &QScreen::physicalDotsPerInchChanged, this,
                   &MainWindow::onDpiChanged);
    }

    // Connect to the new screen's DPI change signal
    connect(newScreen, &QScreen::physicalDotsPerInchChanged, this,
            &MainWindow::onDpiChanged);

    // Update window size for the new screen's DPI
    updateWindowSize();
}

void MainWindow::onDpiChanged(qreal dpi)
{
    // Update window size when DPI changes on the current screen
    updateWindowSize();
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    // Zoom in/out with mouse wheel
    qreal scaleFactor = 1.15;
    if (event->angleDelta().y() > 0)
    {
        view->scale(scaleFactor, scaleFactor); // Zoom in
    }
    else
    {
        view->scale(1.0 / scaleFactor, 1.0 / scaleFactor); // Zoom out
    }
}

void MainWindow::updateWindowSize()
{
    // Get the current screen's PPI
    double ppi = screen()->physicalDotsPerInch();

    // Calculate pixel size
    int pixelSize = static_cast<int>(desiredInches * ppi);

    // Set window size
    setFixedSize(pixelSize, pixelSize);
}