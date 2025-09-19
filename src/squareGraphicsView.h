#include <QFont>
#include <QGraphicsView>

class SquareGraphicsView : public QGraphicsView
{
  public:
    SquareGraphicsView(QGraphicsScene* scene, QWidget* parent);

  protected:
    QFont getFont(const QString& fontFilename);
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void updateWindowTitle();

  private:
    QGraphicsScene* m_scene;
    QGraphicsTextItem* m_text;
    qreal m_dpiX;
    qreal m_dpiY;
    QFont m_font;
};
