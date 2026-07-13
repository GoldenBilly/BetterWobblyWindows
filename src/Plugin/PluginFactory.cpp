/*
    Portions of this file are adapted from the KDE KWin effect factory.

    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/TraceUtilities.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"
#include "BetterWobblyWindows/WobblyWindowsEffect.h"

namespace
{
struct PluginFactoryLoadTrace {
    PluginFactoryLoadTrace()
    {
        using namespace KWin::BetterWobblyWindows;
        WriteKernelLogLine(PluginFactoryTraceMagic, KernelLogPriority::Info);
    }
};

static PluginFactoryLoadTrace pluginFactoryLoadTrace;
} // namespace

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(WobblyWindowsEffect, "metadata.json", {
    BetterWobblyWindows::WriteKernelLogLine("factory supported() check entered",
        BetterWobblyWindows::KernelLogPriority::Debug);
    return WobblyWindowsEffect::Supported();
})

} // namespace KWin

#include "PluginFactory.moc"
