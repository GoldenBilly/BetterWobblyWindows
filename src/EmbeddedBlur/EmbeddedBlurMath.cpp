/*
    Portions of this file are adapted from Better Blur DX and the KWin Blur effect.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
    SPDX-FileCopyrightText: 2024-2026 Better Blur DX contributors
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later

    Better Blur DX source: https://github.com/xarblu/kwin-effects-better-blur-dx
    See AUTHORS.md and NOTICE.md for the complete provenance record.
*/
#include "BetterWobblyWindows/EmbeddedBlurMath.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"

#include <algorithm>

namespace KWin::BetterWobblyWindows
{
QMatrix4x4 BuildColorTransformMatrix(const qreal saturation, const qreal contrast, const qreal brightness)
{
    QMatrix4x4 saturationMatrix;
    QMatrix4x4 contrastMatrix;
    QMatrix4x4 brightnessMatrix;

    if (!qFuzzyCompare(saturation, 1.0)) {
        const auto saturationFloat = static_cast<float>(saturation);
        const float redLuma = (1.0f - saturationFloat) * 0.2126f;
        const float greenLuma = (1.0f - saturationFloat) * 0.7152f;
        const float blueLuma = (1.0f - saturationFloat) * 0.0722f;
        saturationMatrix = QMatrix4x4(redLuma + saturationFloat,
            redLuma,
            redLuma,
            0.0,
            greenLuma,
            greenLuma + saturationFloat,
            greenLuma,
            0.0,
            blueLuma,
            blueLuma,
            blueLuma + saturationFloat,
            0.0,
            0.0,
            0.0,
            0.0,
            1.0);
    }

    if (!qFuzzyCompare(contrast, 1.0)) {
        const auto contrastFloat = static_cast<float>(contrast);
        const float translation = (1.0f - contrastFloat) / 2.0f;
        contrastMatrix = QMatrix4x4(contrastFloat,
            0.0,
            0.0,
            0.0,
            0.0,
            contrastFloat,
            0.0,
            0.0,
            0.0,
            0.0,
            contrastFloat,
            0.0,
            translation,
            translation,
            translation,
            1.0);
    }

    if (!qFuzzyCompare(brightness, 1.0)) {
        const auto brightnessFloat = static_cast<float>(brightness);
        brightnessMatrix.scale(brightnessFloat, brightnessFloat, brightnessFloat);
    }

    return contrastMatrix * saturationMatrix * brightnessMatrix;
}

int RoundUpToMultiple(int value, int multiple)
{
    if (multiple <= MinimumTextureDimension) {
        return std::max(MinimumTextureDimension, value);
    }

    return std::max(multiple, ((std::max(MinimumTextureDimension, value) + multiple - 1) / multiple) * multiple);
}

QSize BucketEmbeddedBlurTextureSize(const QSize &size)
{
    // Better Blur DX builds the dual-kawase pyramid from the exact capture size.
    // A larger backing texture is not visually neutral: it changes every level's
    // dimensions, half-pixel offsets and normalized sampling coordinates. Keep the
    // exact logical size here; allocation reuse is allowed only for an exact match.
    return QSize(std::max(MinimumTextureDimension, size.width()), std::max(MinimumTextureDimension, size.height()));
}

QRect ClipEmbeddedBlurCaptureRectToRenderTarget(const QRect &captureRect, const QRect &renderRect)
{
    return captureRect.intersected(renderRect);
}

QSize DownsampleTextureSize(const QSize &baseSize, int level)
{
    QSize size = baseSize;
    for (int index = 0; index < level; ++index) {
        size = QSize(std::max(MinimumTextureDimension, size.width() / 2),
            std::max(MinimumTextureDimension, size.height() / 2));
    }
    return size;
}

QVector4D FlipNormalizedYClamp(const QVector4D &sourceClamp)
{
    const float flippedY0 = std::clamp(1.0f - sourceClamp.w(), 0.0f, 1.0f);
    const float flippedY1 = std::clamp(1.0f - sourceClamp.y(), 0.0f, 1.0f);
    return QVector4D(sourceClamp.x(), std::min(flippedY0, flippedY1), sourceClamp.z(), std::max(flippedY0, flippedY1));
}

QVector4D ClampForTextureYOrientation(const QVector4D &sourceClamp, bool textureYFlipped)
{
    return textureYFlipped ? FlipNormalizedYClamp(sourceClamp) : sourceClamp;
}

QVector4D NormalizedTextureClamp(const QRect &sampleRect, const QRect &textureRect)
{
    const QRect clipped = sampleRect.intersected(textureRect);
    if (clipped.isEmpty() || textureRect.isEmpty()) {
        return QVector4D(0.0f, 0.0f, 1.0f, 1.0f);
    }

    const float width = static_cast<float>(std::max(MinimumTextureDimension, textureRect.width()));
    const float height = static_cast<float>(std::max(MinimumTextureDimension, textureRect.height()));

    // Clamp to texel centers. GL_CLAMP_TO_EDGE gives these bounds implicitly
    // for a complete DX-sized texture; the explicit bounds preserve the same
    // behavior when an output boundary clips the captured source rectangle.
    const float left = (static_cast<float>(clipped.left() - textureRect.left()) + 0.5f) / width;
    const float top = (static_cast<float>(clipped.top() - textureRect.top()) + 0.5f) / height;
    const float right = (static_cast<float>(clipped.right() - textureRect.left()) + 0.5f) / width;
    const float bottom = (static_cast<float>(clipped.bottom() - textureRect.top()) + 0.5f) / height;

    return QVector4D(std::clamp(left, 0.0f, 1.0f),
        std::clamp(top, 0.0f, 1.0f),
        std::clamp(right, 0.0f, 1.0f),
        std::clamp(bottom, 0.0f, 1.0f));
}

qreal AdvanceBlurTransition(qreal current, qreal target, int frameCount)
{
    current = std::clamp(current, 0.0, 1.0);
    target = std::clamp(target, 0.0, 1.0);
    if (frameCount <= 0 || qFuzzyCompare(current + 1.0, target + 1.0)) {
        return target;
    }

    const qreal step = 1.0 / static_cast<qreal>(frameCount);
    return current < target ? std::min(target, current + step) : std::max(target, current - step);
}

qreal SmoothBlurTransition(const qreal progress)
{
    const qreal t = std::clamp(progress, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

} // namespace KWin::BetterWobblyWindows
