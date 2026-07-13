/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/BetterBlurDxApi.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"
#include "TestSupport.h"

#include <string_view>

static_assert(BetterBlurDxApi::Version == 1);
static_assert(BetterBlurDxApi::RequestRole != BetterBlurDxApi::ResultRole);

static_assert(KWin::BetterWobblyWindows::MinimumBlurStrength <=
    KWin::BetterWobblyWindows::DefaultBlurStrength);
static_assert(KWin::BetterWobblyWindows::MinimumPhysicsStepsPerFrame <=
    KWin::BetterWobblyWindows::DefaultPhysicsStepsPerFrame);
static_assert(KWin::BetterWobblyWindows::DefaultPhysicsStepsPerFrame <=
    KWin::BetterWobblyWindows::MaximumPhysicsStepsPerFrame);
static_assert(KWin::BetterWobblyWindows::MinimumHandoffFrames <=
    KWin::BetterWobblyWindows::DefaultHandoffFrames);
static_assert(KWin::BetterWobblyWindows::DefaultHandoffFrames <=
    KWin::BetterWobblyWindows::MaximumHandoffFrames);
static_assert(KWin::BetterWobblyWindows::MinimumFallbackFrames <=
    KWin::BetterWobblyWindows::DefaultFallbackFrames);
static_assert(KWin::BetterWobblyWindows::DefaultFallbackFrames <=
    KWin::BetterWobblyWindows::MaximumFallbackFrames);
static_assert(KWin::BetterWobblyWindows::MaximumRetainedEmbeddedBlurBytes <=
    KWin::BetterWobblyWindows::MaximumEmbeddedBlurAllocationBytes);
static_assert(KWin::BetterWobblyWindows::MinimumCornerRadius <=
    KWin::BetterWobblyWindows::DefaultCornerRadius);
static_assert(KWin::BetterWobblyWindows::MinimumNoiseStrength <=
    KWin::BetterWobblyWindows::DefaultNoiseStrength);
static_assert(KWin::BetterWobblyWindows::DefaultNoiseStrength <=
    KWin::BetterWobblyWindows::MaximumNoiseStrength);

int main()
{
    using namespace KWin::BetterWobblyWindows;

    CHECK(std::string_view(TraceMagic) == "BETTER_WOBBLY_BETTER_BLUR_DX_API_V1");
    CHECK(BlurStrengthStepCount > 0);
    CHECK(NoiseTextureWidth > 0);
    CHECK(NoiseTextureHeight > 0);
    CHECK(QuadTriangleVertexCount == 6);
    CHECK(MaximumRetainedBlurMeshVertices > 0);

    return 0;
}
