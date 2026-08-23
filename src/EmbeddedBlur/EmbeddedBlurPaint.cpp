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
#include "../Effect/WobblyWindowsEffect_p.h"

#include <cstddef>

namespace KWin
{

bool WobblyWindowsEffect::BuildBetterBlurDxMesh(
    const quintptr context, const BetterBlurDxApi::MeshBuildInput *input, BetterBlurDxApi::MeshView *output)
{
    if (!context || !input || !output || input->renderScale <= 0.0 || input->textureWidth <= 0 ||
        input->textureHeight <= 0) {
        return false;
    }

    auto *effect = reinterpret_cast<WobblyWindowsEffect *>(context);
    auto *w = reinterpret_cast<EffectWindow *>(input->effectWindow);
    auto *data = reinterpret_cast<WindowPaintData *>(input->windowPaintData);
    if (!effect || !w || !data) {
        return false;
    }

    const auto infoIt = effect->_windows.constFind(w);
    if (infoIt == effect->_windows.constEnd() || !effect->ShouldPaintEmbeddedBlur(w)) {
        return false;
    }

    const WindowQuadList originalQuads =
        effect->MakeWindowQuads(w).makeRegularGrid(static_cast<int>(effect->_xTessellation),
            static_cast<int>(effect->_yTessellation));
    WindowQuadList quads = originalQuads;
    if (!effect->DeformWindowQuads(w, *data, quads, false, true, true)) {
        return false;
    }

    const long quadCount = std::min(quads.count(), originalQuads.count());
    if (quadCount <= 0) {
        return false;
    }

    const qreal scale = input->renderScale;
    const qreal windowLogicalX = w->x() + data->xTranslation();
    const qreal windowLogicalY = w->y() + data->yTranslation();
    auto &vertices = effect->_betterBlurDxProviderVertices;
    vertices.clear();
    vertices.reserve(static_cast<std::size_t>(quadCount * BetterWobblyWindows::QuadTriangleVertexCount));

    const auto writeVertex = [&](const WindowVertex &deformed, const WindowVertex &original) {
        const qreal deformedLogicalX = windowLogicalX + deformed.x() * data->xScale();
        const qreal deformedLogicalY = windowLogicalY + deformed.y() * data->yScale();
        const qreal sourceLogicalX = windowLogicalX + original.x() * data->xScale();
        const qreal sourceLogicalY = windowLogicalY + original.y() * data->yScale();

        BetterBlurDxApi::MeshVertex vertex;
        vertex.x = static_cast<float>(deformedLogicalX * scale);
        vertex.y = static_cast<float>(deformedLogicalY * scale);
        vertex.cacheX = static_cast<float>(sourceLogicalX - input->cacheOriginX);
        vertex.cacheY = static_cast<float>(sourceLogicalY - input->cacheOriginY);
        vertices.push_back(vertex);
    };

    for (int qi = 0; qi < quadCount; ++qi) {
        const WindowQuad &quad = quads[qi];
        const WindowQuad &original = originalQuads[qi];
        writeVertex(quad[0], original[0]);
        writeVertex(quad[2], original[2]);
        writeVertex(quad[3], original[3]);
        writeVertex(quad[0], original[0]);
        writeVertex(quad[1], original[1]);
        writeVertex(quad[2], original[2]);
    }

    output->vertices = vertices.data();
    output->vertexCount = static_cast<int>(vertices.size());
    const bool physicsActive = infoIt->status == Moving || infoIt->wobblying;
    // Provider-owned blur must be fully visible from the first interactive
    // frame because another transition would compound Better Blur DX's own
    // transformed opacity. Retain smoothing only for post-motion fade-out.
    output->opacity = physicsActive
        ? 1.0f
        : static_cast<float>(BetterWobblyWindows::SmoothBlurTransition(infoIt->embeddedBlurTransitionProgress));
    return output->vertices && output->vertexCount > 0 && output->opacity > 0.0f;
}

void WobblyWindowsEffect::DrawEmbeddedBlur(const RenderTarget &renderTarget,
    const RenderViewport &viewport,
    EffectWindow *w,
    int mask,
    const Region &deviceRegion,
    WindowPaintData &data)
{
    Q_UNUSED(mask)

    // KWin can invoke drawWindow() for a partial device-space repaint. The
    // embedded blur is drawn before OffscreenEffect draws the actual window, so
    // painting the whole blur mesh here would overwrite already valid window
    // pixels outside the current damage. A fully opaque window then appears to
    // flash because only the damaged part of its redirected texture is painted
    // back over that blur in this frame. Better Blur DX avoids the same problem
    // by intersecting its effective blur shape with deviceRegion.
    const bool clipOnscreenToDamage = deviceRegion != Region::infinite();
    if (clipOnscreenToDamage && deviceRegion.isEmpty()) {
        return;
    }

    const bool shouldPaint = ShouldPaintEmbeddedBlur(w);
    if (BetterWobblyWindows::IsBlurTraceEnabled()) {
        BetterWobblyWindows::WriteBlurTraceLine(QStringLiteral(
            "drawEmbeddedBlur entry window=%1 enabled=%2 valid=%3 shouldPaint=%4 hasRenderTargetTexture=%5 scale=%6")
                .arg(BetterWobblyWindows::GetTraceWindowId(w))
                .arg(_embeddedBlurEnabled)
                .arg(_embeddedBlurValid)
                .arg(shouldPaint)
                .arg(renderTarget.texture() != nullptr)
                .arg(viewport.scale()));
        qCWarning(KWIN_WOBBLYWINDOWS) << "drawEmbeddedBlur entry"
                                      << "window" << w << "enabled" << _embeddedBlurEnabled << "valid"
                                      << _embeddedBlurValid << "shouldPaint" << shouldPaint << "renderTargetTexture"
                                      << (renderTarget.texture() != nullptr) << "scale" << viewport.scale();
    }

    if (!_embeddedBlurEnabled || !_embeddedBlurFallbackEnabled || !shouldPaint) {
        return;
    }

    // Standalone fallback. This is reached only when the Better Blur DX API V1
    // provider is unavailable or explicitly declines the current frame.
    if (!_embeddedBlurValid) {
        return;
    }

    auto windowInfoIt = _windows.find(w);
    WindowWobblyInfos *windowInfo = windowInfoIt == _windows.end() ? nullptr : &windowInfoIt.value();
    const bool allowInteractiveSourceHold = windowInfo && windowInfo->status == Moving;
    const auto noteSourceMiss = [&]() {
        if (windowInfo) {
            ++windowInfo->embeddedBlurSourceMissFrames;
        }
    };
    const auto noteSourceReady = [&]() {
        if (windowInfo) {
            windowInfo->embeddedBlurSourceMissFrames = 0;
        }
    };

    // Better Blur DX applies the effective window opacity twice when the cached
    // blur is composited. Preserve that modulation and apply a smooth, reversible
    // state transition on top. Only the embedded layer fades; the blur kernel,
    // color matrix, source capture and wobble mesh remain untouched.
    const float windowOpacity = std::clamp(static_cast<float>(data.opacity()), 0.0f, 1.0f);
    const bool physicsActive = windowInfo && (windowInfo->status == Moving || windowInfo->wobblying);
    const float transitionOpacity = physicsActive
        ? 1.0f
        : (windowInfo ? static_cast<float>(
                            BetterWobblyWindows::SmoothBlurTransition(windowInfo->embeddedBlurTransitionProgress))
                      : 1.0f);
    const float embeddedOpacity = windowOpacity * windowOpacity * transitionOpacity;

    // Prefer a fresh capture but keep drawing the last confirmed source for a
    // few transient frames if KWin gives this frame an empty/uncapturable source.
    // This is what prevents the moving blur from disappearing during downward
    // drags or while crossing the bottom of an output.
    // GLFramebuffer::blitFromRenderTarget() also supports render targets that
    // don't expose a texture. Better Blur DX still captures those and simply
    // falls back to GL_RGBA8 for its own pyramid format.
    // Build the regular grid once because both deformation and mask coordinates
    // use the same tessellation.
    const WindowQuadList originalQuads =
        MakeWindowQuads(w).makeRegularGrid(static_cast<int>(_xTessellation), static_cast<int>(_yTessellation));
    WindowQuadList quads = originalQuads;
    if (!DeformWindowQuads(w, data, quads, false, true, true)) {
        return;
    }

    const qreal scale = viewport.scale();
    const qreal windowLogicalX = w->x() + data.xTranslation();
    const qreal windowLogicalY = w->y() + data.yTranslation();
    const qreal windowLogicalWidth = w->width() * data.xScale();
    const qreal windowLogicalHeight = w->height() * data.yScale();

    qreal localLeft = 0.0;
    qreal localTop = 0.0;
    qreal localRight = 0.0;
    qreal localBottom = 0.0;
    bool hasBounds = false;
    for (const WindowQuad &quad : std::as_const(quads)) {
        for (int i = 0; i < 4; ++i) {
            const WindowVertex &v = quad[i];
            const QPointF p(v.x() * data.xScale(), v.y() * data.yScale());
            if (!hasBounds) {
                localLeft = localRight = p.x();
                localTop = localBottom = p.y();
                hasBounds = true;
            } else {
                localLeft = std::min(localLeft, p.x());
                localTop = std::min(localTop, p.y());
                localRight = std::max(localRight, p.x());
                localBottom = std::max(localBottom, p.y());
            }
        }
    }

    const RectF localBounds(localLeft,
        localTop,
        std::max<qreal>(0.0, localRight - localLeft),
        std::max<qreal>(0.0, localBottom - localTop));

    if (!hasBounds || localBounds.isEmpty()) {
        if (BetterWobblyWindows::IsBlurTraceEnabled()) {
            BetterWobblyWindows::WriteBlurTraceLine(
                QStringLiteral("drawEmbeddedBlur ABORT empty logical bounds window=%1")
                    .arg(BetterWobblyWindows::GetTraceWindowId(w)));
            qCWarning(KWIN_WOBBLYWINDOWS) << "drawEmbeddedBlur aborted: empty logical bounds" << "window" << w;
        }
        return;
    }

    const RectF logicalBounds = localBounds.translated(windowLogicalX, windowLogicalY);

    // The final DX cache is sampled through a moving, deformed mesh. It needs a
    // guard band around that mesh, or the last Kawase taps hit the allocation
    // edge and produce the hard rectangular cut reported during strong wobble.
    // The guard band belongs to the capture/cache only; the visible shape and
    // rounded mask remain the original window mesh.
    const int blurKernelPadding = std::max(_embeddedBlurExpandSize * 2,
        static_cast<int>(std::ceil((_embeddedBlurOffset + BetterWobblyWindows::BlurKernelOffsetBias) *
            BetterWobblyWindows::BlurKernelScaleMultiplier)));
    const int wobblePadding = std::clamp(static_cast<int>(std::ceil(std::max(windowLogicalWidth, windowLogicalHeight) *
                                             BetterWobblyWindows::WobblePaddingWindowRatio)),
        BetterWobblyWindows::MinimumWobblePaddingPixels,
        BetterWobblyWindows::MaximumWobblePaddingPixels);
    const int capturePadding = blurKernelPadding + wobblePadding;

    QRect desiredLocalBackgroundRect =
        localBounds.adjusted(-capturePadding, -capturePadding, capturePadding, capturePadding).toAlignedRect();
    const QRect undeformedLocalBackgroundRect =
        RectF(0.0, 0.0, windowLogicalWidth, windowLogicalHeight)
            .adjusted(-capturePadding, -capturePadding, capturePadding, capturePadding)
            .toAlignedRect();
    desiredLocalBackgroundRect = desiredLocalBackgroundRect.united(undeformedLocalBackgroundRect);

    EmbeddedBlurRenderData &renderData = _embeddedBlurRenderData[w];
    const QSize sourceWindowSize(
        std::max(BetterWobblyWindows::MinimumTextureDimension, static_cast<int>(std::ceil(windowLogicalWidth))),
        std::max(BetterWobblyWindows::MinimumTextureDimension, static_cast<int>(std::ceil(windowLogicalHeight))));
    if (!renderData.localBackgroundRect.isValid() || renderData.sourceWindowSize != sourceWindowSize) {
        renderData.localBackgroundRect = desiredLocalBackgroundRect;
        renderData.sourceWindowSize = sourceWindowSize;
    } else if (!renderData.localBackgroundRect.contains(desiredLocalBackgroundRect)) {
        renderData.localBackgroundRect = renderData.localBackgroundRect.united(desiredLocalBackgroundRect);
    }

    const QRect unclippedBackgroundRect =
        renderData.localBackgroundRect.translated(static_cast<int>(std::floor(windowLogicalX)),
            static_cast<int>(std::floor(windowLogicalY)));
    const QRect renderLogicalRect = viewport.renderRect().toAlignedRect();

    // Keep the blur pyramid tied to the complete logical rect. Crossing an output
    // edge clips only the source blit; it must not resize the DX-sized pyramid.
    const QRect backgroundRect = unclippedBackgroundRect;
    const QRect clippedBackgroundRect =
        BetterWobblyWindows::ClipEmbeddedBlurCaptureRectToRenderTarget(backgroundRect, renderLogicalRect);

    // Keep KWin invalidating the full source area that the embedded blur wants,
    // plus the actual visible capture footprint. Repaint both the current and
    // previous footprints, otherwise a fast downward drag can enter pixels that
    // were not scheduled for redraw and the blur source can go empty for a frame.
    const int repaintPadding = std::max(1, capturePadding);
    _updateRegion = _updateRegion.united(
        unclippedBackgroundRect.adjusted(-repaintPadding, -repaintPadding, repaintPadding, repaintPadding));
    _updateRegion =
        _updateRegion.united(backgroundRect.adjusted(-repaintPadding, -repaintPadding, repaintPadding, repaintPadding));
    if (renderData.lastBackgroundRect.isValid()) {
        _updateRegion = _updateRegion.united(
            renderData.lastBackgroundRect.adjusted(-repaintPadding, -repaintPadding, repaintPadding, repaintPadding));
    }

    if (backgroundRect.isEmpty() || clippedBackgroundRect.isEmpty()) {
        if (BetterWobblyWindows::IsBlurTraceEnabled()) {
            BetterWobblyWindows::WriteBlurTraceLine(
                QStringLiteral("drawEmbeddedBlur ABORT empty background/capture rect window=%1")
                    .arg(BetterWobblyWindows::GetTraceWindowId(w)));
            qCWarning(KWIN_WOBBLYWINDOWS) << "drawEmbeddedBlur aborted: empty background/capture rect" << "window" << w;
        }
        return;
    }
    if (BetterWobblyWindows::IsBlurTraceEnabled()) {
        BetterWobblyWindows::WriteBlurTraceLine(
            QStringLiteral("drawEmbeddedBlur PAINTING window=%1 bg=%2,%3 %4x%5 texture=%6x%7")
                .arg(BetterWobblyWindows::GetTraceWindowId(w))
                .arg(backgroundRect.x())
                .arg(backgroundRect.y())
                .arg(backgroundRect.width())
                .arg(backgroundRect.height())
                .arg(std::max(BetterWobblyWindows::MinimumTextureDimension, backgroundRect.width()))
                .arg(std::max(BetterWobblyWindows::MinimumTextureDimension, backgroundRect.height())));
        qCWarning(KWIN_WOBBLYWINDOWS)
            << "drawEmbeddedBlur painting"
            << "window" << w << "bounds" << logicalBounds << "backgroundRect" << backgroundRect << "textureSize"
            << QSize(std::max(BetterWobblyWindows::MinimumTextureDimension, backgroundRect.width()),
                   std::max(BetterWobblyWindows::MinimumTextureDimension, backgroundRect.height()));
    }

    // This is deliberately *not* multiplied by viewport.scale(). Better Blur DX
    // allocates backgroundRect.size() and builds all Kawase levels from that
    // logical-pixel base. A device-pixel pyramid makes the same strength setting
    // visibly weaker at 125/150/200% scaling.
    const QSize requestedTextureSize(std::max(BetterWobblyWindows::MinimumTextureDimension, backgroundRect.width()),
        std::max(BetterWobblyWindows::MinimumTextureDimension, backgroundRect.height()));

    BetterWobblyWindows::ScopedEmbeddedBlurGlState embeddedBlurGlState;

    EmbeddedBlurRenderData &renderTargets = _embeddedBlurReusableRenderData;
    const QRect captureRect = clippedBackgroundRect;
    const bool hasPotentialFreshCapture = !captureRect.isEmpty();

    // Match Better Blur DX exactly: all intermediate textures inherit the current
    // compositor render target's internal format. Forcing GL_RGBA8 clips/quantizes
    // HDR or wide-gamut linear values before the color matrix and is the reason
    // the embedded blur can have a visibly different tint on Plasma 6.7.
    // During a held-source frame there is no fresh target to copy, so preserve the
    // already allocated format instead of reallocating and destroying that source.
    const unsigned int textureFormat = hasPotentialFreshCapture
        ? (renderTarget.texture() ? renderTarget.texture()->internalFormat() : GL_RGBA8)
        : (renderTargets.internalFormat != 0 ? renderTargets.internalFormat : GL_RGBA8);
    const bool canReuseHeldSourceWithoutCapture = !hasPotentialFreshCapture && renderTargets.hasValidSource &&
        renderTargets.lastSourceWindow == w && !renderTargets.size.isEmpty();
    const QSize textureRequest = canReuseHeldSourceWithoutCapture ? renderTargets.size : requestedTextureSize;
    if (!EnsureEmbeddedBlurRenderTargets(renderTargets, textureRequest, textureFormat)) {
        noteSourceMiss();
        return;
    }

    const QSize textureSize = renderTargets.size;

    const QVector4D roundedBox(0.0f,
        0.0f,
        static_cast<float>(windowLogicalWidth * scale),
        static_cast<float>(windowLogicalHeight * scale));

    // The SDF mask is evaluated in undeformed window-local coordinates
    // carried by the mesh vertices, not in screen-space. That keeps the blur
    // outline transformed with the wobbling window while preserving DX-style
    // rounded clipping when Better Blur DX has a CornerRadius configured.
    const auto roundedCornerRadiusValue = static_cast<float>(_embeddedBlurCornerRadius * scale);
    const QVector4D roundedCornerRadius(roundedCornerRadiusValue,
        roundedCornerRadiusValue,
        roundedCornerRadiusValue,
        roundedCornerRadiusValue);

    const QRect centerDest = captureRect.isEmpty() ? QRect() : captureRect.translated(-backgroundRect.topLeft());

    const auto clampForReadLevel = [](const QVector4D &sourceClamp, const int textureLevel) -> QVector4D {
        // The offscreen quad intentionally uses inverted Y texture coordinates.
        // Every blur pass therefore toggles the orientation of the written level.
        // For a fresh DX-style capture the clamp is the full texture; for held
        // sources this still preserves the stored clamp orientation correctly.
        return BetterWobblyWindows::ClampForTextureYOrientation(sourceClamp, (textureLevel % 2) != 0);
    };

    QVector4D sourceClamp(0.0f, 0.0f, 1.0f, 1.0f);
    QRect sourceBackgroundRect = backgroundRect;
    bool usingHeldSource = false;

    if (!captureRect.isEmpty() && !centerDest.isEmpty()) {
        renderTargets.framebuffers[0]->blitFromRenderTarget(renderTarget, viewport, captureRect, centerDest);
        // Keep every valid captured guard-band pixel available to the Kawase
        // taps. Clamping to the deformed window bounds discarded the padding and
        // created a thin smeared/cut strip along the top edge. At an output edge,
        // clamp only to the portion that this RenderView actually captured.
        sourceClamp = BetterWobblyWindows::NormalizedTextureClamp(captureRect, backgroundRect);

        renderTargets.lastBackgroundRect = backgroundRect;
        renderTargets.lastSourceClamp = sourceClamp;
        renderTargets.lastSourceWindow = w;
        renderTargets.lastRenderRect = renderLogicalRect;
        renderTargets.lastRenderScale = scale;
        renderTargets.hasValidSource = true;
        renderTargets.lastSourceWasClipped = captureRect != backgroundRect;
        renderTargets.fallbackFramesRemaining = _embeddedBlurFallbackFrames;
        renderData.lastBackgroundRect = backgroundRect;
        noteSourceReady();
    } else if (renderTargets.hasValidSource && renderTargets.lastSourceWindow == w &&
        renderTargets.lastBackgroundRect.isValid() && renderTargets.lastRenderRect == renderLogicalRect &&
        qFuzzyCompare(renderTargets.lastRenderScale + 1.0, scale + 1.0) && !renderTargets.lastSourceWasClipped &&
        (renderTargets.fallbackFramesRemaining > 0 || allowInteractiveSourceHold)) {
        sourceBackgroundRect = renderTargets.lastBackgroundRect;
        sourceClamp = renderTargets.lastSourceClamp;
        usingHeldSource = true;
        if (renderTargets.fallbackFramesRemaining > 0 && !allowInteractiveSourceHold) {
            --renderTargets.fallbackFramesRemaining;
        }
        noteSourceReady();
        if (BetterWobblyWindows::IsBlurTraceEnabled()) {
            BetterWobblyWindows::WriteBlurTraceLine(
                QStringLiteral("drawEmbeddedBlur HOLD source window=%1 remaining=%2 bg=%3,%4 %5x%6 render=%7,%8 %9x%10")
                    .arg(BetterWobblyWindows::GetTraceWindowId(w))
                    .arg(renderTargets.fallbackFramesRemaining)
                    .arg(backgroundRect.x())
                    .arg(backgroundRect.y())
                    .arg(backgroundRect.width())
                    .arg(backgroundRect.height())
                    .arg(renderLogicalRect.x())
                    .arg(renderLogicalRect.y())
                    .arg(renderLogicalRect.width())
                    .arg(renderLogicalRect.height()));
        }
    } else {
        // Keep any last valid source metadata around. A later frame may be able to
        // hold it again during an interactive drag, and clearing it here is what
        // made the blur drop out permanently on some render-target glitches.
        noteSourceMiss();
        if (BetterWobblyWindows::IsBlurTraceEnabled()) {
            BetterWobblyWindows::WriteBlurTraceLine(
                QStringLiteral("drawEmbeddedBlur ABORT no source window=%1 clippedEmpty=%2 centerEmpty=%3 "
                               "bg=%4,%5 %6x%7 render=%8,%9 %10x%11")
                    .arg(BetterWobblyWindows::GetTraceWindowId(w))
                    .arg(captureRect.isEmpty())
                    .arg(centerDest.isEmpty())
                    .arg(backgroundRect.x())
                    .arg(backgroundRect.y())
                    .arg(backgroundRect.width())
                    .arg(backgroundRect.height())
                    .arg(renderLogicalRect.x())
                    .arg(renderLogicalRect.y())
                    .arg(renderLogicalRect.width())
                    .arg(renderLogicalRect.height()));
        }
        return;
    }

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    const int quadCount = static_cast<int>(std::min(quads.count(), originalQuads.count()));
    const int meshVertexCount = quadCount * BetterWobblyWindows::QuadTriangleVertexCount;
    constexpr int offscreenVertexCount = BetterWobblyWindows::QuadTriangleVertexCount;

    // Reuse capacity across frames; allocating this mesh vector in every paint
    // showed up in the drag hot path for highly tessellated windows.
    std::vector<GLVertex2D> &vertices = _embeddedBlurVertices;
    const auto requiredVertexCount = static_cast<std::size_t>(offscreenVertexCount + meshVertexCount);
    if (vertices.capacity() > BetterWobblyWindows::MaximumRetainedBlurMeshVertices &&
        requiredVertexCount < BetterWobblyWindows::MaximumRetainedBlurMeshVertices / 2) {
        std::vector<GLVertex2D>().swap(vertices);
    }
    vertices.clear();
    vertices.reserve(requiredVertexCount);

    const auto bw = static_cast<float>(textureSize.width());
    const auto bh = static_cast<float>(textureSize.height());
    vertices.push_back(GLVertex2D{.position = QVector2D(0.0f, 0.0f), .texcoord = QVector2D(0.0f, 1.0f)});
    vertices.push_back(GLVertex2D{.position = QVector2D(bw, bh), .texcoord = QVector2D(1.0f, 0.0f)});
    vertices.push_back(GLVertex2D{.position = QVector2D(0.0f, bh), .texcoord = QVector2D(0.0f, 0.0f)});
    vertices.push_back(GLVertex2D{.position = QVector2D(0.0f, 0.0f), .texcoord = QVector2D(0.0f, 1.0f)});
    vertices.push_back(GLVertex2D{.position = QVector2D(bw, 0.0f), .texcoord = QVector2D(1.0f, 1.0f)});
    vertices.push_back(GLVertex2D{.position = QVector2D(bw, bh), .texcoord = QVector2D(1.0f, 0.0f)});

    const auto writeWindowVertex = [&](const WindowVertex &vertex, const WindowVertex &originalVertex) {
        const qreal logicalX = w->x() + data.xTranslation() + vertex.x() * data.xScale();
        const qreal logicalY = w->y() + data.yTranslation() + vertex.y() * data.yScale();
        const auto x = static_cast<float>((logicalX - sourceBackgroundRect.x()) * scale);
        const auto y = static_cast<float>((logicalY - sourceBackgroundRect.y()) * scale);

        const auto maskLocalX = static_cast<float>(originalVertex.x() * data.xScale() * scale);
        const auto maskLocalY = static_cast<float>(originalVertex.y() * data.yScale() * scale);
        vertices.push_back(GLVertex2D{.position = QVector2D(x, y), .texcoord = QVector2D(maskLocalX, maskLocalY)});
    };

    for (int qi = 0; qi < quadCount; ++qi) {
        const WindowQuad &quad = quads[qi];
        const WindowQuad &originalQuad = originalQuads[qi];
        writeWindowVertex(quad[0], originalQuad[0]);
        writeWindowVertex(quad[2], originalQuad[2]);
        writeWindowVertex(quad[3], originalQuad[3]);
        writeWindowVertex(quad[0], originalQuad[0]);
        writeWindowVertex(quad[1], originalQuad[1]);
        writeWindowVertex(quad[2], originalQuad[2]);
    }

    vbo->setData(vertices.data(), vertices.size() * sizeof(GLVertex2D));
    vbo->setVertexCount(static_cast<int>(vertices.size()));
    vbo->bindArrays();

    // Only the final passes target KWin's current render target. Keep the
    // offscreen Kawase pyramid unscissored, but clip every onscreen draw to the
    // exact device damage supplied by KWin. This mirrors DX's effectiveShape
    // clipping and prevents blur/noise from touching opaque window pixels that
    // OffscreenEffect will not repaint in the current partial-update frame.
    const Region onscreenClipRegion = clipOnscreenToDamage
        ? viewport.transform().map(deviceRegion, renderTarget.transformedSize())
        : Region::infinite();
    const auto drawOnscreenMesh = [&]() {
        if (clipOnscreenToDamage) {
            glEnable(GL_SCISSOR_TEST);
            vbo->draw(onscreenClipRegion, GL_TRIANGLES, offscreenVertexCount, meshVertexCount, true);
            glDisable(GL_SCISSOR_TEST);
        } else {
            vbo->draw(GL_TRIANGLES, offscreenVertexCount, meshVertexCount);
        }
    };

    // The dual-kawase levels are replacement renders, not compositing passes.
    // Do not inherit an enabled blend state from a previous KWin effect, otherwise
    // stale target contents are mixed into the kernel and change both color and
    // brightness. ScopedEmbeddedBlurGlState restores the incoming state afterwards.
    glDisable(GL_BLEND);
    if (_embeddedBlurClearIntermediateTargets) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    }

