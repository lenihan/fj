// layout.h -- physical (_in) card/row design constants for the new core.
// Separate from src/common.h (still used by the old Qt app on this branch,
// and Qt-dependent) rather than shared.
//
// Card size is a real physical target (3in x 5in), not just "however big
// the fixed-pixel atlas happens to render it" -- see PLAN_addendum.md's
// "Coordinate system (core)" section for how _in reconciles with the
// fixed-pixel atlas via a best-fit integer render scale chosen from the
// display's DPI at window-creation time.

#pragma once

#include "types.h"

namespace Card
{
inline constexpr double kWidth_in = 3.0;
inline constexpr double kHeight_in = 5.0;

inline constexpr RowCount kNumRows = 11;
inline constexpr Row kNumTitleRows = 1;
inline constexpr Row kNumBodyNavigationRows = 1;
inline constexpr RowCount kNumUserBodyRows = kNumRows - kNumTitleRows - kNumBodyNavigationRows;
} // namespace Card

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
