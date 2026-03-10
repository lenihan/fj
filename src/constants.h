#pragma once
#include <qtypes.h>

namespace Card 
{
    inline static constexpr qreal kLeft_scn = 0.0;
    inline static constexpr qreal kRight_scn = 5.0;
    inline static constexpr qreal kTop_scn = 0.0;
    inline static constexpr qreal kBottom_scn = 3.0;
    inline static constexpr qreal kBorder_scn = 0.1;
    inline static constexpr uint8_t kNumRows = 11;
    inline static constexpr char kColor[] = "#fdf9f0";
}

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