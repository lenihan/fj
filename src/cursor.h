#pragma once

#include "cardStack.h"
#include <QMap>

class QPainter;
class QRectF;
class CardItem;
class QGraphicsScene;

class Cursor
{
  public:
    Cursor(QGraphicsScene* scene);

    CardNumber lastCardNumber() const;

    QGraphicsScene* scene();

    Year year() const;
    void setYear(Year year);

    Row row() const;
    void setRow(Row row);

    Col col() const;
    void setCol(Col col);

    CardItem* currentCard();
    void setCurrentCard(CardItem* card);

    bool actionMode() const;
    void setActionMode(bool actionMode);

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

    void newContent();
    void newTOC();

    void draw(QPainter* painter, const QRectF& rect, const bool typing);

  private:
    void showCard(CardItem* card);
    void tocCurrent();
    Year m_year{0};
    Row m_row{0};
    Col m_col{0};
    CardItem* m_currentCard{nullptr};
    bool m_actionMode{true};

    QMap<Year, CardStack*> m_yearToCardStack;
    QGraphicsScene* m_scene{nullptr};
};