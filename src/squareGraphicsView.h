#pragma once

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
    struct Cursor
    {
        uint16_t m_year;      // 0000–9999
        uint16_t m_cardNum;   // 0–9999
        uint8_t m_row;        // 0–10
        uint8_t m_col;        // 0–60
    };
    Cursor m_cursor{};

    using CardStack = QList<CardItem*>;
    using YearToCardStack = QMap<uint16_t, CardStack>;
    YearToCardStack m_yearToCardStack;
    bool m_capsDown{false};
    bool m_actionMode{false};
    bool m_shiftDown{false};  
    int m_lastKeyPress{0};
};
