/*
    Portions of this file are adapted from Better Blur DX and the KWin Blur effect.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
    SPDX-FileCopyrightText: 2024-2026 Better Blur DX contributors
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later

    Better Blur DX source: https://github.com/xarblu/kwin-effects-better-blur-dx
    See AUTHORS.md and NOTICE.md for the complete provenance record.
*/
#include "BetterWobblyWindows/ScopedEmbeddedBlurGlState.h"

namespace KWin::BetterWobblyWindows
{
namespace
{
GLboolean IsFramebufferSrgbEnabled()
{
#ifdef GL_FRAMEBUFFER_SRGB
    return glIsEnabled(GL_FRAMEBUFFER_SRGB);
#else
    return GL_FALSE;
#endif
}
} // namespace

ScopedEmbeddedBlurGlState::ScopedEmbeddedBlurGlState()
    : _blendEnabled(glIsEnabled(GL_BLEND)), _scissorEnabled(glIsEnabled(GL_SCISSOR_TEST)),
      _depthEnabled(glIsEnabled(GL_DEPTH_TEST)), _stencilEnabled(glIsEnabled(GL_STENCIL_TEST)),
      _cullEnabled(glIsEnabled(GL_CULL_FACE)), _framebufferSrgbEnabled(IsFramebufferSrgbEnabled())
{
    glGetIntegerv(GL_BLEND_SRC_RGB, &_blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &_blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &_blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &_blendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &_blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &_blendEquationAlpha);
    glGetIntegerv(GL_SCISSOR_BOX, _scissorBox);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, _clearColor);
    glGetBooleanv(GL_COLOR_WRITEMASK, _colorMask);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
}

ScopedEmbeddedBlurGlState::~ScopedEmbeddedBlurGlState()
{
    glBlendFuncSeparate(_blendSrcRgb, _blendDstRgb, _blendSrcAlpha, _blendDstAlpha);
    glBlendEquationSeparate(_blendEquationRgb, _blendEquationAlpha);
    if (_blendEnabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    if (_scissorEnabled) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(_scissorBox[0], _scissorBox[1], _scissorBox[2], _scissorBox[3]);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
    if (_depthEnabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    if (_stencilEnabled) {
        glEnable(GL_STENCIL_TEST);
    } else {
        glDisable(GL_STENCIL_TEST);
    }
    if (_cullEnabled) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }
#ifdef GL_FRAMEBUFFER_SRGB
    if (_framebufferSrgbEnabled) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    } else {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
#endif
    glColorMask(_colorMask[0], _colorMask[1], _colorMask[2], _colorMask[3]);
    glClearColor(_clearColor[0], _clearColor[1], _clearColor[2], _clearColor[3]);
}
} // namespace KWin::BetterWobblyWindows
