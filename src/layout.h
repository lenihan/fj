// layout.h -- physical (_in) card/row design constants for the new core.
// Separate from src/common.h (still used by the old Qt app on this branch,
// and Qt-dependent) rather than shared.
//
// Card size is a real physical target, not just "however big the
// fixed-pixel atlas happens to render it" -- see PLAN.md's
// "Coordinate system (core)" section for how _in reconciles with the
// fixed-pixel atlas via a best-fit integer render scale chosen from the
// display's DPI at window-creation time.
//
// A "3x5 card" renders landscape (5in wide, 3in tall), matching the
// existing app: 60-column body rows need the wider dimension. kWidth_in/
// kHeight_in below match the old Qt Card struct's kRight_scen-kLeft_scen
// (5.0) / kBottom_scen-kTop_scen (3.0) exactly.

#pragma once

#include "types.h"

namespace Card
{
inline constexpr double kWidth_in = 5.0;
inline constexpr double kHeight_in = 3.0;

inline constexpr RowCount kNumRows = 11;
inline constexpr Row kNumTitleRows = 1;
inline constexpr Row kNumBodyNavigationRows = 1;
inline constexpr RowCount kNumUserBodyRows = kNumRows - kNumTitleRows - kNumBodyNavigationRows;
} // namespace Card

// The physical size of the (currently fictional -- see main.cpp's file
// comment) hardware fj is an emulator for. Deliberately separate from
// Card::kWidth_in/kHeight_in: the card is content shown *on* the
// monitor, not the monitor itself, and the two aren't the same shape --
// a 5x3 card centered on a 5x5 monitor leaves a margin above and below.
// kWidth_in matching Card::kWidth_in exactly is what makes that margin
// purely vertical (see main.cpp's redraw): the card already spans the
// monitor's full width.
namespace Monitor
{
inline constexpr double kWidth_in = 5.0;
inline constexpr double kHeight_in = 5.0;
} // namespace Monitor

namespace Title
{
inline constexpr Col kColsPerRow = 30;
}

namespace Body
{
inline constexpr Col kColsPerRow = 60;
}

namespace Master
{
inline constexpr Year kYear = 0;
}
