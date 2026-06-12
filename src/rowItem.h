#pragma once

#include "common.h"
#include <QBrush>
#include <QFont>
#include <QGraphicsItem>
#include <QPen>

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
    qreal rowHeight_scen() const;
    qreal charHeight_scen() const;
    qreal charWidth_scen() const;
    
    void setChar(QChar c, Row row, Col col);

    void setText(const QString& text);
    QString text() const;
    
    void setReadOnly(bool readOnly);
    bool readOnly() const;

    
  private:
    static QFont font();
    static qreal fontCharHeight_font();
    static qreal fontCharWidth_font();

    const QFont kFont;
    const qreal kFontCharHeight_font;
    const qreal kFontCharWidth_font;

    const Col kColsPerRow;
    const qreal kMyRowHeight_scen;

    
    Row m_row;
    qreal m_fontToScnScale;
    QString m_text;
    bool m_readOnly{false};
    QPen m_textPen;
    QBrush m_backgroundBrush;
    QRectF m_boundingRect_locl;
};