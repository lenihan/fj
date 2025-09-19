#include <QFont>
#include <QGraphicsView>

class SquareGraphicsView : public QGraphicsView
{
  public:
    SquareGraphicsView(QGraphicsScene* scene, QWidget* parent);

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    QFont getFont(const QString& fontFilename);
};
