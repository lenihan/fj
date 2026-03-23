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
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    class Cursor
    {
      public:

        uint16_t year() const;
        uint16_t cardNum() const;
        uint8_t row() const;
        uint8_t col() const;

        void setYear(const uint16_t year);
        void setCardNum(const uint16_t cardNum);
        void setRow(const uint8_t row);
        void setCol(const uint8_t col);
        void setScene(QGraphicsScene* scene);

      private:
        uint16_t m_year;      // 0000–9999
        uint16_t m_cardNum;   // 0–9999
        uint8_t m_row;        // 0–10
        uint8_t m_col;        // 0–60
        QGraphicsScene* m_scene;
    };
    Cursor m_cursor{};

    using CardStack = QList<CardItem*>;
    using YearToCardStack = QMap<uint16_t, CardStack>;
    YearToCardStack m_yearToCardStack;
};
