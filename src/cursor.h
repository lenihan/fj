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
    CardNum lastThreadCardNm() const;
    CardNum lastCardNum() const;
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
    void continueContent();
    void newTOC();
    void continueTOC();
    void draw(QPainter* painter, const QRectF& rect, const bool typing);

  private:
    void showCard(CardItem* card);
    QMap<Year, CardStack>  m_yearToCardStack;
    Year m_year{0};    
    Row m_row{0};      
    Col m_col{0};      

    CardItem* m_currentCard{nullptr};
    QGraphicsScene* m_scene{nullptr};
};