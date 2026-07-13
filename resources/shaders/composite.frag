/*
    Final Better Blur DX cache composite adapted for the deformed Wobbly mesh.
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#extension GL_OES_standard_derivatives : enable

uniform sampler2D texUnit;
uniform vec2 textureSize;
uniform vec4 sourceClamp;
uniform float edgeSoftness;
uniform vec4 box;
uniform vec4 cornerRadius;
uniform float opacity;
uniform bool sourceYFlip;

varying vec2 uv;
varying vec2 vertex;

vec4 effectiveSourceClamp()
{
    if (!sourceYFlip) {
        return sourceClamp;
    }
    float y0 = clamp(1.0 - sourceClamp.w, 0.0, 1.0);
    float y1 = clamp(1.0 - sourceClamp.y, 0.0, 1.0);
    return vec4(sourceClamp.x, min(y0, y1), sourceClamp.z, max(y0, y1));
}

float sourceEdgeFade(vec2 coord, vec4 clampBounds)
{
    if (edgeSoftness <= 0.0) {
        return 1.0;
    }
    vec2 softness = vec2(edgeSoftness);
    vec2 low = smoothstep(clampBounds.xy, clampBounds.xy + softness, coord);
    vec2 high = 1.0 - smoothstep(clampBounds.zw - softness, clampBounds.zw, coord);
    return clamp(min(min(low.x, low.y), min(high.x, high.y)), 0.0, 1.0);
}

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
    vec2 sourceUv = vertex / textureSize;
    if (sourceYFlip) {
        sourceUv.y = 1.0 - sourceUv.y;
    }
    vec4 clampBounds = effectiveSourceClamp();
    vec4 result = texture2D(texUnit, clamp(sourceUv, clampBounds.xy, clampBounds.zw)) * opacity;
    result *= sourceEdgeFade(sourceUv, clampBounds);

    float maxRadius = max(max(cornerRadius.x, cornerRadius.y), max(cornerRadius.z, cornerRadius.w));
    if (maxRadius > 0.0) {
        float f = sdfRoundedBox(uv, box.xy, box.zw, cornerRadius);
        float df = max(fwidth(f), 0.001);
        result *= 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
    }
    gl_FragColor = result;
}
