/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/EmbeddedBlurMath.h"
#include "TestSupport.h"

int main()
{
    using namespace BetterWobblyWindowsTest;
    using namespace KWin::BetterWobblyWindows;

    CHECK(NearlyEqual(AdvanceBlurTransition(-1.0, 1.0, 4), 0.25));
    CHECK(NearlyEqual(AdvanceBlurTransition(2.0, 0.0, 4), 0.75));
    CHECK(NearlyEqual(AdvanceBlurTransition(0.25, 2.0, 4), 0.5));
    CHECK(NearlyEqual(AdvanceBlurTransition(0.75, -1.0, 4), 0.5));
    CHECK(NearlyEqual(AdvanceBlurTransition(0.5, 0.5, 4), 0.5));
    CHECK(NearlyEqual(AdvanceBlurTransition(0.5, 0.75, 4), 0.75));
    CHECK(NearlyEqual(AdvanceBlurTransition(0.5, 0.25, 4), 0.25));
    CHECK(NearlyEqual(AdvanceBlurTransition(0.2, 0.9, 1), 0.9));
    CHECK(NearlyEqual(AdvanceBlurTransition(0.2, 0.9, 0), 0.9));
    CHECK(NearlyEqual(AdvanceBlurTransition(0.2, 0.9, -5), 0.9));

    CHECK(NearlyEqual(SmoothBlurTransition(-1.0), 0.0));
    CHECK(NearlyEqual(SmoothBlurTransition(0.0), 0.0));
    CHECK(NearlyEqual(SmoothBlurTransition(0.25), 0.15625));
    CHECK(NearlyEqual(SmoothBlurTransition(0.5), 0.5));
    CHECK(NearlyEqual(SmoothBlurTransition(0.75), 0.84375));
    CHECK(NearlyEqual(SmoothBlurTransition(1.0), 1.0));
    CHECK(NearlyEqual(SmoothBlurTransition(2.0), 1.0));

    qreal previous = SmoothBlurTransition(0.0);
    for (int step = 1; step <= 100; ++step) {
        const qreal current = SmoothBlurTransition(static_cast<qreal>(step) / 100.0);
        CHECK(current >= previous);
        CHECK(current >= 0.0);
        CHECK(current <= 1.0);
        previous = current;
    }

    return 0;
}
