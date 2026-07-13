/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/TraceUtilities.h"
#include "TestSupport.h"

#include <QStringList>
#include <QtGlobal>

#include <unistd.h>

int main()
{
    using namespace KWin::BetterWobblyWindows;

    const uint userId = static_cast<uint>(::getuid());
    const QStringList paths = GetBlurTracePaths();
    CHECK(paths.size() == 2);
    CHECK(paths[0] == QStringLiteral("/tmp/better-wobbly-blur-trace-%1.log").arg(userId));
    CHECK(paths[1] == QStringLiteral("/run/user/%1/better-wobbly-blur-trace.log").arg(userId));
    CHECK(paths[0] != paths[1]);
    CHECK(paths[0].endsWith(QStringLiteral(".log")));
    CHECK(paths[1].endsWith(QStringLiteral(".log")));

    CHECK(GetTraceWindowId(nullptr) == QStringLiteral("0x0"));

    auto *smallAddress = reinterpret_cast<KWin::EffectWindow *>(static_cast<quintptr>(0x1234));
    CHECK(GetTraceWindowId(smallAddress) == QStringLiteral("0x1234"));

    const quintptr largeValue = sizeof(quintptr) == 8 ? static_cast<quintptr>(0x123456789abcdef0ULL) :
                                                        static_cast<quintptr>(0x7abcdef0UL);
    auto *largeAddress = reinterpret_cast<KWin::EffectWindow *>(largeValue);
    CHECK(GetTraceWindowId(largeAddress) == QStringLiteral("0x%1").arg(largeValue, 0, 16));

    CHECK(!WriteKernelLogLine(static_cast<const char *>(nullptr)));

    return 0;
}
