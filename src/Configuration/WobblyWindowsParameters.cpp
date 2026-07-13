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
#include "WobblyWindowsEffect_p.h"

namespace KWin
{
namespace
{
constexpr qreal MinimumTessellation = 1.0;
constexpr qreal MaximumTessellation = 128.0;
constexpr qreal MaximumPhysicsCoefficient = 10.0;
constexpr qreal MaximumPhysicsThreshold = 1000000.0;

qreal ClampFinite(qreal value, qreal fallback, qreal minimum, qreal maximum)
{
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minimum, maximum);
}
} // namespace

void WobblyWindowsEffect::reconfigure(ReconfigureFlags)
{
    effects->config()->reparseConfiguration();
    WobblyWindowsConfig::Self()->Read();

    const QString settingsMode = WobblyWindowsConfig::Settings();
    if (settingsMode != QStringLiteral("Custom")) {
        unsigned int wobblynessLevel = WobblyWindowsConfig::WobblynessLevel();
        if (wobblynessLevel > BetterWobblyWindows::MaxWobblynessLevel) {
            qCDebug(KWIN_WOBBLYWINDOWS) << "Wrong value for \"WobblynessLevel\" : " << wobblynessLevel;
            wobblynessLevel = BetterWobblyWindows::MaxWobblynessLevel;
        }
        SetParameterSet(BetterWobblyWindows::GetDefaultWobblyParameterSet(wobblynessLevel));

        if (WobblyWindowsConfig::AdvancedMode()) {
            _stiffness = WobblyWindowsConfig::Stiffness() / BetterWobblyWindows::PercentScale;
            _drag = WobblyWindowsConfig::Drag() / BetterWobblyWindows::PercentScale;
            _moveFactor = WobblyWindowsConfig::MoveFactor() / BetterWobblyWindows::PercentScale;
        }
    } else {
        _stiffness = WobblyWindowsConfig::Stiffness() / BetterWobblyWindows::PercentScale;
        _drag = WobblyWindowsConfig::Drag() / BetterWobblyWindows::PercentScale;
        _moveFactor = WobblyWindowsConfig::MoveFactor() / BetterWobblyWindows::PercentScale;

        _xTessellation = WobblyWindowsConfig::XTessellation();
        _yTessellation = WobblyWindowsConfig::YTessellation();

        _minVelocity = WobblyWindowsConfig::MinVelocity();
        _maxVelocity = WobblyWindowsConfig::MaxVelocity();
        _stopVelocity = WobblyWindowsConfig::StopVelocity();
        _minAcceleration = WobblyWindowsConfig::MinAcceleration();
        _maxAcceleration = WobblyWindowsConfig::MaxAcceleration();
        _stopAcceleration = WobblyWindowsConfig::StopAcceleration();
    }

    // KConfig files are user-editable. Reject NaN/Inf, negative thresholds and
    // pathological tessellation values before they reach vector sizing, division
    // or the per-frame mesh builder.
    const auto &fallback = BetterWobblyWindows::GetDefaultWobblyParameterSet(1);
    _stiffness = ClampFinite(_stiffness, fallback.Stiffness, 0.0, MaximumPhysicsCoefficient);
    _drag = ClampFinite(_drag, fallback.Drag, 0.0, MaximumPhysicsCoefficient);
    _moveFactor = ClampFinite(_moveFactor, fallback.MoveFactor, 0.0, MaximumPhysicsCoefficient);
    _xTessellation = ClampFinite(_xTessellation, fallback.XTessellation, MinimumTessellation, MaximumTessellation);
    _yTessellation = ClampFinite(_yTessellation, fallback.YTessellation, MinimumTessellation, MaximumTessellation);
    _minVelocity = ClampFinite(_minVelocity, fallback.MinimumVelocity, 0.0, MaximumPhysicsThreshold);
    _maxVelocity = ClampFinite(_maxVelocity, fallback.MaximumVelocity, _minVelocity, MaximumPhysicsThreshold);
    _stopVelocity = ClampFinite(_stopVelocity, fallback.StopVelocity, 0.0, _maxVelocity);
    _minAcceleration = ClampFinite(_minAcceleration, fallback.MinimumAcceleration, 0.0, MaximumPhysicsThreshold);
    _maxAcceleration =
        ClampFinite(_maxAcceleration, fallback.MaximumAcceleration, _minAcceleration, MaximumPhysicsThreshold);
    _stopAcceleration = ClampFinite(_stopAcceleration, fallback.StopAcceleration, 0.0, _maxAcceleration);

    _moveWobble = WobblyWindowsConfig::MoveWobble();
    _resizeWobble = WobblyWindowsConfig::ResizeWobble();
    _visualOppositePointFollowOnResize = WobblyWindowsConfig::VisualOppositePointFollowOnResize();
    const int oppositePointStrengthPercent = std::clamp(WobblyWindowsConfig::VisualOppositePointFollowStrengthPercent(),
        BetterWobblyWindows::MinimumOppositePointFollowStrengthPercent,
        BetterWobblyWindows::MaximumOppositePointFollowStrengthPercent);
    _visualOppositePointFollowStrength = oppositePointStrengthPercent / BetterWobblyWindows::PercentScale;

    ReconfigureEmbeddedBlur();

#if defined VERBOSE_MODE
    qCDebug(KWIN_WOBBLYWINDOWS) << "Parameters :\n"
                                << "grid(" << _stiffness << ", " << _drag << ", " << _moveFactor << ")\n"
                                << "velocity(" << _minVelocity << ", " << _maxVelocity << ", " << _stopVelocity << ")\n"
                                << "acceleration(" << _minAcceleration << ", " << _maxAcceleration << ", "
                                << _stopAcceleration << ")\n"
                                << "tessellation(" << _xTessellation << ", " << _yTessellation << ")";
#endif
}

