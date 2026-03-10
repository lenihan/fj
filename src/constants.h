#pragma once

#include <qpoint.h>
#include <qrect.h>
#include <qtypes.h>

struct Card 
{
    static inline constexpr qreal kLeft_scn = 0.0;
    static inline constexpr qreal kRight_scn = 5.0;
    static inline constexpr qreal kTop_scn = 0.0;
    static inline constexpr qreal kBottom_scn = 3.0;
    static inline constexpr qreal kBorder_scn = 0.1;
    static inline constexpr uint8_t kNumRows = 11;
    static inline constexpr char kColor[] = "#fdf9f0";

    inline static const auto kTopLeftPt_scn = QPointF(kLeft_scn, kTop_scn);
    inline static const auto kBottomRightPt_scn = QPointF(kRight_scn, kBottom_scn);
    inline static const auto kRect_scn = QRectF(kTopLeftPt_scn, kBottomRightPt_scn);
    inline static const qreal kUseabledWidth_scn = kRect_scn.width() - (2 * kBorder_scn);
};

namespace Title
{
    inline static constexpr qreal kCharsPerRow = 30.0;
    inline static constexpr qreal kRowHeight_scn = 0.5;
    inline static constexpr char kLineColor[] = "#C9A1AE";
}

namespace Body
{
    inline static constexpr qreal kCharsPerRow = 60.0;
    inline static constexpr qreal kRowHeight_scn = 0.25;
    inline static constexpr char kLineColor[] = "#7d93eaff";
}