#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QGraphicsScene;
class SquareGraphicsView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow();

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    QGraphicsScene* m_scene;
    SquareGraphicsView* m_view;
};

#endif // MAINWINDOW_H