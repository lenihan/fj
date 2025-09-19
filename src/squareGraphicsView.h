#include <QFont>
#include <QGraphicsView>

class SquareGraphicsView : public QGraphicsView
{
  public:
    SquareGraphicsView(QGraphicsScene* scene);

  protected:
    QFont getFont(const QString& fontFilename);
    void paintEvent(QPaintEvent* event) override;

  private:
    QGraphicsTextItem* m_text;
    qreal m_dpiX;
    qreal m_dpiY;
    QFont m_font;
};
