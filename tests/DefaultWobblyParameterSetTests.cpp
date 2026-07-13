/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/WobblyParameterSet.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"
#include "TestSupport.h"

#include <array>
#include <limits>

namespace
{
bool PresetsEqual(const KWin::BetterWobblyWindows::WobblyParameterSet &left,
    const KWin::BetterWobblyWindows::WobblyParameterSet &right)
{
    using BetterWobblyWindowsTest::NearlyEqual;

    return NearlyEqual(left.Stiffness, right.Stiffness) && NearlyEqual(left.Drag, right.Drag) &&
        NearlyEqual(left.MoveFactor, right.MoveFactor) && NearlyEqual(left.XTessellation, right.XTessellation) &&
        NearlyEqual(left.YTessellation, right.YTessellation) &&
        NearlyEqual(left.MinimumVelocity, right.MinimumVelocity) &&
        NearlyEqual(left.MaximumVelocity, right.MaximumVelocity) && NearlyEqual(left.StopVelocity, right.StopVelocity) &&
        NearlyEqual(left.MinimumAcceleration, right.MinimumAcceleration) &&
        NearlyEqual(left.MaximumAcceleration, right.MaximumAcceleration) &&
        NearlyEqual(left.StopAcceleration, right.StopAcceleration);
}
} // namespace

int main()
{
    using namespace KWin::BetterWobblyWindows;

    constexpr std::array<WobblyParameterSet, MaxWobblynessLevel + 1> ExpectedPresets{
        WobblyParameterSet{0.15, 0.80, 0.10, 20.0, 20.0, 0.0, 1000.0, 0.5, 0.0, 1000.0, 0.5},
        WobblyParameterSet{0.10, 0.85, 0.10, 20.0, 20.0, 0.0, 1000.0, 0.5, 0.0, 1000.0, 0.5},
        WobblyParameterSet{0.06, 0.90, 0.10, 20.0, 20.0, 0.0, 1000.0, 0.5, 0.0, 1000.0, 0.5},
        WobblyParameterSet{0.03, 0.92, 0.20, 20.0, 20.0, 0.0, 1000.0, 0.5, 0.0, 1000.0, 0.5},
        WobblyParameterSet{0.01, 0.97, 0.25, 20.0, 20.0, 0.0, 1000.0, 0.5, 0.0, 1000.0, 0.5},
    };

    const std::span<const WobblyParameterSet> presets = GetDefaultWobblyParameterSets();
    CHECK(presets.size() == ExpectedPresets.size());
    CHECK(presets.size() == static_cast<std::size_t>(MaxWobblynessLevel + 1));
    CHECK(!presets.empty());

    for (std::size_t index = 0; index < presets.size(); ++index) {
        const WobblyParameterSet &preset = presets[index];
        CHECK(PresetsEqual(preset, ExpectedPresets[index]));
        CHECK(&GetDefaultWobblyParameterSet(static_cast<unsigned int>(index)) == &preset);
        CHECK(preset.Stiffness > 0.0);
        CHECK(preset.Drag >= 0.0);
        CHECK(preset.Drag <= 1.0);
        CHECK(preset.MoveFactor > 0.0);
        CHECK(preset.XTessellation > 0.0);
        CHECK(preset.YTessellation > 0.0);
        CHECK(preset.MaximumVelocity >= preset.MinimumVelocity);
        CHECK(preset.StopVelocity >= preset.MinimumVelocity);
        CHECK(preset.StopVelocity <= preset.MaximumVelocity);
        CHECK(preset.MaximumAcceleration >= preset.MinimumAcceleration);
        CHECK(preset.StopAcceleration >= preset.MinimumAcceleration);
        CHECK(preset.StopAcceleration <= preset.MaximumAcceleration);

        if (index > 0) {
            CHECK(preset.Stiffness < presets[index - 1].Stiffness);
            CHECK(preset.Drag > presets[index - 1].Drag);
            CHECK(preset.MoveFactor >= presets[index - 1].MoveFactor);
        }
    }

    const WobblyParameterSet &last = presets.back();
    CHECK(&GetDefaultWobblyParameterSet(MaxWobblynessLevel) == &last);
    CHECK(&GetDefaultWobblyParameterSet(MaxWobblynessLevel + 1) == &last);
    CHECK(&GetDefaultWobblyParameterSet(999) == &last);
    CHECK(&GetDefaultWobblyParameterSet(std::numeric_limits<unsigned int>::max()) == &last);

    const std::span<const WobblyParameterSet> secondView = GetDefaultWobblyParameterSets();
    CHECK(secondView.data() == presets.data());
    CHECK(secondView.size() == presets.size());

    return 0;
}
