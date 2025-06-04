#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QGraphicsView>
#include <QMainWindow>
#include <QPropertyAnimation>
#include "animatedgraphicsitem.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

  protected:
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void connectScreenChanged();
    void onScreenChanged(QScreen *newScreen);
    void onDpiChanged(qreal dpi);
    
  private:
    void updateWindowSize();
    
    double desiredInches; // Desired window size in inches
    bool screenChangeConnected = false; // Track if screenChanged is connected
    QGraphicsView *view;
    QGraphicsScene *scene;
    AnimatedGraphicsItem *rectItem;
    AnimatedGraphicsItem *circleItem;
    QPropertyAnimation *rectAnimation;
    QPropertyAnimation *circleAnimation;
};

#endif // MAINWINDOW_H