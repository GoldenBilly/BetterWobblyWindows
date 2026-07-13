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

#include "BetterWobblyWindows/WobblyWindowsConstants.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QString>

namespace KWin::BetterWobblyWindows
{
inline const QString BetterWobblyConfigGroupName = QStringLiteral("Effect-better_wobblywindows");
inline const QString BetterBlurDxConfigGroupName = QStringLiteral("Effect-better-blur-dx");

struct DxBlurVisualSettings {
    int blurStrength = DefaultBlurStrength;
    int saturation = DefaultSaturationPercent;
    int contrast = DefaultContrastPercent;
    int brightness = DefaultBrightnessPercent;
    int noiseStrength = DefaultNoiseStrength;
    qreal cornerRadius = DefaultCornerRadius;
    int refractionStrength = 0;
    int refractionMode = 0;
    int refractionEdgeSize = 20;
    int refractionNormalPow = 2;
    int refractionCornerRadius = 8;
    int refractionRgbFringing = 1;
    int refractionTextureRepeatMode = 0;
};

template <typename T> T ReadBetterBlurDxEntry(const KSharedConfigPtr &config, const QString &key, const T &defaultValue)
{
    const KConfigGroup group(config, BetterBlurDxConfigGroupName);
    return group.readEntry(key, defaultValue);
}

inline DxBlurVisualSettings ReadBetterBlurDxSettings(const KSharedConfigPtr &config)
{
    DxBlurVisualSettings settings;
    settings.blurStrength = ReadBetterBlurDxEntry(config, QStringLiteral("BlurStrength"), DefaultBlurStrength);
    settings.saturation = ReadBetterBlurDxEntry(config, QStringLiteral("Saturation"), DefaultSaturationPercent);
    settings.contrast = ReadBetterBlurDxEntry(config, QStringLiteral("Contrast"), DefaultContrastPercent);
    settings.brightness = ReadBetterBlurDxEntry(config, QStringLiteral("Brightness"), DefaultBrightnessPercent);
    settings.noiseStrength = ReadBetterBlurDxEntry(config, QStringLiteral("NoiseStrength"), DefaultNoiseStrength);
    settings.cornerRadius =
        ReadBetterBlurDxEntry(config, QStringLiteral("CornerRadius"), static_cast<qreal>(DefaultCornerRadius));
    settings.refractionStrength = ReadBetterBlurDxEntry(config, QStringLiteral("RefractionStrength"), 0);
    settings.refractionMode = ReadBetterBlurDxEntry(config, QStringLiteral("RefractionMode"), 0);
    settings.refractionEdgeSize = ReadBetterBlurDxEntry(config, QStringLiteral("RefractionEdgeSize"), 20);
    settings.refractionNormalPow = ReadBetterBlurDxEntry(config, QStringLiteral("RefractionNormalPow"), 2);
    settings.refractionCornerRadius = ReadBetterBlurDxEntry(config, QStringLiteral("RefractionCornerRadius"), 8);
    settings.refractionRgbFringing = ReadBetterBlurDxEntry(config, QStringLiteral("RefractionRGBFringing"), 1);
    settings.refractionTextureRepeatMode =
        ReadBetterBlurDxEntry(config, QStringLiteral("RefractionTextureRepeatMode"), 0);
    return settings;
}

inline DxBlurVisualSettings ReadEmbeddedBlurLocalSettings(const KSharedConfigPtr &config,
    const DxBlurVisualSettings &dxSettings)
{
    const KConfigGroup group(config, BetterWobblyConfigGroupName);

    DxBlurVisualSettings settings;
    settings.blurStrength = group.readEntry("EmbeddedDXBlurStrength", dxSettings.blurStrength);
    settings.saturation = group.readEntry("EmbeddedDXBlurSaturation", dxSettings.saturation);
    settings.contrast = group.readEntry("EmbeddedDXBlurContrast", dxSettings.contrast);
    settings.brightness = group.readEntry("EmbeddedDXBlurBrightness", dxSettings.brightness);
    settings.noiseStrength = group.readEntry("EmbeddedDXBlurNoiseStrength", dxSettings.noiseStrength);
    settings.cornerRadius = group.readEntry("EmbeddedDXBlurCornerRadius", dxSettings.cornerRadius);

    // Local blur controls do not expose refraction. Reusing the DX values keeps
    // the fallback material identical instead of silently disabling refraction.
    settings.refractionStrength = dxSettings.refractionStrength;
    settings.refractionMode = dxSettings.refractionMode;
    settings.refractionEdgeSize = dxSettings.refractionEdgeSize;
    settings.refractionNormalPow = dxSettings.refractionNormalPow;
    settings.refractionCornerRadius = dxSettings.refractionCornerRadius;
    settings.refractionRgbFringing = dxSettings.refractionRgbFringing;
    settings.refractionTextureRepeatMode = dxSettings.refractionTextureRepeatMode;
    return settings;
}

inline DxBlurVisualSettings ReadEmbeddedBlurLocalSettings(const KSharedConfigPtr &config)
{
    return ReadEmbeddedBlurLocalSettings(config, ReadBetterBlurDxSettings(config));
}

inline bool UseBetterBlurDxSettings(const KSharedConfigPtr &config)
{
    const KConfigGroup group(config, BetterWobblyConfigGroupName);
    return group.readEntry("EmbeddedDXBlurUseBetterBlurDXSettings", true);
}

inline DxBlurVisualSettings ReadEmbeddedBlurEffectiveSettings(const KSharedConfigPtr &config)
{
    if (UseBetterBlurDxSettings(config)) {
        return ReadBetterBlurDxSettings(config);
    }
    return ReadEmbeddedBlurLocalSettings(config);
}

inline bool IsRelevantConfigGroupForHotReload(const QString &groupName)
{
    return groupName.isEmpty() || groupName == BetterWobblyConfigGroupName || groupName == BetterBlurDxConfigGroupName;
}

} // namespace KWin::BetterWobblyWindows
