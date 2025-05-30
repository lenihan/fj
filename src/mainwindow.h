#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QGraphicsView>
#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow(QWidget *parent = nullptr);

  protected:
    void wheelEvent(QWheelEvent *event) override;

  private:
    QGraphicsView *view;
    QGraphicsScene *scene;
};

#endif // MAINWINDOW_H