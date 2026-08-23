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
#include "BetterWobblyWindows/WobblyWindowsEffectConfig.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"

#include <KConfig>
#include <KConfigGroup>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QCheckBox>
#include <QComboBox>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace KWin
{

K_PLUGIN_FACTORY_WITH_JSON(
    WobblyWindowsEffectConfigFactory, "better_wobblywindows_config.json", registerPlugin<WobblyWindowsEffectConfig>();)

namespace
{
struct ParameterSet {
    int stiffness;
    int drag;
    int moveFactor;
};

static constexpr ParameterSet Presets[] = {
    {15, 80, 10},
    {10, 85, 10},
    {6, 90, 10},
    {3, 92, 20},
    {1, 97, 25},
};

static KSharedConfigPtr OpenKWinConfig()
{
    return KSharedConfig::openConfig(QStringLiteral("kwinrc"));
}

static void SetComboValue(QComboBox *combo, int value)
{
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

template <typename T> static void WriteNotifyingEntry(KConfigGroup &group, const char *key, const T &value)
{
    // KConfigWatcher only receives changes written with the Notify flag.
    // This makes the running effect reload immediately after Apply, even if
    // the effect-specific D-Bus reconfigure call is unavailable on a given
    // Plasma/KWin build.
    group.writeEntry(key, value, KConfigBase::Notify);
}
} // namespace

WobblyWindowsEffectConfig::WobblyWindowsEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    auto root = new QVBoxLayout(widget());
    auto tabs = new QTabWidget(widget());
    root->addWidget(tabs);

    auto behaviorPage = new QWidget(tabs);
    auto behaviorPageLayout = new QVBoxLayout(behaviorPage);

    auto actions = new QGroupBox(QStringLiteral("Actions"), behaviorPage);
    auto actionsLayout = new QVBoxLayout(actions);
    _moveWobble = new QCheckBox(QStringLiteral("Wobble when moving"), actions);
    _resizeWobble = new QCheckBox(QStringLiteral("Wobble when resizing"), actions);
    actionsLayout->addWidget(_moveWobble);
    actionsLayout->addWidget(_resizeWobble);
    behaviorPageLayout->addWidget(actions);

    auto elasticResize = new QGroupBox(QStringLiteral("Elastic resize"), behaviorPage);
    auto elasticResizeLayout = new QVBoxLayout(elasticResize);
    _visualOppositePointFollowOnResize =
        new QCheckBox(QStringLiteral("Elastic opposite edge during resize (visual effect only)"), elasticResize);
    _visualOppositePointFollowOnResize->setToolTip(
        QStringLiteral("The edge opposite the resize handle briefly moves in the mirrored direction, "
                       "then springs back to the real geometry. The actual resize is not changed."));
    elasticResizeLayout->addWidget(_visualOppositePointFollowOnResize);

    auto elasticResizeForm = new QFormLayout();
    _visualOppositePointFollowStrength = new QSlider(Qt::Horizontal, elasticResize);
    _visualOppositePointFollowStrength->setRange(BetterWobblyWindows::MinimumOppositePointFollowStrengthPercent,
        BetterWobblyWindows::MaximumOppositePointFollowStrengthPercent);
    _visualOppositePointFollowStrength->setTickInterval(5);
    _visualOppositePointFollowStrength->setTickPosition(QSlider::TicksBelow);
    _visualOppositePointFollowStrength->setToolTip(
        QStringLiteral("Strength of the visual counter-resize impulse applied to the opposite edge."));
    elasticResizeForm->addRow(QStringLiteral("Opposite edge response strength:"), _visualOppositePointFollowStrength);
    elasticResizeLayout->addLayout(elasticResizeForm);
    behaviorPageLayout->addWidget(elasticResize);

    auto feel = new QGroupBox(QStringLiteral("Wobble feel"), behaviorPage);
    auto feelForm = new QFormLayout(feel);
    _wobblyness = new QSlider(Qt::Horizontal, feel);
    _wobblyness->setRange(0, 4);
    _wobblyness->setTickPosition(QSlider::TicksBelow);
    feelForm->addRow(QStringLiteral("Wobbliness:"), _wobblyness);
    behaviorPageLayout->addWidget(feel);
    behaviorPageLayout->addStretch();
    tabs->addTab(behaviorPage, QStringLiteral("Behavior"));

    auto physicsPage = new QWidget(tabs);
    auto physicsPageLayout = new QVBoxLayout(physicsPage);

    auto physics = new QGroupBox(QStringLiteral("Physics"), physicsPage);
    auto physicsLayout = new QVBoxLayout(physics);
    _advancedMode = new QCheckBox(QStringLiteral("Enable advanced mode"), physics);
    physicsLayout->addWidget(_advancedMode);

    auto physicsForm = new QFormLayout();
    _stiffness = new QSpinBox(physics);
    _stiffness->setRange(1, 50);
    physicsForm->addRow(QStringLiteral("Stiffness:"), _stiffness);

    _drag = new QSpinBox(physics);
    _drag->setRange(50, 100);
    physicsForm->addRow(QStringLiteral("Drag:"), _drag);

    _moveFactor = new QSpinBox(physics);
    _moveFactor->setRange(1, 25);
    physicsForm->addRow(QStringLiteral("Move factor:"), _moveFactor);

    _maxPhysicsSteps = new QSpinBox(physics);
    _maxPhysicsSteps->setRange(1, 16);
    physicsForm->addRow(QStringLiteral("Max physics steps per frame:"), _maxPhysicsSteps);
    physicsLayout->addLayout(physicsForm);
    physicsPageLayout->addWidget(physics);
    physicsPageLayout->addStretch();
    tabs->addTab(physicsPage, QStringLiteral("Physics"));

    auto blurPage = new QWidget(tabs);
    auto blurPageLayout = new QVBoxLayout(blurPage);

    auto blurSource = new QGroupBox(QStringLiteral("Blur source"), blurPage);
    auto blurSourceLayout = new QVBoxLayout(blurSource);
    _embeddedDXBlur = new QCheckBox(QStringLiteral("Draw embedded blur while wobbling"), blurSource);
    blurSourceLayout->addWidget(_embeddedDXBlur);

    _useDxBlurSettings = new QCheckBox(QStringLiteral("Use values from Better Blur DX settings"), blurSource);
    blurSourceLayout->addWidget(_useDxBlurSettings);

    _pullDxSettings = new QPushButton(QStringLiteral("Use settings as in Better Blur DX now"), blurSource);
    blurSourceLayout->addWidget(_pullDxSettings);
    blurPageLayout->addWidget(blurSource);

    auto blurAppearance = new QGroupBox(QStringLiteral("Appearance"), blurPage);
    auto blurAppearanceForm = new QFormLayout(blurAppearance);
    _blurStrength = new QSpinBox(blurAppearance);
    _blurStrength->setRange(1, 15);
    blurAppearanceForm->addRow(QStringLiteral("Blur strength:"), _blurStrength);

    _saturation = new QSpinBox(blurAppearance);
    _saturation->setRange(0, 400);
    _saturation->setSuffix(QStringLiteral(" %"));
    blurAppearanceForm->addRow(QStringLiteral("Saturation:"), _saturation);

    _contrast = new QSpinBox(blurAppearance);
    _contrast->setRange(0, 400);
    _contrast->setSuffix(QStringLiteral(" %"));
    blurAppearanceForm->addRow(QStringLiteral("Contrast:"), _contrast);

    _brightness = new QSpinBox(blurAppearance);
    _brightness->setRange(0, 400);
    _brightness->setSuffix(QStringLiteral(" %"));
    blurAppearanceForm->addRow(QStringLiteral("Brightness:"), _brightness);

    _noiseStrength = new QSpinBox(blurAppearance);
    _noiseStrength->setRange(0, 255);
    blurAppearanceForm->addRow(QStringLiteral("Noise strength:"), _noiseStrength);

    _cornerRadius = new QSpinBox(blurAppearance);
    _cornerRadius->setRange(0, 256);
    _cornerRadius->setSuffix(QStringLiteral(" px"));
    blurAppearanceForm->addRow(QStringLiteral("Corner radius:"), _cornerRadius);
    blurPageLayout->addWidget(blurAppearance);

    auto note = new QLabel(QStringLiteral(
                               "When \"Use values from Better Blur DX settings\" is enabled, the embedded moving blur "
                               "reads BlurStrength, Saturation, Contrast, Brightness, NoiseStrength, and CornerRadius "
                               "directly from Better Blur DX's kwinrc group and hot-reloads when that group changes."),
        blurPage);
    note->setWordWrap(true);
    blurPageLayout->addWidget(note);
    blurPageLayout->addStretch();
    tabs->addTab(blurPage, QStringLiteral("Blur"));

    auto blurAdvancedPage = new QWidget(tabs);
    auto blurAdvancedPageLayout = new QVBoxLayout(blurAdvancedPage);

    auto blurAdvanced = new QGroupBox(QStringLiteral("Rendering and fallback"), blurAdvancedPage);
    auto blurAdvancedLayout = new QVBoxLayout(blurAdvanced);
    auto blurAdvancedForm = new QFormLayout();

    _apiOnlyBlur =
        new QCheckBox(QStringLiteral("Better Wobbly API only (disable embedded blur fallback)"), blurAdvanced);
    _apiOnlyBlur->setToolTip(QStringLiteral(
        "Keep the Better Blur DX API hand-off enabled, but never run Better Wobbly Windows' embedded blur renderer."));
    blurAdvancedLayout->addWidget(_apiOnlyBlur);

    _colorMode = new QComboBox(blurAdvanced);
    _colorMode->addItem(QStringLiteral("DX exact color"), 0);
    _colorMode->addItem(QStringLiteral("Anti-flash clamp above 100%"), 1);
    _colorMode->addItem(QStringLiteral("No embedded color transform"), 2);
    blurAdvancedForm->addRow(QStringLiteral("Moving blur color mode:"), _colorMode);

    _noiseMode = new QComboBox(blurAdvanced);
    _noiseMode->addItem(QStringLiteral("DX exact additive noise"), 0);
    _noiseMode->addItem(QStringLiteral("Alpha-safe noise"), 1);
    blurAdvancedForm->addRow(QStringLiteral("Moving blur noise mode:"), _noiseMode);

    _handoffFrames = new QSpinBox(blurAdvanced);
    _handoffFrames->setRange(BetterWobblyWindows::MinimumHandoffFrames, BetterWobblyWindows::MaximumHandoffFrames);
    _handoffFrames->setSuffix(QStringLiteral(" frames"));
    blurAdvancedForm->addRow(QStringLiteral("Blur transition (in/out):"), _handoffFrames);

    _edgeSoftness = new QSpinBox(blurAdvanced);
    _edgeSoftness->setRange(0, 16);
    _edgeSoftness->setSuffix(QStringLiteral(" px"));
    blurAdvancedForm->addRow(QStringLiteral("Source edge softness:"), _edgeSoftness);

    _fallbackFrames = new QSpinBox(blurAdvanced);
    _fallbackFrames->setRange(BetterWobblyWindows::MinimumFallbackFrames, BetterWobblyWindows::MaximumFallbackFrames);
    _fallbackFrames->setSuffix(QStringLiteral(" frames"));
    blurAdvancedForm->addRow(QStringLiteral("Hold last valid blur source:"), _fallbackFrames);
    blurAdvancedLayout->addLayout(blurAdvancedForm);

    _clearIntermediates =
        new QCheckBox(QStringLiteral("Clear intermediate blur targets every frame (slower, safer)"), blurAdvanced);
    blurAdvancedLayout->addWidget(_clearIntermediates);

    _sourceYFlip = new QCheckBox(QStringLiteral("Flip embedded DX source vertically (recommended)"), blurAdvanced);
    blurAdvancedLayout->addWidget(_sourceYFlip);

    blurAdvancedPageLayout->addWidget(blurAdvanced);
    blurAdvancedPageLayout->addStretch();
    tabs->addTab(blurAdvancedPage, QStringLiteral("Blur Advanced"));

    const auto changed = [this]() { MarkChanged(); };
    connect(_moveWobble, &QCheckBox::toggled, this, changed);
    connect(_resizeWobble, &QCheckBox::toggled, this, [this](bool enabled) {
        _visualOppositePointFollowOnResize->setEnabled(enabled);
        _visualOppositePointFollowStrength->setEnabled(enabled && _visualOppositePointFollowOnResize->isChecked());
        MarkChanged();
    });
    connect(_visualOppositePointFollowOnResize, &QCheckBox::toggled, this, [this](bool enabled) {
        _visualOppositePointFollowStrength->setEnabled(enabled && _resizeWobble->isChecked());
        MarkChanged();
    });
    connect(_visualOppositePointFollowStrength, &QSlider::valueChanged, this, changed);
    connect(_advancedMode, &QCheckBox::toggled, this, &WobblyWindowsEffectConfig::AdvancedModeChanged);
    connect(_embeddedDXBlur, &QCheckBox::toggled, this, changed);
    connect(_useDxBlurSettings, &QCheckBox::toggled, this, &WobblyWindowsEffectConfig::UseDxBlurSettingsChanged);
    connect(_pullDxSettings, &QPushButton::clicked, this, &WobblyWindowsEffectConfig::PullDxSettingsClicked);
    connect(_wobblyness, &QSlider::valueChanged, this, &WobblyWindowsEffectConfig::WobblinessChanged);
    connect(_stiffness, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_drag, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_moveFactor, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_blurStrength, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_saturation, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_contrast, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_brightness, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_noiseStrength, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_cornerRadius, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_colorMode, qOverload<int>(&QComboBox::currentIndexChanged), this, changed);
    connect(_noiseMode, qOverload<int>(&QComboBox::currentIndexChanged), this, changed);
    connect(_handoffFrames, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_edgeSoftness, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_apiOnlyBlur, &QCheckBox::toggled, this, changed);
    connect(_clearIntermediates, &QCheckBox::toggled, this, changed);
    connect(_fallbackFrames, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(_sourceYFlip, &QCheckBox::toggled, this, changed);
    connect(_maxPhysicsSteps, qOverload<int>(&QSpinBox::valueChanged), this, changed);

    load();
}

void WobblyWindowsEffectConfig::MarkChanged()
{
    setNeedsSave(true);
}

void WobblyWindowsEffectConfig::SetPreset(int value)
{
    value = std::clamp(value, 0, 4);
    const ParameterSet preset = Presets[value];
    _stiffness->setValue(preset.stiffness);
    _drag->setValue(preset.drag);
    _moveFactor->setValue(preset.moveFactor);
}

void WobblyWindowsEffectConfig::WobblinessChanged(int value)
{
    if (!_advancedMode->isChecked()) {
        SetPreset(value);
    }
    MarkChanged();
}

void WobblyWindowsEffectConfig::AdvancedModeChanged(bool enabled)
{
    _stiffness->setEnabled(enabled);
    _drag->setEnabled(enabled);
    _moveFactor->setEnabled(enabled);
    if (!enabled) {
        SetPreset(_wobblyness->value());
    }
    MarkChanged();
}

void WobblyWindowsEffectConfig::SetBlurSettingsWidgets(const BetterWobblyWindows::DxBlurVisualSettings &settings)
{
    _blurStrength->setValue(std::clamp(settings.blurStrength,
        BetterWobblyWindows::MinimumBlurStrength,
        BetterWobblyWindows::BlurStrengthStepCount));
    _saturation->setValue(std::clamp(settings.saturation, 0, 400));
    _contrast->setValue(std::clamp(settings.contrast, 0, 400));
    _brightness->setValue(std::clamp(settings.brightness, 0, 400));
    _noiseStrength->setValue(std::clamp(settings.noiseStrength,
        BetterWobblyWindows::MinimumNoiseStrength,
        BetterWobblyWindows::MaximumNoiseStrength));
    _cornerRadius->setValue(
        static_cast<int>(std::clamp<qreal>(settings.cornerRadius, BetterWobblyWindows::MinimumCornerRadius, 256)));
}

void WobblyWindowsEffectConfig::ReadDxSettings()
{
    SetBlurSettingsWidgets(BetterWobblyWindows::ReadBetterBlurDxSettings(OpenKWinConfig()));
}

void WobblyWindowsEffectConfig::UpdateBlurInputState()
{
    const bool useDxSettings = _useDxBlurSettings->isChecked();
    _blurStrength->setEnabled(!useDxSettings);
    _saturation->setEnabled(!useDxSettings);
    _contrast->setEnabled(!useDxSettings);
    _brightness->setEnabled(!useDxSettings);
    _noiseStrength->setEnabled(!useDxSettings);
    _cornerRadius->setEnabled(!useDxSettings);
}

void WobblyWindowsEffectConfig::UseDxBlurSettingsChanged(bool enabled)
{
    if (enabled) {
        ReadDxSettings();
    }
    UpdateBlurInputState();
    MarkChanged();
}

void WobblyWindowsEffectConfig::PullDxSettingsClicked()
{
    ReadDxSettings();
    MarkChanged();
}

void WobblyWindowsEffectConfig::load()
{
    const auto config = OpenKWinConfig();
    const KConfigGroup group(config, QStringLiteral("Effect-better_wobblywindows"));
    _wobblyness->setValue(group.readEntry("WobblynessLevel", 0));
    _moveWobble->setChecked(group.readEntry("MoveWobble", true));
    _resizeWobble->setChecked(group.readEntry("ResizeWobble", true));
    _visualOppositePointFollowOnResize->setChecked(group.readEntry("VisualOppositePointFollowOnResize", false));
    _visualOppositePointFollowStrength->setValue(group.readEntry("VisualOppositePointFollowStrengthPercent",
        BetterWobblyWindows::DefaultOppositePointFollowStrengthPercent));
    _visualOppositePointFollowOnResize->setEnabled(_resizeWobble->isChecked());
    _visualOppositePointFollowStrength->setEnabled(
        _resizeWobble->isChecked() && _visualOppositePointFollowOnResize->isChecked());
    _advancedMode->setChecked(group.readEntry("AdvancedMode", false));
    _embeddedDXBlur->setChecked(group.readEntry("EmbeddedDXBlur", true));
    _apiOnlyBlur->setChecked(group.readEntry("EmbeddedDXBlurApiOnly", false));
    _useDxBlurSettings->setChecked(BetterWobblyWindows::UseBetterBlurDxSettings(config));
    SetComboValue(_colorMode, group.readEntry("EmbeddedDXBlurColorMode", 0));
    SetComboValue(_noiseMode, group.readEntry("EmbeddedDXBlurNoiseMode", 0));
    {
        const int configuredTransitionFrames =
            group.readEntry("EmbeddedDXBlurHandoffFrames", BetterWobblyWindows::DefaultHandoffFrames);
        _handoffFrames->setValue(
            configuredTransitionFrames <= 0 ? BetterWobblyWindows::DefaultHandoffFrames : configuredTransitionFrames);
    }
    _edgeSoftness->setValue(group.readEntry("EmbeddedDXBlurEdgeSoftnessPixels", 0));
    _clearIntermediates->setChecked(group.readEntry("EmbeddedDXBlurClearIntermediateTargets", false));
    _fallbackFrames->setValue(
        group.readEntry("EmbeddedDXBlurFallbackFrames", BetterWobblyWindows::DefaultFallbackFrames));
    _sourceYFlip->setChecked(group.readEntry("EmbeddedDXBlurSourceYFlip", BetterWobblyWindows::DefaultSourceYFlip));
    _maxPhysicsSteps->setValue(
        group.readEntry("MaxPhysicsStepsPerFrame", BetterWobblyWindows::DefaultPhysicsStepsPerFrame));
    _stiffness->setValue(group.readEntry("Stiffness", 15));
    _drag->setValue(group.readEntry("Drag", 80));
    _moveFactor->setValue(group.readEntry("MoveFactor", 10));
    if (_useDxBlurSettings->isChecked()) {
        ReadDxSettings();
    } else {
        SetBlurSettingsWidgets(BetterWobblyWindows::ReadEmbeddedBlurLocalSettings(config));
    }
    UpdateBlurInputState();
    AdvancedModeChanged(_advancedMode->isChecked());
    setNeedsSave(false);
}

void WobblyWindowsEffectConfig::save()
{
    const auto config = OpenKWinConfig();
    KConfigGroup group(config, QStringLiteral("Effect-better_wobblywindows"));
    WriteNotifyingEntry(group, "WobblynessLevel", _wobblyness->value());
    WriteNotifyingEntry(group,
        "Settings",
        _advancedMode->isChecked() ? QStringLiteral("Custom") : QStringLiteral("Auto"));
    WriteNotifyingEntry(group, "MoveWobble", _moveWobble->isChecked());
    WriteNotifyingEntry(group, "ResizeWobble", _resizeWobble->isChecked());
    WriteNotifyingEntry(group, "VisualOppositePointFollowOnResize", _visualOppositePointFollowOnResize->isChecked());
    WriteNotifyingEntry(group, "VisualOppositePointFollowStrengthPercent", _visualOppositePointFollowStrength->value());
    WriteNotifyingEntry(group, "AdvancedMode", _advancedMode->isChecked());
    WriteNotifyingEntry(group, "Stiffness", _stiffness->value());
    WriteNotifyingEntry(group, "Drag", _drag->value());
    WriteNotifyingEntry(group, "MoveFactor", _moveFactor->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlur", _embeddedDXBlur->isChecked());
    WriteNotifyingEntry(group, "EmbeddedDXBlurApiOnly", _apiOnlyBlur->isChecked());
    WriteNotifyingEntry(group, "EmbeddedDXBlurUseBetterBlurDXSettings", _useDxBlurSettings->isChecked());
    WriteNotifyingEntry(group, "EmbeddedDXBlurStrength", _blurStrength->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlurSaturation", _saturation->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlurContrast", _contrast->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlurBrightness", _brightness->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlurNoiseStrength", _noiseStrength->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlurCornerRadius", _cornerRadius->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlurColorMode", _colorMode->currentData().toInt());
    WriteNotifyingEntry(group, "EmbeddedDXBlurNoiseMode", _noiseMode->currentData().toInt());
    WriteNotifyingEntry(group, "EmbeddedDXBlurHandoffFrames", _handoffFrames->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlurEdgeSoftnessPixels", _edgeSoftness->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlurClearIntermediateTargets", _clearIntermediates->isChecked());
    WriteNotifyingEntry(group, "EmbeddedDXBlurFallbackFrames", _fallbackFrames->value());
    WriteNotifyingEntry(group, "EmbeddedDXBlurSourceYFlip", _sourceYFlip->isChecked());
    WriteNotifyingEntry(group, "MaxPhysicsStepsPerFrame", _maxPhysicsSteps->value());

    // Sync before asking KWin to reconfigure. The Notify write flag wakes the
    // effect's KConfigWatcher, while reconfigureEffect is the standard immediate
    // path used by built-in KWin effect KCMs.
    group.sync();
    config->sync();
    KCModule::save();

    QDBusInterface effectsInterface(QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QDBusConnection::sessionBus());
    const QDBusMessage reply = effectsInterface.call(QDBus::Block,
        QStringLiteral("reconfigureEffect"),
        QStringLiteral("better_wobblywindows"));

    // Older or distribution-patched KWin builds may not expose the per-effect
    // method. In that case, fall back to KWin's global reconfigure method.
    if (!effectsInterface.isValid() || reply.type() == QDBusMessage::ErrorMessage) {
        QDBusInterface kwinInterface(QStringLiteral("org.kde.KWin"),
            QStringLiteral("/KWin"),
            QStringLiteral("org.kde.KWin"),
            QDBusConnection::sessionBus());
        kwinInterface.call(QDBus::NoBlock, QStringLiteral("reconfigure"));
    }

    setNeedsSave(false);
}

void WobblyWindowsEffectConfig::defaults()
{
    _wobblyness->setValue(0);
    _moveWobble->setChecked(true);
    _resizeWobble->setChecked(true);
    _visualOppositePointFollowOnResize->setChecked(false);
    _visualOppositePointFollowOnResize->setEnabled(true);
    _visualOppositePointFollowStrength->setValue(BetterWobblyWindows::DefaultOppositePointFollowStrengthPercent);
    _visualOppositePointFollowStrength->setEnabled(false);
    _advancedMode->setChecked(false);
    _embeddedDXBlur->setChecked(true);
    _apiOnlyBlur->setChecked(false);
    _useDxBlurSettings->setChecked(true);
    SetComboValue(_colorMode, 0);
    SetComboValue(_noiseMode, 0);
    _handoffFrames->setValue(BetterWobblyWindows::DefaultHandoffFrames);
    _edgeSoftness->setValue(0);
    _clearIntermediates->setChecked(false);
    _fallbackFrames->setValue(BetterWobblyWindows::DefaultFallbackFrames);
    _sourceYFlip->setChecked(BetterWobblyWindows::DefaultSourceYFlip);
    _maxPhysicsSteps->setValue(6);
    SetPreset(0);
    SetBlurSettingsWidgets(_useDxBlurSettings->isChecked()
            ? BetterWobblyWindows::ReadBetterBlurDxSettings(OpenKWinConfig())
            : BetterWobblyWindows::DxBlurVisualSettings());
    UpdateBlurInputState();
    MarkChanged();
}

} // namespace KWin

#include "WobblyWindowsEffectConfig.moc"
