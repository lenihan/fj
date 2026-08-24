// textUtil.h -- small std::u32string helpers shared by CardItem/CardStack.
// fj works in char32_t codepoints throughout the core (see cardItem.h for
// why), so this is the one place that bridges an integer into that world.

#pragma once

#include <string>

inline std::u32string toU32(unsigned value)
{
    if (value == 0)
        return U"0";
    std::u32string result;
    while (value > 0)
    {
        result.insert(result.begin(), static_cast<char32_t>(U'0' + (value % 10)));
        value /= 10;
    }
    return result;
}
