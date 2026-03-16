#pragma once

#include <QGraphicsView>
#include <QMap>

class CardItem;
class SquareGraphicsView : public QGraphicsView
{
  public:
    SquareGraphicsView(QGraphicsScene* scene);

  protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    struct CardPosition
    {
        uint16_t year; // 0000–9999
        uint16_t page; // 0–9999
        uint8_t row;   // 0–10
        uint8_t col;   // 0–60
    };
    CardPosition m_current;

    using CardStack = QList<CardItem*>;
    using YearToCardStack = QMap<uint16_t, CardStack>;
    YearToCardStack m_yearToCardStack;
};
