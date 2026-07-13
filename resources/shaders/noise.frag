/*
    SPDX-FileCopyrightText: 2024-2026 Better Blur DX contributors (notably Xarblu)
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later

    Adapted from Better Blur DX shader code:
    https://github.com/xarblu/kwin-effects-better-blur-dx
    Reference snapshot: 35451011b48bd01c2afa1a58ceeac46197147b81
*/
#extension GL_OES_standard_derivatives : enable

uniform sampler2D texUnit;
uniform vec2 noiseTextureSize;
uniform vec2 blurTextureSize;
uniform float renderScale;
uniform vec4 box;
uniform vec4 cornerRadius;
uniform float opacity;
uniform int noiseMode;
varying vec2 uv;
varying vec2 vertex;

float sdfRoundedBox(vec2 position, vec2 boxTopLeft, vec2 boxSize, vec4 radius)
{
    vec2 p = position - boxTopLeft - boxSize * 0.5;
    vec2 b = boxSize * 0.5;
    radius.xy = (p.x > 0.0) ? radius.xy : radius.zw;
    radius.x = (p.y > 0.0) ? radius.x : radius.y;
    vec2 q = abs(p) - b + radius.x;
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius.x;
}

void main(void)
{
    // Better Blur DX adds noise in its logical-pixel cache FBO and then draws
    // that cache to screen. Reconstruct those FBO fragment coordinates from the
    // deformed mesh instead of using device-space gl_FragCoord directly.
    vec2 cacheCoord = vec2(vertex.x / renderScale,
                           blurTextureSize.y - vertex.y / renderScale);
    vec2 uvNoise = cacheCoord / noiseTextureSize;
    float noiseValue = texture2D(texUnit, uvNoise).r * opacity;

    // mode 0: DX-identical additive noise (RGB only, alpha 0, GL_ONE/GL_ONE).
    // mode 1: non-additive alpha overlay for stacks that flash on additive noise.
    vec4 fragColor = (noiseMode == 0)
        ? vec4(vec3(noiseValue), 0.0)
        : vec4(vec3(noiseValue), noiseValue);

    float maxRadius = max(max(cornerRadius.x, cornerRadius.y), max(cornerRadius.z, cornerRadius.w));
    if (maxRadius > 0.0) {
        float f = sdfRoundedBox(uv, box.xy, box.zw, cornerRadius);
        float df = max(fwidth(f), 0.001);
        fragColor *= 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
    }
    gl_FragColor = fragColor;
}
