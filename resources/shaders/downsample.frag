/*
    SPDX-FileCopyrightText: 2024-2026 Better Blur DX contributors (notably taj-ny and Xarblu)
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later

    Adapted from Better Blur DX shader code:
    https://github.com/xarblu/kwin-effects-better-blur-dx
    Reference snapshot: 35451011b48bd01c2afa1a58ceeac46197147b81
*/
uniform sampler2D texUnit;
uniform float offset;
uniform vec2 halfpixel;
uniform vec4 sourceClamp;
varying vec2 uv;

vec4 sampleClamped(vec2 coord)
{
    return texture2D(texUnit, clamp(coord, sourceClamp.xy, sourceClamp.zw));
}

void main(void)
{
    vec4 sum = sampleClamped(uv) * 4.0;
    sum += sampleClamped(uv - halfpixel.xy * offset);
    sum += sampleClamped(uv + halfpixel.xy * offset);
    sum += sampleClamped(uv + vec2(halfpixel.x, -halfpixel.y) * offset);
    sum += sampleClamped(uv - vec2(halfpixel.x, -halfpixel.y) * offset);
    gl_FragColor = sum / 8.0;
}
