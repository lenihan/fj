#pragma once

#include <qpoint.h>
#include <qrect.h>
#include <qtypes.h>

using Year = uint16_t;          // 0000–9999
using CardNumber = uint16_t;    // 0–9999
using Row = uint8_t;            // 0–10
using Col = uint8_t;            // 0–59

using CardCount = uint16_t;     // Max 10,000
using RowCount = uint8_t;       // Max 11
using ColCount = uint8_t;       // Max 60

struct Screen
{
    // Coordsys is scene (_scn) which is in inches
    // Width of 3x5 card
    // Height of 3x5 card + 2" for UI
    static inline constexpr qreal kLeft_scn = 0.0;
    static inline constexpr qreal kRight_scn = 5.0;
    static inline constexpr qreal kTop_scn = 0.0;
    static inline constexpr qreal kBottom_scn = 5.0;
    static inline constexpr qreal kWidth_scn = kRight_scn - kLeft_scn;
    static inline constexpr qreal kHeight_scn = kBottom_scn - kTop_scn;
};

struct Card
{
    // Coordsys is scene (_scn) which is in inches
    // A 3" by 5" index card
    static inline constexpr qreal kLeft_scn = Screen::kLeft_scn;
    static inline constexpr qreal kRight_scn = Screen::kRight_scn;
    static inline constexpr qreal kTop_scn = Screen::kTop_scn;
    static inline constexpr qreal kBottom_scn = 3.0;
    static inline constexpr qreal kBorder_scn = 0.1;
    static inline constexpr Row kNumRows = 11;
    static inline constexpr Row kNumTitleRows = 1;
    static inline constexpr Row kNumBodyNavigationRows = 1;
    static inline constexpr Row kNumUserBodyRows = kNumRows - kNumTitleRows - kNumBodyNavigationRows;
    static inline constexpr char kColor[] = "#fdf9f0";

    inline static const auto kTopLeftPt_scn = QPointF(kLeft_scn, kTop_scn);
    inline static const auto kBottomRightPt_scn =
        QPointF(kRight_scn, kBottom_scn);

    inline static const auto kRect_scn =
        QRectF(kTopLeftPt_scn, kBottomRightPt_scn);

    inline static const qreal kUseabledWidth_scn =
        kRect_scn.width() - (2 * kBorder_scn);
};

struct Title
{
    inline static constexpr Col kColsPerRow = 30;
    inline static constexpr qreal kRowHeight_scn = 0.5;
    inline static constexpr char kLineColor[] = "#C9A1AE";
};

struct Body
{
    inline static constexpr Col kColsPerRow = 60;
    inline static constexpr qreal kRowHeight_scn = 0.25;
    inline static constexpr char kLineColor[] = "#7d93eaff";
};

struct Master
{
    inline static constexpr Year kYear = 0;
};

struct UI
{
    static inline constexpr qreal kLeft_scn = Screen::kLeft_scn;
    static inline constexpr qreal kRight_scn = Screen::kRight_scn;
    static inline constexpr qreal kTop_scn = Card::kBottom_scn;
    static inline constexpr qreal kBottom_scn = Screen::kBottom_scn;

    inline static const auto kTopLeftPt_scn = QPointF(kLeft_scn, kTop_scn);
    inline static const auto kBottomRightPt_scn =
        QPointF(kRight_scn, kBottom_scn);

    inline static const auto kRect_scn =
        QRectF(kTopLeftPt_scn, kBottomRightPt_scn);

    inline static constexpr char kBackgroundColor[] = "#202020";
};
