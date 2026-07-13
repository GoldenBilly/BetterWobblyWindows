/*
    KWin - the KDE window manager
    Portions of this file are adapted from the KDE KWin Wobbly Windows effect.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later

    Official KWin Wobbly Windows source:
    https://github.com/KDE/kwin/tree/master/src/plugins/wobblywindows
    Upstream authors are credited above according to the corresponding KDE files.
*/
#include "WobblyWindowsEffect_p.h"

#include <cmath>

namespace KWin
{

WindowQuadList WobblyWindowsEffect::MakeWindowQuads(EffectWindow *w) const
{
    const RectF visibleRect(0.0, 0.0, w->width(), w->height());
    WindowQuad quad;
    quad[0] = WindowVertex(visibleRect.topLeft(), QPointF(0, 0));
    quad[1] = WindowVertex(visibleRect.topRight(), QPointF(1, 0));
    quad[2] = WindowVertex(visibleRect.bottomRight(), QPointF(1, 1));
    quad[3] = WindowVertex(visibleRect.bottomLeft(), QPointF(0, 1));

    WindowQuadList quads;
    quads.append(quad);
    return quads;
}

bool WobblyWindowsEffect::ShouldPaintEmbeddedBlur(EffectWindow *w) const
{
    const auto it = _windows.constFind(w);
    if (it == _windows.constEnd()) {
        return false;
    }

    // Keep rendering while the transition is fading out after the spring has
    // settled. During fade-in, Moving forces an initial capture even when the
    // current opacity is still close to zero.
    return it->status == Moving || it->wobblying || it->embeddedBlurTransitionProgress > 0.0;
}

bool WobblyWindowsEffect::DeformWindowQuads(EffectWindow *w,
    WindowPaintData &data,
    WindowQuadList &quads,
    bool updateRepaintRegion,
    bool forceGrid,
    bool quadsAlreadyTessellated)
{
    BetterWobblyWindows::WriteHardTraceLine("DEFORM HIT");
    auto infoIt = _windows.find(w);
    if (infoIt == _windows.end()) {
        return false;
    }

    WindowWobblyInfos &wwi = infoIt.value();
    if (!wwi.wobblying && !forceGrid) {
        return false;
    }

    const RectF frameGeometry = w->frameGeometry();
    const qreal tx = frameGeometry.x();
    const qreal ty = frameGeometry.y();
    const qreal width = std::max<qreal>(1.0, frameGeometry.width());
    const qreal height = std::max<qreal>(1.0, frameGeometry.height());
    qreal left = 0.0;
    qreal top = 0.0;
    qreal right = w->width();
    qreal bottom = w->height();

    if (!quadsAlreadyTessellated) {
        quads = quads.makeRegularGrid(_xTessellation, _yTessellation);
    }
    for (int i = 0; i < quads.count(); ++i) {
        for (int j = 0; j < 4; ++j) {
            WindowVertex &v = quads[i][j];
            Pair uv = {v.x() / width, v.y() / height};
            Pair newPos = ComputeBezierPoint(wwi, uv);

            // Apply the elastic counter-resize as an affine edge gradient.
            // The actively resized edge stays at the real frameGeometry(), while
            // the complete opposite edge receives the full spring displacement.
            // This makes the window look as though it briefly resizes from both
            // sides, instead of bending one tiny patch around a single vertex.
            const qreal oppositeXInfluence =
                wwi.visualOppositePointNormalized.x < 0.5 ? static_cast<qreal>(1.0) - uv.x : uv.x;
            const qreal oppositeYInfluence =
                wwi.visualOppositePointNormalized.y < 0.5 ? static_cast<qreal>(1.0) - uv.y : uv.y;
            newPos.x += wwi.visualOppositePointFollowOffset.x * oppositeXInfluence;
            newPos.y += wwi.visualOppositePointFollowOffset.y * oppositeYInfluence;

            v.move(newPos.x - tx, newPos.y - ty);
        }
        left = std::min(left, quads[i].left());
        top = std::min(top, quads[i].top());
        right = std::max(right, quads[i].right());
        bottom = std::max(bottom, quads[i].bottom());
    }

    if (updateRepaintRegion) {
        RectF dirtyRect(left * data.xScale() + w->x() + data.xTranslation(),
            top * data.yScale() + w->y() + data.yTranslation(),
            (right - left + 1.0) * data.xScale(),
            (bottom - top + 1.0) * data.yScale());

        // Repaint the whole embedded-blur source footprint, not only the
        // visible deformed mesh. Better Blur DX asks KWin to expand repaints
        // below opaque regions by the blur radius; embedded blur has to do the
        // equivalent itself. Without this, moving a window downward can sample
        // a background area that KWin did not redraw for the current drag frame,
        // which looks like the blur disappearing or turning into stale blocks.
        qreal repaintPadding = 1.0;
        if (_embeddedBlurEnabled) {
            const qreal windowLogicalWidth = static_cast<qreal>(width) * data.xScale();
            const qreal windowLogicalHeight = static_cast<qreal>(height) * data.yScale();
            const qreal blurKernelPadding = std::max<qreal>(_embeddedBlurExpandSize * 2.0,
                (_embeddedBlurOffset + BetterWobblyWindows::BlurKernelOffsetBias) *
                    BetterWobblyWindows::BlurKernelScaleMultiplier);
            const qreal wobblePadding = std::clamp(std::max(windowLogicalWidth, windowLogicalHeight) *
                    BetterWobblyWindows::WobblePaddingWindowRatio,
                static_cast<qreal>(BetterWobblyWindows::MinimumWobblePaddingPixels),
                static_cast<qreal>(BetterWobblyWindows::MaximumWobblePaddingPixels));
            repaintPadding = std::max<qreal>(BetterWobblyWindows::MinimumTextureDimension,
                blurKernelPadding + wobblePadding + BetterWobblyWindows::BlurKernelOffsetBias);
        }
        dirtyRect.adjust(-repaintPadding, -repaintPadding, repaintPadding, repaintPadding);
        _updateRegion = _updateRegion.united(dirtyRect.toAlignedRect());
    }

    return true;
}

} // namespace KWin
