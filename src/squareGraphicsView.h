#pragma once

#include "cursor.h"
#include <QGraphicsView>
#include <QMap>


class CardItem;
class SquareGraphicsView : public QGraphicsView
{
  public:
    SquareGraphicsView(QGraphicsScene* scene);

  protected:
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    Cursor m_cursor;

    bool m_capsDown{false};
    bool m_shiftDown{false};  
    bool m_wasTypingMode{false};
    int m_lastKeyPress{0};
};
