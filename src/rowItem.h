#pragma once

#include "common.h"
#include <QFont>
#include <QGraphicsSimpleTextItem>

// TODO: Make this a QGraphicsItem and use paint w/ drawText
// Then you can share QString for title more efficiently
// Bounding box can be static

class RowItem : public QGraphicsSimpleTextItem
{
  public:
    explicit RowItem(RowNum row, QGraphicsItem* parent = nullptr);
    ColNum colPerRow() const;
    void setChar(QChar c, RowNum row, ColNum col);
    void setText(const QString& text); // Not virtual, hides base class setText
    void setReadOnly(bool readOnly);
    bool readOnly() const;
    qreal rowHeight_scn() const;
    qreal charHeight_scn() const;
    qreal charWidth_scn() const;

  private:
    static QFont font();
    static qreal fontCharHeight_fnt();
    static qreal fontCharWidth_fnt();

    const QFont kFont;
    const qreal kFontCharHeight_fnt;
    const qreal kFontCharWidth_fnt;

    const ColNum kColsPerRow;
    const qreal kRowHeight_scn;

    RowNum m_row;
    qreal m_fontToScnScale;
    QString m_text;
    bool m_readOnly{false};
};