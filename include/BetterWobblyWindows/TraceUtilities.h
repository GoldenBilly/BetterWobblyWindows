/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QString>
#include <QStringList>

namespace KWin
{
class EffectWindow;
}

namespace KWin::BetterWobblyWindows
{
enum class KernelLogPriority : int {
    Error = 3,
    Warning = 4,
    Notice = 5,
    Info = 6,
    Debug = 7,
};

// Writes directly to /dev/kmsg, so successful messages are visible in dmesg.
// This is best-effort: many distributions only allow root to write to /dev/kmsg.
bool WriteKernelLogLine(const QString &line, KernelLogPriority priority = KernelLogPriority::Info);
bool WriteKernelLogLine(const char *line, KernelLogPriority priority = KernelLogPriority::Info);

void WriteHardTraceLine(const char *line);
QString GetTraceWindowId(EffectWindow *window);
QStringList GetBlurTracePaths();
void WriteBlurTraceLine(const QString &line);
bool IsBlurTraceEnabled();
bool IsTraceFileOutputEnabled();
void ResetBlurTraceFiles();
} // namespace KWin::BetterWobblyWindows
