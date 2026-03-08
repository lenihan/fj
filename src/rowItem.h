#include <QFont>
#include <QGraphicsSimpleTextItem>
#include <QPointF>

class RowItem : public QGraphicsSimpleTextItem
{
  public:
    RowItem(uint8_t row);
    virtual void paint(QPainter* painter,
                       const QStyleOptionGraphicsItem* option,
                       QWidget* widget) override;

  private:
    static QFont getFont();
    static qreal getCharWidth();
    static qreal getCharHeight();

    const QFont kFont;
    const qreal kCharsPerRow;
    const qreal kRowHeight_scn;
    const qreal kCharWidth_fnt;
    const qreal kCharHeight_fnt;

    static constexpr qreal kTitleCharsPerRow = 30.0;
    static constexpr qreal kBodyCharsPerRow = 60.0;
    static constexpr const qreal kTitleRowHeight_scn = 0.5;
    static constexpr const qreal kBodyRowHeight_scn = 0.25;
    static constexpr const qreal kCardLeft_scn = 0.0;
    static constexpr const qreal kCardRight_scn = 5.0;
    static constexpr const qreal kCardTop_scn = 0.0;
    static constexpr const qreal kCardBottom_scn = 3.0;
    static constexpr const qreal kCardBorder_scn = 0.1;

    const QPointF kCardTopLeftPt_scn;
    const QPointF kCardBottomRightPt_scn;
    const QRectF kCardRect_scn;
    const qreal kUseableCardWidth_scn =
        kCardRect_scn.width() - (2 * kCardBorder_scn);

    uint8_t m_row;
};