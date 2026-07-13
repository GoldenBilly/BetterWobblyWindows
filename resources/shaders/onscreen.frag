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
uniform mat4 colorMatrix;
uniform float offset;
uniform vec2 halfpixel;
uniform vec2 textureSize;
uniform vec4 sourceClamp;
uniform float edgeSoftness;
uniform vec4 box;
uniform vec4 cornerRadius;
uniform float opacity;
uniform bool sourceYFlip;
uniform bool refractionEnabled;
uniform vec2 refractionRectSize;
uniform float refractionEdgeSizePixels;
uniform float refractionCornerRadiusPixels;
uniform float refractionStrength;
uniform float refractionNormalPow;
uniform float refractionRGBFringing;
uniform int refractionTextureRepeatMode;
uniform int refractionMode;
varying vec2 uv;
varying vec2 vertex;

vec4 effectiveSourceClamp()
{
    if (!sourceYFlip) {
        return sourceClamp;
    }

    // If the sampled source is flipped vertically, the valid clamp/fade band must
    // be flipped too. Flipping only sourceUv makes the top edge use the bottom
    // clamp, which looks like an inverted Y axis and can fade the blur out while
    // dragging upward.
    float y0 = clamp(1.0 - sourceClamp.w, 0.0, 1.0);
    float y1 = clamp(1.0 - sourceClamp.y, 0.0, 1.0);
    return vec4(sourceClamp.x, min(y0, y1), sourceClamp.z, max(y0, y1));
}

vec4 sampleClamped(vec2 coord, vec4 clampBounds)
{
    return texture2D(texUnit, clamp(coord, clampBounds.xy, clampBounds.zw));
}

vec2 applyTextureRepeatMode(vec2 coord)
{
    if (refractionTextureRepeatMode == 0) {
        return clamp(coord, 0.0, 1.0);
    } else if (refractionTextureRepeatMode == 1) {
        vec2 flip = mod(coord, 2.0);
        vec2 result;
        result.x = (flip.x > 1.0) ? 1.0 - mod(coord.x, 1.0) : mod(coord.x, 1.0);
        result.y = (flip.y > 1.0) ? 1.0 - mod(coord.y, 1.0) : mod(coord.y, 1.0);
        return result;
    }
    return coord;
}

