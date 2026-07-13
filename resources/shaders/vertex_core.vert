#version 140

/*
    SPDX-FileCopyrightText: 2024-2026 Better Blur DX contributors (notably taj-ny)
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later

    Adapted from Better Blur DX shader code:
    https://github.com/xarblu/kwin-effects-better-blur-dx
    Reference snapshot: 35451011b48bd01c2afa1a58ceeac46197147b81
*/

uniform mat4 modelViewProjectionMatrix;
in vec2 position;
in vec2 texcoord;
out vec2 uv;
out vec2 vertex;

void main(void)
{
    gl_Position = modelViewProjectionMatrix * vec4(position, 0.0, 1.0);
    uv = texcoord;
    vertex = position;
}
