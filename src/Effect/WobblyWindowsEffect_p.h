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
#pragma once

#include "BetterWobblyWindows/WobblyWindowsEffect.h"
#include "BetterWobblyWindows/BetterBlurDxApi.h"
#include "BetterWobblyWindows/WobblyWindowsConfigStore.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"
#include "BetterWobblyWindows/WobblyParameterSet.h"
#include "BetterWobblyWindows/EmbeddedBlurMath.h"
#include "BetterWobblyWindows/ScopedEmbeddedBlurGlState.h"
#include "BetterWobblyWindows/TraceUtilities.h"

#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glframebuffer.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"

#include <KConfigGroup>

#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QScreen>
#include <QStringList>
#include <QTextStream>
#include <QTime>
#include <QVariant>
#include <QVector4D>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <utility>
