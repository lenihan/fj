#include <QGraphicsView>

class SquareGraphicsView : public QGraphicsView
{
  public:
    SquareGraphicsView(QWidget* parent = nullptr);

  protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

  private:
    QGraphicsScene* m_scene;
    QGraphicsRectItem* m_sceneBorder;
};