void WobblyWindowsEffect::SetParameterSet(const BetterWobblyWindows::WobblyParameterSet &parameterSet)
{
    _stiffness = parameterSet.Stiffness;
    _drag = parameterSet.Drag;
    _moveFactor = parameterSet.MoveFactor;

    _xTessellation = parameterSet.XTessellation;
    _yTessellation = parameterSet.YTessellation;

    _minVelocity = parameterSet.MinimumVelocity;
    _maxVelocity = parameterSet.MaximumVelocity;
    _stopVelocity = parameterSet.StopVelocity;
    _minAcceleration = parameterSet.MinimumAcceleration;
    _maxAcceleration = parameterSet.MaximumAcceleration;
    _stopAcceleration = parameterSet.StopAcceleration;
}

void WobblyWindowsEffect::SetVelocityThreshold(qreal minVelocity)
{
    _minVelocity = minVelocity;
}

void WobblyWindowsEffect::SetMoveFactor(qreal factor)
{
    _moveFactor = factor;
}

void WobblyWindowsEffect::SetStiffness(qreal stiffness)
{
    _stiffness = stiffness;
}

void WobblyWindowsEffect::SetDrag(qreal drag)
{
    _drag = drag;
}

bool WobblyWindowsEffect::isActive() const
{
    return !_windows.isEmpty();
}

qreal WobblyWindowsEffect::Stiffness() const
{
    return _stiffness;
}

qreal WobblyWindowsEffect::Drag() const
{
    return _drag;
}

qreal WobblyWindowsEffect::MoveFactor() const
{
    return _moveFactor;
}

qreal WobblyWindowsEffect::XTessellation() const
{
    return _xTessellation;
}

qreal WobblyWindowsEffect::YTessellation() const
{
    return _yTessellation;
}

qreal WobblyWindowsEffect::MinVelocity() const
{
    return _minVelocity;
}

qreal WobblyWindowsEffect::MaxVelocity() const
{
    return _maxVelocity;
}

qreal WobblyWindowsEffect::StopVelocity() const
{
    return _stopVelocity;
}

qreal WobblyWindowsEffect::MinAcceleration() const
{
    return _minAcceleration;
}

qreal WobblyWindowsEffect::MaxAcceleration() const
{
    return _maxAcceleration;
}

qreal WobblyWindowsEffect::StopAcceleration() const
{
    return _stopAcceleration;
}

bool WobblyWindowsEffect::IsMoveWobble() const
{
    return _moveWobble;
}

bool WobblyWindowsEffect::IsResizeWobble() const
{
    return _resizeWobble;
}

bool WobblyWindowsEffect::IsVisualOppositePointFollowOnResize() const
{
    return _visualOppositePointFollowOnResize;
}

qreal WobblyWindowsEffect::VisualOppositePointFollowStrength() const
{
    return _visualOppositePointFollowStrength;
}

} // namespace KWin
