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

#include <epoxy/gl.h>

namespace KWin::BetterWobblyWindows
{
class ScopedEmbeddedBlurGlState
{
  public:
    ScopedEmbeddedBlurGlState();
    ~ScopedEmbeddedBlurGlState();

    ScopedEmbeddedBlurGlState(const ScopedEmbeddedBlurGlState &) = delete;
    ScopedEmbeddedBlurGlState &operator=(const ScopedEmbeddedBlurGlState &) = delete;

  private:
    const GLboolean _blendEnabled;
    const GLboolean _scissorEnabled;
    const GLboolean _depthEnabled;
    const GLboolean _stencilEnabled;
    const GLboolean _cullEnabled;
    const GLboolean _framebufferSrgbEnabled;
    GLboolean _colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLint _blendSrcRgb = GL_ONE;
    GLint _blendDstRgb = GL_ZERO;
    GLint _blendSrcAlpha = GL_ONE;
    GLint _blendDstAlpha = GL_ZERO;
    GLint _blendEquationRgb = GL_FUNC_ADD;
    GLint _blendEquationAlpha = GL_FUNC_ADD;
    GLint _scissorBox[4] = {0, 0, 0, 0};
    GLfloat _clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};
} // namespace KWin::BetterWobblyWindows
