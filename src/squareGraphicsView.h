#pragma once

#include <QGraphicsView>

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
        uint16_t year = 2026; // 0000–9999
        uint16_t page = 1;    // 1–9999
        uint8_t row = 0;      // 0–10
        uint8_t col = 0;      // 0–60
    };

    CardPosition m_current;
    CardItem* m_card;
};
