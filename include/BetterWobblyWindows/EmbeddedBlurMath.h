/*
    Portions of this file are adapted from Better Blur DX and the KWin Blur effect.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
    SPDX-FileCopyrightText: 2024-2026 Better Blur DX contributors
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later

    Better Blur DX source: https://github.com/xarblu/kwin-effects-better-blur-dx
    Reference snapshot: 35451011b48bd01c2afa1a58ceeac46197147b81
    See AUTHORS.md and NOTICE.md for the complete provenance record.
*/
#pragma once

#include <QMatrix4x4>
#include <QRect>
#include <QSize>
#include <QVector4D>
#include <QtGlobal>

namespace KWin::BetterWobblyWindows
{
QMatrix4x4 BuildColorTransformMatrix(qreal saturation, qreal contrast, qreal brightness);
int RoundUpToMultiple(int value, int multiple);
QSize BucketEmbeddedBlurTextureSize(const QSize &size);
QRect ClipEmbeddedBlurCaptureRectToRenderTarget(const QRect &captureRect, const QRect &renderRect);
QSize DownsampleTextureSize(const QSize &baseSize, int level);
QVector4D FlipNormalizedYClamp(const QVector4D &sourceClamp);
QVector4D ClampForTextureYOrientation(const QVector4D &sourceClamp, bool textureYFlipped);
QVector4D NormalizedTextureClamp(const QRect &sampleRect, const QRect &textureRect);
qreal AdvanceBlurTransition(qreal current, qreal target, int frameCount);
qreal SmoothBlurTransition(qreal progress);
} // namespace KWin::BetterWobblyWindows
