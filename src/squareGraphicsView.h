#include <QFont>
#include <QGraphicsView>

class SquareGraphicsView : public QGraphicsView
{
  public:
    SquareGraphicsView(QGraphicsScene* scene, QWidget* parent);

  protected:
    QFont getFont(const QString& fontFilename);
    void paintEvent(QPaintEvent* event) override;

  private:
    QGraphicsScene* m_scene;
    QGraphicsTextItem* m_text;
    qreal m_dpiX;
    qreal m_dpiY;
    QFont m_font;
};
