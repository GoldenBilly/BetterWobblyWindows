/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <cmath>
#include <cstdio>

namespace BetterWobblyWindowsTest
{
inline bool Check(bool condition, const char *expression, int line)
{
    if (!condition) {
        std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
    }
    return condition;
}

template <typename T>
bool NearlyEqual(T left, T right, T epsilon = static_cast<T>(0.0001))
{
    return std::fabs(left - right) <= epsilon;
}
} // namespace BetterWobblyWindowsTest

#define CHECK(expression) \
    do { \
        if (!BetterWobblyWindowsTest::Check(static_cast<bool>(expression), #expression, __LINE__)) { \
            return 1; \
        } \
    } while (false)
