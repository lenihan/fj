#pragma once

#include "common.h"
#include <QGraphicsRectItem>

class RowItem;

class CardItem : public QGraphicsRectItem
{
  public:
    enum class Type
    {
        Unknown,
        TOC,
        Content
    };

    CardItem(CardNumber cardNumber, Year year, QGraphicsItem* parent = nullptr);

    virtual Type cardType() const { return Type::Unknown; }
    bool isTOC() const; 
    bool isContent() const;
    bool isThreadStart() const;

    void setChar(QChar c, Row row, Col col);
    void setText(Row row, const QString& text);
    
    ColCount colPerRow(Row row) const;
    qreal rowLineY_scn(Row row) const;
    const RowItem* rowItem(Row row) const;
    CardNumber cardNumber() const;
    Year year() const;
    
    RowItem* rowItemAt(Row row);
    RowItem* firstRowItem();
    RowItem* lastRowItem();

    Row firstUserRow() const;
    Row lastUserRow() const;

    Col lastColAt(Row row) const;
    Col firstColAt(Row row) const;

    CardItem* tableOfContents();

    void setThreadStart(CardItem* threadStart);
    CardItem* threadStart() const;
    
    void setThreadPrev(CardItem* card);
    CardItem* threadPrev();

    void setThreadNext(CardItem* card);
    CardItem* threadNext();

    void setDeleted(bool deleted);
    bool deleted() const;

    void setReadOnly(bool readOnly);
    bool readOnly() const;

  protected:
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value);
    void setupLastRow();
    void setupBackground();
    void setupLines();

  private:
    QString threadStr(CardItem* card);
    CardItem* m_threadPrev{nullptr};
    CardItem* m_threadNext{nullptr};
    CardItem* m_threadStart{nullptr};
    QList<RowItem*> m_rows;
    CardNumber m_cardNum{0};
    Year m_year{0};
    bool m_deleted{false};
    bool m_readOnly{false};
};