#version 140

/*
    SPDX-FileCopyrightText: 2024-2026 Better Blur DX contributors (notably Xarblu)
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later

    Adapted from Better Blur DX shader code:
    https://github.com/xarblu/kwin-effects-better-blur-dx
    Reference snapshot: 35451011b48bd01c2afa1a58ceeac46197147b81
*/

uniform sampler2D texUnit;
uniform vec2 noiseTextureSize;
uniform vec2 blurTextureSize;
uniform float renderScale;
uniform vec4 box;
uniform vec4 cornerRadius;
uniform float opacity;
uniform int noiseMode;
in vec2 uv;
in vec2 vertex;
out vec4 fragColor;

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
    vec2 cacheCoord = vec2(vertex.x / renderScale,
                           blurTextureSize.y - vertex.y / renderScale);
    vec2 uvNoise = cacheCoord / noiseTextureSize;
    float noiseValue = texture(texUnit, uvNoise).r * opacity;
    vec4 result = (noiseMode == 0)
        ? vec4(vec3(noiseValue), 0.0)
        : vec4(vec3(noiseValue), noiseValue);

    float maxRadius = max(max(cornerRadius.x, cornerRadius.y), max(cornerRadius.z, cornerRadius.w));
    if (maxRadius > 0.0) {
        float f = sdfRoundedBox(uv, box.xy, box.zw, cornerRadius);
        float df = max(fwidth(f), 0.001);
        result *= 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
    }
    fragColor = result;
}
