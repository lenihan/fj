#include <QFont>
#include <QGraphicsSimpleTextItem>
#include <QPointF>

class RowItem : public QGraphicsSimpleTextItem
{
  public:
    enum class RowType
    {
        Title,
        Body
    };
    RowItem(RowType rowType);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
  private:
    static QFont getFont();
    static qreal getCharWidth();
    static qreal getCharHeight();
    
    const QFont m_font;
    const qreal m_charsPerRow;
    const qreal m_charWidth_fnt;
    const qreal m_charHeight_fnt;

    static constexpr qreal m_titleCharsPerRow = 30.0;
    static constexpr qreal m_bodyCharsPerRow = 60.0;
    const qreal m_titleRowHeight_scn = 0.5;
    const qreal m_bodyRowHeight_scn = 0.25;

    const qreal m_cardLeft_scn = 0.0;
    const qreal m_cardRight_scn = 5.0;
    const qreal m_cardTop_scn = 0.0;
    const qreal m_cardBottom_scn = 3.0;
    const qreal m_cardBorder_scn = 0.1;
    const QPointF m_cardTopLeftPt_scn;
    const QPointF m_cardBottomRightPt_scn;
    const QRectF m_cardRect_scn;
    const qreal m_useableCardWidth_scn = m_cardRect_scn.width() - (2 * m_cardBorder_scn);

    
    RowType m_rowType;
    qreal m_rowHeight_scn;
    uint8_t m_maxNumChars;
};