float roundedRectangleDist(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

vec4 refractionSample(vec2 sourceUv, vec4 clampBounds)
{
    vec2 offsets[8];
    offsets[0] = vec2(-halfpixel.x * 2.0, 0.0);
    offsets[1] = vec2(-halfpixel.x, halfpixel.y);
    offsets[2] = vec2(0.0, halfpixel.y * 2.0);
    offsets[3] = vec2(halfpixel.x, halfpixel.y);
    offsets[4] = vec2(halfpixel.x * 2.0, 0.0);
    offsets[5] = vec2(halfpixel.x, -halfpixel.y);
    offsets[6] = vec2(0.0, -halfpixel.y * 2.0);
    offsets[7] = vec2(-halfpixel.x, -halfpixel.y);
    float weights[8];
    weights[0] = 1.0; weights[1] = 2.0; weights[2] = 1.0; weights[3] = 2.0;
    weights[4] = 1.0; weights[5] = 2.0; weights[6] = 1.0; weights[7] = 2.0;

    vec2 halfRect = 0.5 * refractionRectSize;
    vec2 position = sourceUv * refractionRectSize - halfRect;
    float cornerR = min(refractionCornerRadiusPixels, min(halfRect.x, halfRect.y));
    float distConcave = roundedRectangleDist(position, halfRect, cornerR);
    float distBulge = roundedRectangleDist(position, halfRect, refractionEdgeSizePixels);
    vec2 coordR;
    vec2 coordG;
    vec2 coordB;

    if (refractionMode == 1) {
        float fringing = refractionRGBFringing * 0.3;
        float baseStrength = 0.2 * refractionStrength;
        float edgeProximity = clamp(1.0 + distConcave / refractionEdgeSizePixels, 0.0, 1.0);
        float shaped = sin(pow(edgeProximity, refractionNormalPow) * 1.57079632679);
        vec2 fromCenter = sourceUv - vec2(0.5);
        coordR = applyTextureRepeatMode(vec2(0.5) + fromCenter * (1.0 - shaped * baseStrength * (1.0 + fringing)));
        coordG = applyTextureRepeatMode(vec2(0.5) + fromCenter * (1.0 - shaped * baseStrength));
        coordB = applyTextureRepeatMode(vec2(0.5) + fromCenter * (1.0 - shaped * baseStrength * (1.0 - fringing)));
    } else {
        float concaveFactor = pow(clamp(1.0 + distBulge / refractionEdgeSizePixels, 0.0, 1.0), refractionNormalPow);
        const float h = 1.0;
        vec2 gradient = vec2(
            roundedRectangleDist(position + vec2(h, 0.0), halfRect, refractionEdgeSizePixels) - roundedRectangleDist(position - vec2(h, 0.0), halfRect, refractionEdgeSizePixels),
            roundedRectangleDist(position + vec2(0.0, h), halfRect, refractionEdgeSizePixels) - roundedRectangleDist(position - vec2(0.0, h), halfRect, refractionEdgeSizePixels));
        vec2 normal = length(gradient) > 1e-6 ? -normalize(gradient) : vec2(0.0, 1.0);
        float finalStrength = 0.2 * concaveFactor * refractionStrength;
        float fringingFactor = refractionRGBFringing * 0.3;
        coordR = applyTextureRepeatMode(sourceUv - normal * finalStrength * (1.0 + fringingFactor));
        coordG = applyTextureRepeatMode(sourceUv - normal * finalStrength);
        coordB = applyTextureRepeatMode(sourceUv - normal * finalStrength * (1.0 - fringingFactor));
    }

    vec4 result = vec4(0.0);
    for (int i = 0; i < 8; ++i) {
        vec2 sampleOffset = offsets[i] * offset;
        result.r += sampleClamped(coordR + sampleOffset, clampBounds).r * weights[i];
        result.g += sampleClamped(coordG + sampleOffset, clampBounds).g * weights[i];
        result.b += sampleClamped(coordB + sampleOffset, clampBounds).b * weights[i];
        result.a += sampleClamped(coordG + sampleOffset, clampBounds).a * weights[i];
    }
    return result / 12.0;
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
    // `vertex` is the deformed position inside the stable blur capture texture.
    // `uv` is intentionally not the sampling uv for this pass; for mesh vertices
    // it carries the original, undeformed window-local coordinate for the mask.
    vec2 sourceUv = vec2(vertex.x / textureSize.x, vertex.y / textureSize.y);
    if (sourceYFlip) {
        sourceUv.y = 1.0 - sourceUv.y;
    }
    vec4 clampBounds = effectiveSourceClamp();

    vec4 sum = refractionEnabled
        ? refractionSample(sourceUv, clampBounds) * 12.0
        : sampleClamped(sourceUv + vec2(-halfpixel.x * 2.0, 0.0) * offset, clampBounds);
    if (!refractionEnabled) {
        sum += sampleClamped(sourceUv + vec2(-halfpixel.x, halfpixel.y) * offset, clampBounds) * 2.0;
        sum += sampleClamped(sourceUv + vec2(0.0, halfpixel.y * 2.0) * offset, clampBounds);
        sum += sampleClamped(sourceUv + vec2(halfpixel.x, halfpixel.y) * offset, clampBounds) * 2.0;
        sum += sampleClamped(sourceUv + vec2(halfpixel.x * 2.0, 0.0) * offset, clampBounds);
        sum += sampleClamped(sourceUv + vec2(halfpixel.x, -halfpixel.y) * offset, clampBounds) * 2.0;
        sum += sampleClamped(sourceUv + vec2(0.0, -halfpixel.y * 2.0) * offset, clampBounds);
        sum += sampleClamped(sourceUv + vec2(-halfpixel.x, -halfpixel.y) * offset, clampBounds) * 2.0;
    }

    // Keep the color path byte-for-byte equivalent to Better Blur DX: the
    // upstream onscreen shader writes (sum / 12.0) * colorMatrix. Do not clamp
    // RGB and do not premultiply here, otherwise saturation/brightness during
    // Wobbly drag no longer matches the original DX blur.
    vec4 fragColor = (sum / 12.0) * colorMatrix;
    fragColor *= opacity;
    fragColor *= sourceEdgeFade(sourceUv, clampBounds);

    float maxRadius = max(max(cornerRadius.x, cornerRadius.y), max(cornerRadius.z, cornerRadius.w));
    if (maxRadius > 0.0) {
        float f = sdfRoundedBox(uv, box.xy, box.zw, cornerRadius);
        float df = max(fwidth(f), 0.001);
        fragColor *= 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
    }
    gl_FragColor = fragColor;
}
