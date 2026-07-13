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

#include "BetterWobblyWindows/DxBlurSettings.h"

#include <kcmodule.h>

class QCheckBox;
class QComboBox;
class QPushButton;
class QSlider;
class QSpinBox;

namespace KWin
{

class WobblyWindowsEffectConfig : public KCModule
{
    Q_OBJECT

  public:
    explicit WobblyWindowsEffectConfig(QObject *parent, const KPluginMetaData &data);
    ~WobblyWindowsEffectConfig() override = default;

  public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

  private Q_SLOTS:
    void MarkChanged();
    void WobblinessChanged(int value);
    void AdvancedModeChanged(bool enabled);
    void UseDxBlurSettingsChanged(bool enabled);
    void PullDxSettingsClicked();

  private:
    void SetPreset(int value);
    void ReadDxSettings();
    void SetBlurSettingsWidgets(const BetterWobblyWindows::DxBlurVisualSettings &settings);
    void UpdateBlurInputState();

    QSlider *_wobblyness = nullptr;
    QCheckBox *_moveWobble = nullptr;
    QCheckBox *_resizeWobble = nullptr;
    QCheckBox *_visualOppositePointFollowOnResize = nullptr;
    QSlider *_visualOppositePointFollowStrength = nullptr;
    QCheckBox *_advancedMode = nullptr;
    QCheckBox *_embeddedDXBlur = nullptr;
    QCheckBox *_useDxBlurSettings = nullptr;
    QPushButton *_pullDxSettings = nullptr;
    QSpinBox *_stiffness = nullptr;
    QSpinBox *_drag = nullptr;
    QSpinBox *_moveFactor = nullptr;
    QSpinBox *_blurStrength = nullptr;
    QSpinBox *_saturation = nullptr;
    QSpinBox *_contrast = nullptr;
    QSpinBox *_brightness = nullptr;
    QSpinBox *_noiseStrength = nullptr;
    QSpinBox *_cornerRadius = nullptr;
    QComboBox *_colorMode = nullptr;
    QComboBox *_noiseMode = nullptr;
    QSpinBox *_handoffFrames = nullptr;
    QSpinBox *_edgeSoftness = nullptr;
    QCheckBox *_clearIntermediates = nullptr;
    QSpinBox *_fallbackFrames = nullptr;
    QCheckBox *_sourceYFlip = nullptr;
    QSpinBox *_maxPhysicsSteps = nullptr;
};

} // namespace KWin