    {
        ShaderManager::instance()->pushShader(_embeddedDownsamplePass.shader.get());
        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(QRectF(0.0, 0.0, textureSize.width(), textureSize.height()));
        _embeddedDownsamplePass.shader->setUniform(_embeddedDownsamplePass.mvpMatrixLocation, projectionMatrix);
        _embeddedDownsamplePass.shader->setUniform(_embeddedDownsamplePass.offsetLocation, _embeddedBlurOffset);
        for (int i = 1; i <= _embeddedBlurIterationCount; ++i) {
            GLTexture *readTexture = renderTargets.textures[i - 1].get();
            const QVector2D halfpixel(0.5f / static_cast<float>(readTexture->width()),
                0.5f / static_cast<float>(readTexture->height()));
            _embeddedDownsamplePass.shader->setUniform(_embeddedDownsamplePass.halfpixelLocation, halfpixel);
            if (_embeddedDownsamplePass.sourceClampLocation >= 0) {
                _embeddedDownsamplePass.shader->setUniform(_embeddedDownsamplePass.sourceClampLocation,
                    clampForReadLevel(sourceClamp, i - 1));
            }

            readTexture->bind();
            GLFramebuffer::pushFramebuffer(renderTargets.framebuffers[i].get());
            if (_embeddedBlurClearIntermediateTargets) {
                glClear(GL_COLOR_BUFFER_BIT);
            }
            vbo->draw(GL_TRIANGLES, 0, offscreenVertexCount);
            GLFramebuffer::popFramebuffer();
        }
        ShaderManager::instance()->popShader();
    }

