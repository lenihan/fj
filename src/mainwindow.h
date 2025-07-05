#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class SquareGraphicsView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow();

  protected:
    void wheelEvent(QWheelEvent* event) override;

  private:
    double m_desiredInches;               // Desired window size in inches
    SquareGraphicsView* m_view;
};

#endif // MAINWINDOW_H