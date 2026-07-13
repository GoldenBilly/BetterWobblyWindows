/*
    KWin - the KDE window manager
    Portions of this file are adapted from the KDE KWin Wobbly Windows effect.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later

    Official KWin Wobbly Windows source:
    https://github.com/KDE/kwin/tree/master/src/plugins/wobblywindows
    Upstream authors are credited above according to the corresponding KDE files.
*/
#include "BetterWobblyWindows/WobblyWindowsConfigStore.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"

#include <KConfigGroup>

#include <memory>

namespace KWin
{

namespace
{
std::unique_ptr<WobblyWindowsConfig> configInstance;
}

WobblyWindowsConfig::WobblyWindowsConfig(const KSharedConfigPtr &config)
    : _config(config ? config : KSharedConfig::openConfig())
{
    Read();
}

WobblyWindowsConfig *WobblyWindowsConfig::Instance(const KSharedConfigPtr &config)
{
    if (!configInstance) {
        configInstance = std::unique_ptr<WobblyWindowsConfig>(new WobblyWindowsConfig(config));
    } else if (config) {
        configInstance->_config = config;
        configInstance->Read();
    }
    return configInstance.get();
}

WobblyWindowsConfig *WobblyWindowsConfig::Self()
{
    return Instance(KSharedConfigPtr());
}

void WobblyWindowsConfig::Read()
{
    const KConfigGroup group(_config, QStringLiteral("Effect-better_wobblywindows"));
    _wobblynessLevel = group.readEntry("WobblynessLevel", 0u);
    _settings = group.readEntry("Settings", QStringLiteral("Auto"));
    _moveWobble = group.readEntry("MoveWobble", true);
    _resizeWobble = group.readEntry("ResizeWobble", true);
    _visualOppositePointFollowOnResize = group.readEntry("VisualOppositePointFollowOnResize", false);
    _visualOppositePointFollowStrengthPercent = group.readEntry("VisualOppositePointFollowStrengthPercent",
        BetterWobblyWindows::DefaultOppositePointFollowStrengthPercent);
    _advancedMode = group.readEntry("AdvancedMode", false);
    _stiffness = group.readEntry("Stiffness", 15);
    _drag = group.readEntry("Drag", 80);
    _moveFactor = group.readEntry("MoveFactor", 10);
    _xTessellation = group.readEntry("XTessellation", 20.0);
    _yTessellation = group.readEntry("YTessellation", 20.0);
    _minVelocity = group.readEntry("MinVelocity", 0.0);
    _maxVelocity = group.readEntry("MaxVelocity", 1000.0);
    _stopVelocity = group.readEntry("StopVelocity", 0.5);
    _minAcceleration = group.readEntry("MinAcceleration", 0.0);
    _maxAcceleration = group.readEntry("MaxAcceleration", 1000.0);
    _stopAcceleration = group.readEntry("StopAcceleration", 5.0);
}

unsigned int WobblyWindowsConfig::WobblynessLevel()
{
    return Self()->_wobblynessLevel;
}

QString WobblyWindowsConfig::Settings()
{
    return Self()->_settings;
}

bool WobblyWindowsConfig::MoveWobble()
{
    return Self()->_moveWobble;
}

bool WobblyWindowsConfig::ResizeWobble()
{
    return Self()->_resizeWobble;
}

bool WobblyWindowsConfig::VisualOppositePointFollowOnResize()
{
    return Self()->_visualOppositePointFollowOnResize;
}

int WobblyWindowsConfig::VisualOppositePointFollowStrengthPercent()
{
    return Self()->_visualOppositePointFollowStrengthPercent;
}

bool WobblyWindowsConfig::AdvancedMode()
{
    return Self()->_advancedMode;
}

int WobblyWindowsConfig::Stiffness()
{
    return Self()->_stiffness;
}

int WobblyWindowsConfig::Drag()
{
    return Self()->_drag;
}

int WobblyWindowsConfig::MoveFactor()
{
    return Self()->_moveFactor;
}

double WobblyWindowsConfig::XTessellation()
{
    return Self()->_xTessellation;
}

double WobblyWindowsConfig::YTessellation()
{
    return Self()->_yTessellation;
}

double WobblyWindowsConfig::MinVelocity()
{
    return Self()->_minVelocity;
}

double WobblyWindowsConfig::MaxVelocity()
{
    return Self()->_maxVelocity;
}

double WobblyWindowsConfig::StopVelocity()
{
    return Self()->_stopVelocity;
}

double WobblyWindowsConfig::MinAcceleration()
{
    return Self()->_minAcceleration;
}

double WobblyWindowsConfig::MaxAcceleration()
{
    return Self()->_maxAcceleration;
}

double WobblyWindowsConfig::StopAcceleration()
{
    return Self()->_stopAcceleration;
}

} // namespace KWin
