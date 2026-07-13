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
#pragma once

#include <QtGlobal>
#include <span>

namespace KWin::BetterWobblyWindows
{
struct WobblyParameterSet {
    qreal Stiffness;
    qreal Drag;
    qreal MoveFactor;
    qreal XTessellation;
    qreal YTessellation;
    qreal MinimumVelocity;
    qreal MaximumVelocity;
    qreal StopVelocity;
    qreal MinimumAcceleration;
    qreal MaximumAcceleration;
    qreal StopAcceleration;
};

std::span<const WobblyParameterSet> GetDefaultWobblyParameterSets();
const WobblyParameterSet &GetDefaultWobblyParameterSet(unsigned int wobblynessLevel);
} // namespace KWin::BetterWobblyWindows
