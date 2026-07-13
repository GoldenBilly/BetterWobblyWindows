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
#pragma once

#include <KSharedConfig>

#include <QString>

namespace KWin
{

class WobblyWindowsConfig
{
  public:
    static WobblyWindowsConfig *Instance(const KSharedConfigPtr &config);
    static WobblyWindowsConfig *Self();

    void Read();

    static unsigned int WobblynessLevel();
    static QString Settings();
    static bool MoveWobble();
    static bool ResizeWobble();
    static bool VisualOppositePointFollowOnResize();
    static int VisualOppositePointFollowStrengthPercent();
    static bool AdvancedMode();
    static int Stiffness();
    static int Drag();
    static int MoveFactor();
    static double XTessellation();
    static double YTessellation();
    static double MinVelocity();
    static double MaxVelocity();
    static double StopVelocity();
    static double MinAcceleration();
    static double MaxAcceleration();
    static double StopAcceleration();

  private:
    explicit WobblyWindowsConfig(const KSharedConfigPtr &config);

    KSharedConfigPtr _config;

    unsigned int _wobblynessLevel = 0;
    QString _settings = QStringLiteral("Auto");
    bool _moveWobble = true;
    bool _resizeWobble = true;
    bool _visualOppositePointFollowOnResize = false;
    int _visualOppositePointFollowStrengthPercent = 8;
    bool _advancedMode = false;
    int _stiffness = 15;
    int _drag = 80;
    int _moveFactor = 10;
    double _xTessellation = 20.0;
    double _yTessellation = 20.0;
    double _minVelocity = 0.0;
    double _maxVelocity = 1000.0;
    double _stopVelocity = 0.5;
    double _minAcceleration = 0.0;
    double _maxAcceleration = 1000.0;
    double _stopAcceleration = 5.0;
};

} // namespace KWin
