/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/EmbeddedBlurMath.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"
#include "TestSupport.h"

#include <QMatrix4x4>
#include <QRect>
#include <QSize>
#include <QVector4D>

#include <cmath>
#include <cstdio>

static_assert(KWin::BetterWobblyWindows::DefaultBlurStrength == 15);
static_assert(KWin::BetterWobblyWindows::DefaultSaturationPercent == 150);
static_assert(KWin::BetterWobblyWindows::DefaultContrastPercent == 100);
static_assert(KWin::BetterWobblyWindows::DefaultBrightnessPercent == 100);
static_assert(KWin::BetterWobblyWindows::DefaultNoiseStrength == 5);
static_assert(KWin::BetterWobblyWindows::DefaultCornerRadius == 0);

int main()
{
    using namespace BetterWobblyWindowsTest;
    using namespace KWin::BetterWobblyWindows;

    CHECK(RoundUpToMultiple(-20, 0) == 1);
    CHECK(RoundUpToMultiple(1, 4) == 4);
    CHECK(RoundUpToMultiple(5, 4) == 8);
    CHECK(RoundUpToMultiple(16, 4) == 16);

    // Better Blur DX parity requires an exact-size base texture. Bucketing changes
    // the dimensions and half-pixel offsets of every dual-kawase level.
    CHECK(BucketEmbeddedBlurTextureSize(QSize(0, -10)) == QSize(1, 1));
    CHECK(BucketEmbeddedBlurTextureSize(QSize(640, 480)) == QSize(640, 480));
    CHECK(BucketEmbeddedBlurTextureSize(QSize(641, 513)) == QSize(641, 513));

    const QRect outputRect(0, 0, 1920, 1080);
    const QRect bottomPaddedCapture(100, 900, 800, 260);
    const QRect bottomClippedCapture = ClipEmbeddedBlurCaptureRectToRenderTarget(bottomPaddedCapture, outputRect);
    CHECK(bottomClippedCapture == QRect(100, 900, 800, 180));

    const QRect fullyVisibleCapture(100, 700, 800, 260);
    CHECK(ClipEmbeddedBlurCaptureRectToRenderTarget(fullyVisibleCapture, outputRect) == fullyVisibleCapture);

    const QRect offscreenCapture(100, 1200, 800, 260);
    CHECK(ClipEmbeddedBlurCaptureRectToRenderTarget(offscreenCapture, outputRect).isEmpty());

    CHECK(DownsampleTextureSize(QSize(9, 5), 0) == QSize(9, 5));
    CHECK(DownsampleTextureSize(QSize(9, 5), 1) == QSize(4, 2));
    CHECK(DownsampleTextureSize(QSize(9, 5), 3) == QSize(1, 1));

    const QMatrix4x4 identity = BuildColorTransformMatrix(1.0, 1.0, 1.0);
    const QVector4D unchanged = identity * QVector4D(0.2f, 0.4f, 0.6f, 1.0f);
    CHECK(NearlyEqual(unchanged.x(), 0.2f));
    CHECK(NearlyEqual(unchanged.y(), 0.4f));
    CHECK(NearlyEqual(unchanged.z(), 0.6f));

    const QMatrix4x4 darker = BuildColorTransformMatrix(1.0, 1.0, 0.5);
    const QVector4D darkened = darker * QVector4D(0.8f, 0.6f, 0.4f, 1.0f);
    CHECK(NearlyEqual(darkened.x(), 0.4f));
    CHECK(NearlyEqual(darkened.y(), 0.3f));
    CHECK(NearlyEqual(darkened.z(), 0.2f));

    const QVector4D flippedClamp = FlipNormalizedYClamp(QVector4D(0.1f, 0.2f, 0.9f, 0.8f));
    CHECK(NearlyEqual(flippedClamp.x(), 0.1f));
    CHECK(NearlyEqual(flippedClamp.y(), 0.2f));
    CHECK(NearlyEqual(flippedClamp.z(), 0.9f));
    CHECK(NearlyEqual(flippedClamp.w(), 0.8f));

    const QVector4D flippedAsymmetricClamp = FlipNormalizedYClamp(QVector4D(0.0f, 0.1f, 1.0f, 0.3f));
    CHECK(NearlyEqual(flippedAsymmetricClamp.y(), 0.7f));
    CHECK(NearlyEqual(flippedAsymmetricClamp.w(), 0.9f));

    const QVector4D bottomClippedClamp(0.0f, 0.0f, 1.0f, 0.75f);
    const QVector4D unflippedReadClamp = ClampForTextureYOrientation(bottomClippedClamp, false);
    CHECK(NearlyEqual(unflippedReadClamp.y(), 0.0f));
    CHECK(NearlyEqual(unflippedReadClamp.w(), 0.75f));

    const QVector4D flippedReadClamp = ClampForTextureYOrientation(bottomClippedClamp, true);
    CHECK(NearlyEqual(flippedReadClamp.y(), 0.25f));
    CHECK(NearlyEqual(flippedReadClamp.w(), 1.0f));

    const QVector4D insetClamp = NormalizedTextureClamp(QRect(10, 20, 20, 10), QRect(0, 0, 100, 50));
    CHECK(NearlyEqual(insetClamp.x(), 0.105f));
    CHECK(NearlyEqual(insetClamp.y(), 0.41f));
    CHECK(NearlyEqual(insetClamp.z(), 0.295f));
    CHECK(NearlyEqual(insetClamp.w(), 0.59f));

    const QVector4D singlePixelClamp = NormalizedTextureClamp(QRect(4, 7, 1, 1), QRect(0, 0, 10, 10));
    CHECK(NearlyEqual(singlePixelClamp.x(), 0.45f));
    CHECK(NearlyEqual(singlePixelClamp.y(), 0.75f));
    CHECK(NearlyEqual(singlePixelClamp.z(), 0.45f));
    CHECK(NearlyEqual(singlePixelClamp.w(), 0.75f));

    CHECK(NearlyEqual(static_cast<float>(AdvanceBlurTransition(0.0, 1.0, 4)), 0.25f));
    CHECK(NearlyEqual(static_cast<float>(AdvanceBlurTransition(0.75, 1.0, 4)), 1.0f));
    CHECK(NearlyEqual(static_cast<float>(AdvanceBlurTransition(1.0, 0.0, 4)), 0.75f));
    CHECK(NearlyEqual(static_cast<float>(AdvanceBlurTransition(0.25, 0.0, 4)), 0.0f));
    CHECK(NearlyEqual(static_cast<float>(AdvanceBlurTransition(0.25, 1.0, 0)), 1.0f));
    CHECK(NearlyEqual(static_cast<float>(SmoothBlurTransition(0.0)), 0.0f));
    CHECK(NearlyEqual(static_cast<float>(SmoothBlurTransition(0.5)), 0.5f));
    CHECK(NearlyEqual(static_cast<float>(SmoothBlurTransition(1.0)), 1.0f));

    return 0;
}