    {
        ShaderManager::instance()->pushShader(_embeddedUpsamplePass.shader.get());
        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(QRectF(0.0, 0.0, textureSize.width(), textureSize.height()));
        _embeddedUpsamplePass.shader->setUniform(_embeddedUpsamplePass.mvpMatrixLocation, projectionMatrix);
        _embeddedUpsamplePass.shader->setUniform(_embeddedUpsamplePass.offsetLocation, _embeddedBlurOffset);
        for (int i = _embeddedBlurIterationCount; i > 1; --i) {
            GLTexture *readTexture = renderTargets.textures[i].get();
            const QVector2D halfpixel(0.5f / static_cast<float>(readTexture->width()),
                0.5f / static_cast<float>(readTexture->height()));
            _embeddedUpsamplePass.shader->setUniform(_embeddedUpsamplePass.halfpixelLocation, halfpixel);
            if (_embeddedUpsamplePass.sourceClampLocation >= 0) {
                _embeddedUpsamplePass.shader->setUniform(_embeddedUpsamplePass.sourceClampLocation,
                    clampForReadLevel(sourceClamp, i));
            }

            readTexture->bind();
            GLFramebuffer::pushFramebuffer(renderTargets.framebuffers[i - 1].get());
            if (_embeddedBlurClearIntermediateTargets) {
                glClear(GL_COLOR_BUFFER_BIT);
            }
            vbo->draw(GL_TRIANGLES, 0, offscreenVertexCount);
            GLFramebuffer::popFramebuffer();
        }
        ShaderManager::instance()->popShader();
    }

