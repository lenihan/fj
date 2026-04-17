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

    CardItem(CardNum cardNum, Year year, QGraphicsItem* parent = nullptr);

    virtual Type cardType() const { return Type::Unknown; }
    bool isTOC() const { return cardType() == Type::TOC; }
    bool isContent() const { return cardType() == Type::Content; }

    void setChar(QChar c, RowNum row, ColNum col);
    void setText(RowNum row, const QString& text);
    ColNum colPerRow(RowNum row) const;
    qreal rowLineY_scn(RowNum row) const;
    const RowItem* rowItem(RowNum row) const;
    RowItem* rowItem(RowNum row);
    RowItem* firstRowItem();
    RowItem* lastRowItem();
    void setThreadStart(CardItem* threadStart);
    CardItem* threadStart() const;
    CardNum cardNum() const;
    Year year() const;
    void setThreadPrev(CardItem* card);
    CardItem* threadPrev();
    void setThreadNext(CardItem* card);
    CardItem* threadNext();
    RowNum firstEditableRowNum() const;
    RowNum lastEditableRowNum() const;
    ColNum lastColNum(RowNum row) const;
    ColNum firstColNum(RowNum row) const;
    CardItem* TOC();
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
    CardNum m_cardNum{0};
    Year m_year{0};
    bool m_deleted{false};
    bool m_readOnly{false};
};