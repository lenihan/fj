#pragma once

#include "common.h"
#include <QBrush>
#include <QFont>
#include <QGraphicsItem>

// TODO: Make this a QGraphicsItem and use paint w/ drawText
// Then you can share QString for title more efficiently
// Bounding box can be static

class RowItem : public QGraphicsItem
{
  public:
  explicit RowItem(Row row, QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);

    Row row() const;
    ColCount colPerRow() const;
    qreal rowHeight_scn() const;
    qreal charHeight_scn() const;
    qreal charWidth_scn() const;
    
    void setChar(QChar c, Row row, Col col);

    void setText(const QString& text);
    QString text() const;
    
    void setReadOnly(bool readOnly);
    bool readOnly() const;

    
  private:
    static QFont font();
    static qreal fontCharHeight_fnt();
    static qreal fontCharWidth_fnt();

    const QFont kFont;
    const qreal kFontCharHeight_fnt;
    const qreal kFontCharWidth_fnt;

    const Col kColsPerRow;
    const qreal kRowHeight_scn;

    Row m_row;
    qreal m_fontToScnScale;
    QString m_text;
    bool m_readOnly{false};
    QBrush m_brush;
};