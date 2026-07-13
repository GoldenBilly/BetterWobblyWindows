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
#include "WobblyWindowsEffect_p.h"

#include <QtGlobal>

namespace KWin
{

Q_LOGGING_CATEGORY(KWIN_WOBBLYWINDOWS, "kwin_effect_wobblywindows", QtWarningMsg)

WobblyWindowsEffect::WobblyWindowsEffect()
{
    _betterBlurDxProvider.apiVersion = BetterBlurDxApi::Version;
    _betterBlurDxProvider.structSize = sizeof(BetterBlurDxApi::MeshProvider);
    _betterBlurDxProvider.context = reinterpret_cast<quintptr>(this);
    _betterBlurDxProvider.BuildMesh = &WobblyWindowsEffect::BuildBetterBlurDxMesh;
    BetterWobblyWindows::WriteHardTraceLine("CTOR");
    if (BetterWobblyWindows::IsBlurTraceEnabled()) {
        BetterWobblyWindows::ResetBlurTraceFiles();
        BetterWobblyWindows::WriteBlurTraceLine(
            QStringLiteral("Better Blur DX API V1 effect constructed; trace file reset"));
    }
    WobblyWindowsConfig::Instance(effects->config());

    _configWatcher = KConfigWatcher::create(effects->config());
    connect(_configWatcher.data(),
        &KConfigWatcher::configChanged,
        this,
        [this](const KConfigGroup &group, const QByteArrayList &names) {
            Q_UNUSED(names);
            if (!BetterWobblyWindows::IsRelevantConfigGroupForHotReload(group.name())) {
                return;
            }

            effects->config()->reparseConfiguration();
            BetterWobblyWindows::WriteBlurTraceLine(QStringLiteral("hot-reload config group=%1").arg(group.name()));
            reconfigure(ReconfigureAll);
            effects->addRepaintFull();
        });

    InitEmbeddedBlurStrengthValues();
    // Shader initialization cannot depend on early effect discovery because
    // OffscreenEffect::supported() may still be false on a clean VM.
    // DrawEmbeddedBlur() still requires a valid render target blit and successful
    // FBO allocation, but skipping shader init here made the moving window fall
    // back to a sharp/no-blur translucent surface.
    InitEmbeddedBlur();
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &WobblyWindowsEffect::SlotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &WobblyWindowsEffect::CleanupWindow);

    const auto stackingOrder = effects->stackingOrder();
    for (EffectWindow *window : stackingOrder) {
        SlotWindowAdded(window);
    }
}

WobblyWindowsEffect::~WobblyWindowsEffect()
{
    // Plugin reload can destroy the effect while a window is still redirected.
    // Release those redirects explicitly instead of leaving cleanup to a partially
    // destructed OffscreenEffect.
    const auto activeWindows = _windows.keys();
    for (const EffectWindow *window : activeWindows) {
        if (window) {
            EffectWindow *w = const_cast<EffectWindow *>(window);
            // RequestRole contains a pointer to _betterBlurDxProvider. Clear it
            // before this object is destroyed so a provider can never call a
            // stale mesh callback during an effect reload.
            w->setData(BetterBlurDxApi::RequestRole, QVariant());
            w->setData(BetterBlurDxApi::ResultRole, QVariant());
            unredirect(w);
        }
    }
    _windows.clear();

    _embeddedBlurRenderData.clear();
}

bool WobblyWindowsEffect::Supported()
{
    const bool offscreenSupported = OffscreenEffect::supported();
    const bool animationsSupported = effects->animationsSupported();
    const bool strictSupport = qEnvironmentVariableIntValue("BETTER_WOBBLY_STRICT_SUPPORT") > 0;
    const bool forceSupport = qEnvironmentVariableIntValue("BETTER_WOBBLY_FORCE_SUPPORT") > 0;

    // Plasma's Desktop Effects KCM asks the plugin factory whether an effect is
    // supported before showing it. On clean virtual machines that query can happen
    // while KWin reports a temporarily incomplete/offscreen-limited GL stack, which
    // makes a correctly installed plugin look as if it does not exist. Advertise the
    // plugin by default; runtime capture still checks the actual render target and
    // FBO allocation before drawing embedded blur. Set BETTER_WOBBLY_STRICT_SUPPORT=1
    // to restore the upstream Wobbly Windows policy.
    const bool visibleByDefault = true;
    const bool supported =
        forceSupport || (strictSupport ? (offscreenSupported && animationsSupported) : visibleByDefault);

    BetterWobblyWindows::WriteBlurTraceLine(
        QStringLiteral("supported() offscreen=%1 animations=%2 strict=%3 force=%4 result=%5")
            .arg(offscreenSupported)
            .arg(animationsSupported)
            .arg(strictSupport)
            .arg(forceSupport)
            .arg(supported));
    return supported;
}

} // namespace KWin