    // Better Blur DX does not send the final 8-tap pass straight to the screen.
    // It resolves color/refraction/noise into a full-resolution cache first,
    // then samples that cache in a separate texture pass. Skipping this stage
    // changes filtering order and makes the fallback visibly sharper even with
    // identical strength values.
    {
        ShaderManager::instance()->pushShader(_embeddedOnscreenPass.shader.get());
        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(QRectF(0.0, 0.0, textureSize.width(), textureSize.height()));

        GLTexture *readTexture = renderTargets.textures[1].get();
        const QVector2D halfpixel(0.5f / static_cast<float>(readTexture->width()),
            0.5f / static_cast<float>(readTexture->height()));
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.mvpMatrixLocation, projectionMatrix);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.colorMatrixLocation, _embeddedBlurColorMatrix);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.halfpixelLocation, halfpixel);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.offsetLocation, _embeddedBlurOffset);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.textureSizeLocation,
            QVector2D(static_cast<float>(textureSize.width()), static_cast<float>(textureSize.height())));
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.sourceClampLocation, sourceClamp);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.edgeSoftnessLocation, 0.0f);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.sourceYFlipLocation, 1);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.refractionEnabledLocation,
            _embeddedRefractionEnabled ? 1 : 0);

        const QVector2D refractionRectSize(static_cast<float>(textureSize.width()),
            static_cast<float>(textureSize.height()));
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.refractionRectSizeLocation, refractionRectSize);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.refractionEdgeSizePixelsLocation,
            static_cast<float>(std::min(_embeddedRefractionEdgeSizePixels,
                static_cast<qreal>(std::min(refractionRectSize.x() / 2.0f, refractionRectSize.y() / 2.0f)))));
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.refractionCornerRadiusPixelsLocation,
            static_cast<float>(_embeddedRefractionCornerRadiusPixels));
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.refractionStrengthLocation,
            static_cast<float>(_embeddedRefractionStrength));
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.refractionNormalPowLocation,
            static_cast<float>(_embeddedRefractionNormalPow));
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.refractionRgbFringingLocation,
            static_cast<float>(_embeddedRefractionRgbFringing));
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.refractionTextureRepeatModeLocation,
            _embeddedRefractionTextureRepeatMode);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.refractionModeLocation, _embeddedRefractionMode);
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.boxLocation, QVector4D());
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.cornerRadiusLocation, QVector4D());
        _embeddedOnscreenPass.shader->setUniform(_embeddedOnscreenPass.opacityLocation, 1.0f);

        readTexture->bind();
        glDisable(GL_BLEND);
        GLFramebuffer::pushFramebuffer(renderTargets.compositeFramebuffer.get());
        vbo->draw(GL_TRIANGLES, 0, offscreenVertexCount);
        GLFramebuffer::popFramebuffer();
        ShaderManager::instance()->popShader();
    }

    if (GLTexture *noiseTexture = EnsureEmbeddedBlurNoiseTexture()) {
        ShaderManager::instance()->pushShader(_embeddedNoisePass.shader.get());
        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(QRectF(0.0, 0.0, textureSize.width(), textureSize.height()));
        _embeddedNoisePass.shader->setUniform(_embeddedNoisePass.mvpMatrixLocation, projectionMatrix);
        _embeddedNoisePass.shader->setUniform(_embeddedNoisePass.noiseTextureSizeLocation,
            QVector2D(static_cast<float>(noiseTexture->width()), static_cast<float>(noiseTexture->height())));
        _embeddedNoisePass.shader->setUniform(_embeddedNoisePass.blurTextureSizeLocation,
            QVector2D(static_cast<float>(textureSize.width()), static_cast<float>(textureSize.height())));
        _embeddedNoisePass.shader->setUniform(_embeddedNoisePass.renderScaleLocation, 1.0f);
        _embeddedNoisePass.shader->setUniform(_embeddedNoisePass.boxLocation, QVector4D());
        _embeddedNoisePass.shader->setUniform(_embeddedNoisePass.cornerRadiusLocation, QVector4D());
        _embeddedNoisePass.shader->setUniform(_embeddedNoisePass.opacityLocation, 1.0f);
        _embeddedNoisePass.shader->setUniform(_embeddedNoisePass.noiseModeLocation, _embeddedBlurNoiseMode);
        noiseTexture->bind();
        GLFramebuffer::pushFramebuffer(renderTargets.compositeFramebuffer.get());
        glEnable(GL_BLEND);
        glBlendFunc(_embeddedBlurNoiseMode == 0 ? GL_ONE : GL_SRC_ALPHA,
            _embeddedBlurNoiseMode == 0 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
        vbo->draw(GL_TRIANGLES, 0, offscreenVertexCount);
        glDisable(GL_BLEND);
        GLFramebuffer::popFramebuffer();
        ShaderManager::instance()->popShader();
    }

    {
        ShaderManager::instance()->pushShader(_embeddedCompositePass.shader.get());
        QMatrix4x4 projectionMatrix = viewport.projectionMatrix();
        projectionMatrix.translate(static_cast<float>(sourceBackgroundRect.x()) * static_cast<float>(scale),
            static_cast<float>(sourceBackgroundRect.y()) * static_cast<float>(scale));
        _embeddedCompositePass.shader->setUniform(_embeddedCompositePass.mvpMatrixLocation, projectionMatrix);
        _embeddedCompositePass.shader->setUniform(_embeddedCompositePass.textureSizeLocation,
            QVector2D(static_cast<float>(textureSize.width() * scale),
                static_cast<float>(textureSize.height() * scale)));
        _embeddedCompositePass.shader->setUniform(_embeddedCompositePass.sourceClampLocation, sourceClamp);
        const float edgeSoftness = static_cast<float>(_embeddedBlurEdgeSoftnessPixels) /
            static_cast<float>(std::max(1, std::min(textureSize.width(), textureSize.height())));
        _embeddedCompositePass.shader->setUniform(_embeddedCompositePass.edgeSoftnessLocation, edgeSoftness);
        _embeddedCompositePass.shader->setUniform(_embeddedCompositePass.sourceYFlipLocation,
            _embeddedBlurSourceYFlip ? 1 : 0);
        _embeddedCompositePass.shader->setUniform(_embeddedCompositePass.boxLocation, roundedBox);
        _embeddedCompositePass.shader->setUniform(_embeddedCompositePass.cornerRadiusLocation, roundedCornerRadius);
        _embeddedCompositePass.shader->setUniform(_embeddedCompositePass.opacityLocation, embeddedOpacity);

        renderTargets.compositeTexture->bind();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        drawOnscreenMesh();
        glDisable(GL_BLEND);
        ShaderManager::instance()->popShader();
    }

    vbo->unbindArrays();
    if (BetterWobblyWindows::IsBlurTraceEnabled()) {
        BetterWobblyWindows::WriteBlurTraceLine(
            QStringLiteral("drawEmbeddedBlur DONE window=%1 held=%2 clipped=%3 transition=%4 internalFormat=0x%5")
                .arg(BetterWobblyWindows::GetTraceWindowId(w))
                .arg(usingHeldSource)
                .arg(renderTargets.lastSourceWasClipped)
                .arg(transitionOpacity)
                .arg(QString::number(textureFormat, 16)));
        qCWarning(KWIN_WOBBLYWINDOWS) << "drawEmbeddedBlur done" << "window" << w << "heldSource" << usingHeldSource
                                      << "sourceClipped" << renderTargets.lastSourceWasClipped << "internalFormat"
                                      << QStringLiteral("0x%1").arg(QString::number(textureFormat, 16));
    }
}

} // namespace KWin
