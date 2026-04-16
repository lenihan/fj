#pragma once

#include "common.h"
#include <QGraphicsRectItem>

class RowItem;

class CardItem : public QGraphicsRectItem
{
  public:
    CardItem(CardNum cardNum, Year year, QGraphicsItem* parent = nullptr);
    void setChar(QChar c, Row row, Col col);
    void setText(Row row, const QString& text);
    Col colPerRow(Row row) const;
    qreal rowLineY_scn(Row row) const;
    const RowItem* rowItem(Row row) const;
    RowItem* rowItem(Row row);
    RowItem* firstRow();
    RowItem* lastRow();
    void setThreadStart(CardItem* threadStart);
    CardItem* threadStart() const;
    CardNum cardNum() const;
    Year year() const;
    void setThreadPrev(CardItem* card);
    CardItem* threadPrev();
    void setThreadNext(CardItem* card);
    CardItem* threadNext();
    Row firstEditableRow() const;
    Row lastEditableRow() const;
    Col lastCol(Row row) const;
    Col firstCol(Row row) const;
    CardItem* toc();
    void setReadOnly(bool readOnly);
    bool readOnly() const;

  protected:
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value);
    void updateLastRow();
    virtual void setupVisuals() = 0;
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