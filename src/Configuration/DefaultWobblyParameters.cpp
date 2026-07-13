/*
    KWin - the KDE window manager
    Portions of this file are adapted from the KDE KWin Wobbly Windows effect.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later

    Official KWin Wobbly Windows source:
    https://github.com/KDE/kwin/tree/master/src/plugins/wobblywindows
    Upstream authors are credited above according to the corresponding KDE files.
*/
#include "BetterWobblyWindows/WobblyParameterSet.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"

#include <array>
#include <algorithm>

namespace KWin::BetterWobblyWindows
{
namespace
{
constexpr WobblyParameterSet ResponsivePreset{
    0.15,
    0.80,
    0.10,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

constexpr WobblyParameterSet BalancedPreset{
    0.10,
    0.85,
    0.10,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

constexpr WobblyParameterSet SoftPreset{
    0.06,
    0.90,
    0.10,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

constexpr WobblyParameterSet VerySoftPreset{
    0.03,
    0.92,
    0.20,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

constexpr WobblyParameterSet GooeyPreset{
    0.01,
    0.97,
    0.25,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

constexpr std::array<WobblyParameterSet, MaxWobblynessLevel + 1> DefaultParameterSets{
    ResponsivePreset,
    BalancedPreset,
    SoftPreset,
    VerySoftPreset,
    GooeyPreset,
};
} // namespace

std::span<const WobblyParameterSet> GetDefaultWobblyParameterSets()
{
    return DefaultParameterSets;
}

const WobblyParameterSet &GetDefaultWobblyParameterSet(unsigned int wobblynessLevel)
{
    return DefaultParameterSets[std::min<unsigned int>(wobblynessLevel, MaxWobblynessLevel)];
}
} // namespace KWin::BetterWobblyWindows
