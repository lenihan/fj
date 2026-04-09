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
    uint16_t lastThreadCard() const;
    uint16_t lastCardNum() const;
    void up();
    void down();
    void left();
    void right();
    void enter();
    void backspace();
    void charTyped(QChar c);
    void nextRow();
    void nextRowCreateCard();
    void prevRow();
    void nextCard();
    void prevCard();
    void prevThreadCard();
    void nextThreadCard();
    void nextThreadCardCreateCard();
    void newCollection();
    void continueCollection();
    void draw(QPainter* painter, const QRectF& rect, const bool typing);

  private:
    void showCard(CardItem* card);
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