/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/TraceUtilities.h"
#include "TestSupport.h"

#include <cstdlib>

#ifndef EXPECTED_TRACE_ENABLED
#error EXPECTED_TRACE_ENABLED must be defined
#endif

int main()
{
    using namespace KWin::BetterWobblyWindows;

    constexpr bool Expected = EXPECTED_TRACE_ENABLED != 0;
    const char *value = Expected ? "1" : "0";

    CHECK(::setenv("BETTER_WOBBLY_BLUR_TRACE", value, 1) == 0);
    CHECK(::setenv("BETTER_WOBBLY_TRACE_FILES", value, 1) == 0);

    CHECK(IsBlurTraceEnabled() == Expected);
    CHECK(IsTraceFileOutputEnabled() == Expected);
    CHECK(IsBlurTraceEnabled() == Expected);
    CHECK(IsTraceFileOutputEnabled() == Expected);

    return 0;
}
