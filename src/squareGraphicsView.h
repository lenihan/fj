#include <QGraphicsView>

class SquareGraphicsView : public QGraphicsView
{
  public:
    SquareGraphicsView(QGraphicsScene* scene, QWidget* parent);

  protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

  private:
    QGraphicsScene* m_scene;
    QGraphicsTextItem* m_text;
    QString m_fontFamily;
};
