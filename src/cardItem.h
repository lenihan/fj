#include <QGraphicsRectItem>
#include <QColor>

class CardItem : public QGraphicsRectItem
{
  public:
    CardItem(QGraphicsItem *parent = nullptr);
  private:
   static constexpr qreal kCardLeft_scn = 0.0;
    static constexpr qreal kCardRight_scn = 5.0;
    static constexpr qreal kCardTop_scn = 0.0;
    static constexpr qreal kCardBottom_scn = 3.0;
    static constexpr qreal kCardBorder_scn = 0.1;
    static constexpr char kCardColor[] = "#fdf9f0";
    
    static constexpr qreal kTitleRowHeight_scn = 0.5;
    static constexpr qreal kBodyRowHeight_scn = 0.25; 
    static constexpr char kTitleLineColor[] = "#C9A1AE";
    static constexpr char kBodyLineColor[] = "#7d93eaff";


    const QPointF kCardTopLeftPt_scn;
    const QPointF kCardBottomRightPt_scn;
    const QRectF kCardRect_scn;
    const qreal kUseableCardWidth_scn =
        kCardRect_scn.width() - (2 * kCardBorder_scn);
};