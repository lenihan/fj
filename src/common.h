#pragma once

#include <qcolor.h>
#include <qpoint.h>
#include <qrect.h>
#include <qtypes.h>

using Year = uint16_t;       // 0000–9999
using CardNumber = uint16_t; // 0–9999
using Row = uint8_t;         // 0–10
using Col = uint8_t;         // 0–59

using CardCount = uint16_t; // Max 10,000
using RowCount = uint8_t;   // Max 11
using ColCount = uint8_t;   // Max 60

struct Screen
{
    // Coordsys is scene (_scen) which is in inches
    // Width of 3x5 card
    // Height of 3x5 card + 2" for UI
    static inline constexpr qreal kLeft_scen = 0.0;
    static inline constexpr qreal kRight_scen = 5.0;
    static inline constexpr qreal kTop_scen = 0.0;
    static inline constexpr qreal kBottom_scen = 5.0;
    static inline constexpr qreal kWidth_scen = kRight_scen - kLeft_scen;
    static inline constexpr qreal kHeight_scen = kBottom_scen - kTop_scen;
};

struct Card
{
    // Coordsys is scene (_scen) which is in inches
    // A 3" by 5" index card
    static inline constexpr qreal kLeft_scen = Screen::kLeft_scen;
    static inline constexpr qreal kRight_scen = Screen::kRight_scen;
    static inline constexpr qreal kTop_scen = Screen::kTop_scen;
    static inline constexpr qreal kBottom_scen = 3.0;
    static inline constexpr qreal kBorder_scen = 0.1;
    static inline constexpr Row kNumRows = 11;
    static inline constexpr Row kNumTitleRows = 1;
    static inline constexpr Row kNumBodyNavigationRows = 1;
    static inline constexpr Row kNumUserBodyRows = kNumRows - kNumTitleRows - kNumBodyNavigationRows;
    static inline constexpr char kColor[] = "#fdf9f0";
    static inline constexpr qreal kWidth_scen = kRight_scen - kLeft_scen;
    inline static constexpr qreal kUseabledWidth_scen = kWidth_scen - (2 * kBorder_scen);

    inline static const auto kTopLeftPt_scen = QPointF(kLeft_scen, kTop_scen);
    inline static const auto kBottomRightPt_scen = QPointF(kRight_scen, kBottom_scen);
    inline static const auto kRect_scen = QRectF(kTopLeftPt_scen, kBottomRightPt_scen);
};

struct Title
{
    inline static constexpr Col kColsPerRow = 30;
    inline static constexpr qreal kRowHeight_scen = 0.5;
    inline static constexpr char kLineColor[] = "#C9A1AE";
};

struct Body
{
    inline static constexpr Col kColsPerRow = 60;
    inline static constexpr qreal kRowHeight_scen = 0.25;
    inline static constexpr char kLineColor[] = "#7d93eaff";
};

struct Master
{
    inline static constexpr Year kYear = 0;
};

struct UI
{
    static inline constexpr qreal kLeft_scen = Screen::kLeft_scen;
    static inline constexpr qreal kRight_scen = Screen::kRight_scen;
    static inline constexpr qreal kTop_scen = Card::kBottom_scen;
    static inline constexpr qreal kBottom_scen = Screen::kBottom_scen;

    inline static const auto kTopLeftPt_scen = QPointF(kLeft_scen, kTop_scen);
    inline static const auto kBottomRightPt_scen = QPointF(kRight_scen, kBottom_scen);
    inline static const auto kRect_scen = QRectF(kTopLeftPt_scen, kBottomRightPt_scen);
    inline static constexpr char kBackgroundColor[] = "#202020";
};

struct Pen
{
    inline static constexpr qreal kDeletedWidth = 10.0;
    inline static constexpr qreal kTypingModeCursorWidth = 2.0;
};

struct Colors
{
    inline static constexpr QColor kBlack = QColor(0, 0, 0);
    inline static constexpr QColor kOrangishRed = QColor(227, 59, 36);
    inline static constexpr QColor kDarkenedColor = QColor(0, 0, 0, 50);
    inline static constexpr QColor kLightGray = QColor(163, 163, 163);
};
