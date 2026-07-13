/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/EmbeddedBlurMath.h"
#include "TestSupport.h"

#include <QRect>
#include <QSize>
#include <QVector4D>

int main()
{
    using namespace BetterWobblyWindowsTest;
    using namespace KWin::BetterWobblyWindows;

    CHECK(RoundUpToMultiple(-10, -4) == 1);
    CHECK(RoundUpToMultiple(0, 0) == 1);
    CHECK(RoundUpToMultiple(7, 1) == 7);
    CHECK(RoundUpToMultiple(7, 2) == 8);
    CHECK(RoundUpToMultiple(8, 2) == 8);
    CHECK(RoundUpToMultiple(1, 64) == 64);

    CHECK(BucketEmbeddedBlurTextureSize(QSize(-20, -30)) == QSize(1, 1));
    CHECK(BucketEmbeddedBlurTextureSize(QSize(0, 720)) == QSize(1, 720));
    CHECK(BucketEmbeddedBlurTextureSize(QSize(1280, 0)) == QSize(1280, 1));
    CHECK(BucketEmbeddedBlurTextureSize(QSize(1279, 719)) == QSize(1279, 719));

    const QRect renderRect(100, 200, 800, 600);
    CHECK(ClipEmbeddedBlurCaptureRectToRenderTarget(renderRect, renderRect) == renderRect);
    CHECK(ClipEmbeddedBlurCaptureRectToRenderTarget(QRect(50, 250, 100, 100), renderRect) ==
        QRect(100, 250, 50, 100));
    CHECK(ClipEmbeddedBlurCaptureRectToRenderTarget(QRect(850, 250, 100, 100), renderRect) ==
        QRect(850, 250, 50, 100));
    CHECK(ClipEmbeddedBlurCaptureRectToRenderTarget(QRect(150, 150, 100, 100), renderRect) ==
        QRect(150, 200, 100, 50));
    CHECK(ClipEmbeddedBlurCaptureRectToRenderTarget(QRect(150, 750, 100, 100), renderRect) ==
        QRect(150, 750, 100, 50));
    CHECK(ClipEmbeddedBlurCaptureRectToRenderTarget(QRect(-1000, -1000, 10, 10), renderRect).isEmpty());
    CHECK(ClipEmbeddedBlurCaptureRectToRenderTarget(QRect(), renderRect).isEmpty());

    CHECK(DownsampleTextureSize(QSize(64, 32), -1) == QSize(64, 32));
    CHECK(DownsampleTextureSize(QSize(64, 32), 0) == QSize(64, 32));
    CHECK(DownsampleTextureSize(QSize(64, 32), 1) == QSize(32, 16));
    CHECK(DownsampleTextureSize(QSize(64, 32), 5) == QSize(2, 1));
    CHECK(DownsampleTextureSize(QSize(64, 32), 20) == QSize(1, 1));
    CHECK(DownsampleTextureSize(QSize(7, 3), 2) == QSize(1, 1));

    const QVector4D fullClamp = NormalizedTextureClamp(QRect(0, 0, 100, 50), QRect(0, 0, 100, 50));
    CHECK(NearlyEqual(fullClamp.x(), 0.005f));
    CHECK(NearlyEqual(fullClamp.y(), 0.01f));
    CHECK(NearlyEqual(fullClamp.z(), 0.995f));
    CHECK(NearlyEqual(fullClamp.w(), 0.99f));

    const QVector4D translatedClamp = NormalizedTextureClamp(QRect(110, 220, 20, 10), QRect(100, 200, 100, 50));
    CHECK(NearlyEqual(translatedClamp.x(), 0.105f));
    CHECK(NearlyEqual(translatedClamp.y(), 0.41f));
    CHECK(NearlyEqual(translatedClamp.z(), 0.295f));
    CHECK(NearlyEqual(translatedClamp.w(), 0.59f));

    const QVector4D partiallyClipped = NormalizedTextureClamp(QRect(90, 190, 30, 30), QRect(100, 200, 100, 50));
    CHECK(NearlyEqual(partiallyClipped.x(), 0.005f));
    CHECK(NearlyEqual(partiallyClipped.y(), 0.01f));
    CHECK(NearlyEqual(partiallyClipped.z(), 0.195f));
    CHECK(NearlyEqual(partiallyClipped.w(), 0.39f));

    CHECK(NormalizedTextureClamp(QRect(0, 0, 10, 10), QRect()) ==
        QVector4D(0.0f, 0.0f, 1.0f, 1.0f));
    CHECK(NormalizedTextureClamp(QRect(-100, -100, 10, 10), QRect(0, 0, 100, 100)) ==
        QVector4D(0.0f, 0.0f, 1.0f, 1.0f));

    const QVector4D flipped = FlipNormalizedYClamp(QVector4D(-0.5f, -0.2f, 1.5f, 1.2f));
    CHECK(NearlyEqual(flipped.x(), -0.5f));
    CHECK(NearlyEqual(flipped.y(), 0.0f));
    CHECK(NearlyEqual(flipped.z(), 1.5f));
    CHECK(NearlyEqual(flipped.w(), 1.0f));

    const QVector4D reversed = FlipNormalizedYClamp(QVector4D(0.0f, 0.8f, 1.0f, 0.2f));
    CHECK(NearlyEqual(reversed.y(), 0.2f));
    CHECK(NearlyEqual(reversed.w(), 0.8f));

    return 0;
}
