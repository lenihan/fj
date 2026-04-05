#pragma once

#include <QList>
#include <QMap>

class QPainter;
class QRectF;
class CardItem;
class QGraphicsScene;

class Cursor
{
  public:
    Cursor(QGraphicsScene* scene);
    void up();
    void down();
    void left();
    void right();
    void enter();
    void backspace();
    void charTyped(QChar c);
    bool nextRow(const bool createCard = false);
    bool prevRow();
    bool nextCard(const bool createCard = false);
    bool prevCard();
    void draw(QPainter* painter, const QRectF& rect);
    void newCollection();
    void continueCollection();
    uint16_t lastCardNum() const;
    void nextThread();
    void prevThread();

  private:
    using CardStack = QList<CardItem*>;
    using YearToCardStack = QMap<uint16_t, CardStack>;
    YearToCardStack m_yearToCardStack;
    uint16_t m_year{0};    // 0000–9999
    uint16_t m_cardNum{0}; // 0–9999
    uint8_t m_row{0};      // 0–10
    uint8_t m_col{0};      // 0–60

    CardItem* m_currentCard{nullptr};
    QGraphicsScene* m_scene{nullptr};
};