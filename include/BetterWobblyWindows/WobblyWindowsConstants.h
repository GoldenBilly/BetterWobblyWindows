/*
    KWin - the KDE window manager
    Portions of this file are adapted from the KDE KWin Wobbly Windows effect.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>
    Portions of this file are adapted from Better Blur DX and the KWin Blur effect.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
    SPDX-FileCopyrightText: 2024-2026 Better Blur DX contributors
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later

    Official KWin Wobbly Windows source:
    https://github.com/KDE/kwin/tree/master/src/plugins/wobblywindows
    Upstream authors are credited above according to the corresponding KDE files.

    Better Blur DX source: https://github.com/xarblu/kwin-effects-better-blur-dx
    Reference snapshot: 35451011b48bd01c2afa1a58ceeac46197147b81
    See AUTHORS.md and NOTICE.md for the complete provenance record.
*/
#pragma once

#include <QtGlobal>

#include <cstddef>

namespace KWin::BetterWobblyWindows
{
inline constexpr int EffectChainPosition = 70;
inline constexpr const char TraceMagic[] = "BETTER_WOBBLY_BETTER_BLUR_DX_API_V1";
inline constexpr const char PluginFactoryTraceMagic[] = "BETTER_WOBBLY_MAIN_LOADED";

inline constexpr int MaxWobblynessLevel = 4;
inline constexpr qreal PercentScale = 100.0;
inline constexpr qreal StandardDpi = 96.0;
inline constexpr int IntegrationStepMilliseconds = 10;

// Optional visual-only elastic counter-resize. Each real resize step injects
// a short impulse into the opposite edge of the rendered mesh. The opposite
// edge moves outward for expansion (or inward for contraction), then returns
// with a lightly under-damped spring. EffectWindow::frameGeometry() is never
// modified.
inline constexpr int DefaultOppositePointFollowStrengthPercent = 8;
inline constexpr int MinimumOppositePointFollowStrengthPercent = 1;
inline constexpr int MaximumOppositePointFollowStrengthPercent = 30;
inline constexpr qreal OppositePointFollowMaximumPixels = 96.0;
inline constexpr qreal OppositePointFollowImpulseScale = 4.0;
inline constexpr qreal OppositePointFollowSpringFrequencyHz = 5.0;
inline constexpr qreal OppositePointFollowDampingRatio = 0.44;
inline constexpr qreal OppositePointFollowStopPixels = 0.03;
inline constexpr qreal OppositePointFollowStopVelocity = 0.20;
inline constexpr qreal OppositePointFollowGeometryEpsilon = 0.01;

inline constexpr int InitialEmbeddedBlurIterationCount = 2;
inline constexpr float InitialEmbeddedBlurOffset = 2.0f;
inline constexpr int InitialEmbeddedBlurExpandSize = 20;
inline constexpr int DefaultBlurStrength = 15;
inline constexpr int MinimumBlurStrength = 1;
inline constexpr int MinimumPhysicsStepsPerFrame = 1;
inline constexpr int DefaultPhysicsStepsPerFrame = 6;
inline constexpr int MaximumPhysicsStepsPerFrame = 16;
inline constexpr int MinimumBlurMode = 0;
inline constexpr int MaximumColorMode = 2;
inline constexpr int MaximumNoiseMode = 1;
inline constexpr int MinimumHandoffFrames = 1;
inline constexpr int DefaultHandoffFrames = 8;
inline constexpr int MaximumHandoffFrames = 24;
inline constexpr int MinimumEdgeSoftnessPixels = 0;
inline constexpr int MaximumEdgeSoftnessPixels = 16;
inline constexpr int DefaultFallbackFrames = 8;
inline constexpr int MinimumFallbackFrames = 0;
inline constexpr int MaximumFallbackFrames = 60;
inline constexpr bool DefaultSourceYFlip = true;
inline constexpr int EmbeddedBlurExternalFallbackMissFrames = 1;
inline constexpr int MinimumNoiseStrength = 0;
inline constexpr int MaximumNoiseStrength = 255;
inline constexpr int MinimumTextureDimension = 1;
inline constexpr quint64 MaximumEmbeddedBlurAllocationBytes = 256ull * 1024ull * 1024ull;
inline constexpr quint64 MaximumRetainedEmbeddedBlurBytes = 96ull * 1024ull * 1024ull;
inline constexpr int EmbeddedBlurAllocationRetryFrames = 120;
inline constexpr std::size_t MaximumRetainedBlurMeshVertices = 262144;
inline constexpr int BlurStrengthStepCount = 15;

inline constexpr int DefaultSaturationPercent = 150;
inline constexpr int DefaultContrastPercent = 100;
inline constexpr int DefaultBrightnessPercent = 100;
inline constexpr int DefaultNoiseStrength = 5;
inline constexpr int DefaultCornerRadius = 0;

inline constexpr int MinimumCornerRadius = 0;
inline constexpr int NoiseTextureWidth = 256;
inline constexpr int NoiseTextureHeight = 256;
inline constexpr int MaxPhysicsCatchUpMilliseconds = 64;
inline constexpr float BlurKernelOffsetBias = 2.0f;
inline constexpr float BlurKernelScaleMultiplier = 8.0f;
inline constexpr qreal WobblePaddingWindowRatio = 0.08;
inline constexpr int MinimumWobblePaddingPixels = 24;
inline constexpr int MaximumWobblePaddingPixels = 240;
inline constexpr int QuadTriangleVertexCount = 6;
} // namespace KWin::BetterWobblyWindows
