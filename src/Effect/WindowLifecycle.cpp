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
namespace
{
const std::chrono::milliseconds IntegrationStep(BetterWobblyWindows::IntegrationStepMilliseconds);
} // namespace

#if KWIN_VERSION < KWIN_VERSION_CODE(6, 7, 80)
void WobblyWindowsEffect::drawWindow(const RenderTarget &renderTarget,
    const RenderViewport &viewport,
    EffectWindow *w,
    int mask,
    const Region &deviceRegion,
    WindowPaintData &data)
#else
bool WobblyWindowsEffect::drawWindow(const RenderTarget &renderTarget,
    const RenderViewport &viewport,
    EffectWindow *w,
    int mask,
    const Region &deviceRegion,
    WindowPaintData &data)
#endif
{
    if (BetterWobblyWindows::IsBlurTraceEnabled() && _windows.contains(w)) {
        BetterWobblyWindows::WriteBlurTraceLine(
            QStringLiteral("drawWindow tracked window=%1 mask=%2 isUserMove=%3 isUserResize=%4 suppressionRole=%5")
                .arg(BetterWobblyWindows::GetTraceWindowId(w))
                .arg(mask)
                .arg(w->isUserMove())
                .arg(w->isUserResize())
                .arg(w->data(BetterBlurDxApi::RequestRole).toString()));
        qCWarning(KWIN_WOBBLYWINDOWS) << "drawWindow for tracked window"
                                      << "window" << w << "mask" << mask << "isUserMove" << w->isUserMove()
                                      << "isUserResize" << w->isUserResize() << "suppressionRole"
                                      << w->data(BetterBlurDxApi::RequestRole);
    }
    // Better Blur DX API V1 owns both the blur calculation and the final mesh
    // composite. The embedded implementation is a strict fallback and is never
    // executed in a frame acknowledged by the provider.
    const QVariant providerResultValue = w->data(BetterBlurDxApi::ResultRole);
    const bool providerRendered = BetterBlurDxApi::ProviderRendered(providerResultValue);
    const bool providerDeclinedByPolicy = BetterBlurDxApi::ProviderDeclinedByPolicy(providerResultValue);
    if (providerRendered || providerDeclinedByPolicy) {
        // A rendered frame is owned by Better Blur DX. DeclinedByPolicy is also
        // final: Better Blur DX intentionally excluded this window (for example
        // through Blur only matching / Blur all except matching), so applying the
        // embedded fallback would violate the provider's configuration.
        _embeddedBlurRenderData.erase(w);
        if (!_embeddedBlurReusableRenderData.textures.empty() || _embeddedBlurReusableRenderData.compositeTexture) {
            ReleaseEmbeddedBlurRenderTargets(_embeddedBlurReusableRenderData);
        }
    } else if (_embeddedBlurFallbackEnabled) {
        // None/Declined mean unavailable or technical failure; only these states
        // are allowed to activate the strict standalone fallback.
        DrawEmbeddedBlur(renderTarget, viewport, w, mask, deviceRegion, data);
    }
#if KWIN_VERSION < KWIN_VERSION_CODE(6, 7, 80)
    OffscreenEffect::drawWindow(renderTarget, viewport, w, mask, deviceRegion, data);
#else
    return OffscreenEffect::drawWindow(renderTarget, viewport, w, mask, deviceRegion, data);
#endif
}

void WobblyWindowsEffect::SetExternalBlurSuppressed(EffectWindow *w, bool suppressed)
{
    if (!w) {
        return;
    }

    // Request provider-owned rendering first. The paired Better Blur DX build
    // receives only the Wobbly mesh; it keeps ownership of its cache, shaders,
    // color/noise/refraction passes and the final composite. Suppression is
    // always requested while the hand-off is active so no rectangular and
    // deformed blur paths can run concurrently.
    if (suppressed) {
        w->setData(BetterBlurDxApi::ResultRole, QVariant());
        w->setData(BetterBlurDxApi::RequestRole, BetterBlurDxApi::MakeRequest(&_betterBlurDxProvider, true, true));
    } else {
        w->setData(BetterBlurDxApi::RequestRole, QVariant());
        w->setData(BetterBlurDxApi::ResultRole, QVariant());
    }
}

void WobblyWindowsEffect::CleanupWindow(EffectWindow *w)
{
    SetExternalBlurSuppressed(w, false);
    if (_windows.contains(w)) {
        unredirect(w);
        _windows.remove(w);
    }
    _embeddedBlurRenderData.erase(w);
    if (_embeddedBlurReusableRenderData.lastSourceWindow == w) {
        _embeddedBlurReusableRenderData.hasValidSource = false;
        _embeddedBlurReusableRenderData.lastSourceWasClipped = false;
        _embeddedBlurReusableRenderData.lastRenderRect = QRect();
        _embeddedBlurReusableRenderData.lastRenderScale = 0.0;
        _embeddedBlurReusableRenderData.fallbackFramesRemaining = 0;
        _embeddedBlurReusableRenderData.lastSourceWindow = nullptr;
    }

    if (!_embeddedBlurReusableRenderData.lastSourceWindow &&
        EmbeddedBlurRenderTargetBytes(_embeddedBlurReusableRenderData) >
            BetterWobblyWindows::MaximumRetainedEmbeddedBlurBytes) {
        ReleaseEmbeddedBlurRenderTargets(_embeddedBlurReusableRenderData);
    }

    if (_windows.isEmpty()) {
        _embeddedBlurVertices.clear();
        if (_embeddedBlurVertices.capacity() > BetterWobblyWindows::MaximumRetainedBlurMeshVertices) {
            std::vector<GLVertex2D>().swap(_embeddedBlurVertices);
        }
    }
    effects->addRepaintFull();
}

void WobblyWindowsEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    auto infoIt = _windows.find(w);
    if (infoIt != _windows.end()) {
        WindowWobblyInfos &wwi = infoIt.value();
        const bool interactiveMoveResize = w->isUserMove() || w->isUserResize();
        if (wwi.status == Moving) {
            if (interactiveMoveResize) {
                wwi.seenInteractiveMoveResize = true;
                wwi.missedInteractiveMoveResizeFrames = 0;
            } else if (wwi.seenInteractiveMoveResize) {
                ++wwi.missedInteractiveMoveResizeFrames;

                // A screen lock, compositor grab cancellation, or some edge-snap
                // paths can interrupt an interactive move/resize without the
                // normal windowFinishUserMovedResized signal. If we keep the
                // window in Moving forever, the embedded blur capture rect and
                // stale FBO contents can be reused after unlock and produce stale
                // rectangular artifacts. After a couple of consecutive frames
                // with no interactive operation, drop the redirected state hard.
                if (wwi.missedInteractiveMoveResizeFrames >= 2) {
                    if (BetterWobblyWindows::IsBlurTraceEnabled()) {
                        BetterWobblyWindows::WriteBlurTraceLine(
                            QStringLiteral("prePaint cancel lost move/resize window=%1")
                                .arg(BetterWobblyWindows::GetTraceWindowId(w)));
                    }
                    SetExternalBlurSuppressed(w, false);
                    _embeddedBlurRenderData.erase(w);
                    if (_embeddedBlurReusableRenderData.lastSourceWindow == w) {
                        _embeddedBlurReusableRenderData.hasValidSource = false;
                        _embeddedBlurReusableRenderData.lastSourceWasClipped = false;
                        _embeddedBlurReusableRenderData.lastRenderRect = QRect();
                        _embeddedBlurReusableRenderData.lastRenderScale = 0.0;
                        _embeddedBlurReusableRenderData.fallbackFramesRemaining = 0;
                        _embeddedBlurReusableRenderData.lastSourceWindow = nullptr;
                    }
                    _windows.remove(w);
                    unredirect(w);
                    effects->addRepaintFull();
                    effects->prePaintWindow(view, w, data);
                    return;
                }
            }
        }

        std::chrono::milliseconds delta = wwi.clock.tick(view);

        // Avoid a physics catch-up spiral after a compositor/GPU stall. Running a
        // a dozen queued 10 ms spring steps in one pre-paint makes the next frame late
        // too, which looks like drag lag and shows up as sawtooth GPU power in
        // nvidia-smi. Dropping queued time is visually better for an interactive
        // effect than trying to replay it all at once.
        const std::chrono::milliseconds maxPhysicsCatchUp(BetterWobblyWindows::MaxPhysicsCatchUpMilliseconds);
        delta = std::min(delta, maxPhysicsCatchUp);
        int physicsSteps = 0;
        while (delta.count() > 0 && physicsSteps < _maxPhysicsStepsPerFrame) {
            const auto dt = std::min(delta, IntegrationStep);
            delta -= dt;
            ++physicsSteps;

            if (!UpdateWindowWobblyDatas(w, static_cast<double>(dt.count()))) {
                break;
            }
        }

        if (_windows.contains(w)) {
            auto activeInfoIt = _windows.find(w);
            WindowWobblyInfos &activeInfo = activeInfoIt.value();
            const bool physicsStateActive = activeInfo.status == Moving || activeInfo.wobblying;
            // Provider rendering does not depend on the standalone fallback
            // shaders being valid. Keep the visual hand-off active whenever blur
            // is enabled; embedded validity matters only after provider decline.
            const bool blurStateActive = _embeddedBlurEnabled && physicsStateActive;
            const qreal transitionTarget = blurStateActive ? 1.0 : 0.0;
            const qreal previousTransition = activeInfo.embeddedBlurTransitionProgress;
            activeInfo.embeddedBlurTransitionProgress =
                BetterWobblyWindows::AdvanceBlurTransition(activeInfo.embeddedBlurTransitionProgress,
                    transitionTarget,
                    _embeddedBlurHandoffFrames);

            const bool transitionChanged =
                !qFuzzyCompare(previousTransition + 1.0, activeInfo.embeddedBlurTransitionProgress + 1.0);
            const bool transitionFinishedOut = !physicsStateActive && activeInfo.embeddedBlurTransitionProgress <= 0.0;
            if (transitionChanged) {
                // The spring can already be stationary during fade-out, so its mesh
                // no longer schedules damage. Keep frames flowing until the visual
                // hand-off reaches the target state.
                effects->addRepaintFull();
            }
            if (transitionFinishedOut) {
                CleanupWindow(w);
                effects->prePaintWindow(view, w, data);
                return;
            }

            // Keep the transformed paint flag only while the spring mesh is
            // actually moving. Once the physics has settled, the redirected
            // offscreen window is already geometrically identical to the normal
            // window, so leaving PAINT_WINDOW_TRANSFORMED set only prevents the
            // regular blur effect from drawing underneath our fade-out.
            //
            // Releasing the flag at the beginning of fade-out lets the normal
            // blur reappear below the still-opaque embedded layer. The embedded
            // layer can then fade over it without exposing the transparent
            // application surface for a frame.
            if (physicsStateActive) {
                data.setTransformed();
            }

            // The embedded blur samples the already-rendered background behind
            // the wobbly window. If KWin treats this window as opaque, it may
            // cull exactly the pixels we need to blit. Keep the translucent path
            // enabled for the whole transition, including fade-out and source
            // recovery frames.
            const bool embeddedBlurActive = _embeddedBlurEnabled && ShouldPaintEmbeddedBlur(w);
            const bool hasReusableEmbeddedSource = _embeddedBlurReusableRenderData.hasValidSource &&
                _embeddedBlurReusableRenderData.lastSourceWindow == w &&
                !_embeddedBlurReusableRenderData.size.isEmpty();
            if (embeddedBlurActive) {
                data.setTranslucent();
            }

            SetExternalBlurSuppressed(w, embeddedBlurActive);
            if (BetterWobblyWindows::IsBlurTraceEnabled()) {
                BetterWobblyWindows::WriteBlurTraceLine(
                    QStringLiteral("prePaint tracked window=%1 wobblying=%2 status=%3 transition=%4 embeddedEnabled=%5 "
                                   "embeddedValid=%6 reusableSource=%7 sourceMissFrames=%8")
                        .arg(BetterWobblyWindows::GetTraceWindowId(w))
                        .arg(activeInfo.wobblying)
                        .arg(static_cast<int>(activeInfo.status))
                        .arg(activeInfo.embeddedBlurTransitionProgress)
                        .arg(_embeddedBlurEnabled)
                        .arg(_embeddedBlurValid)
                        .arg(hasReusableEmbeddedSource && !_embeddedBlurReusableRenderData.lastSourceWasClipped)
                        .arg(activeInfo.embeddedBlurSourceMissFrames));
                qCWarning(KWIN_WOBBLYWINDOWS)
                    << "prePaint tracked window"
                    << "window" << w << "wobblying" << activeInfo.wobblying << "status"
                    << static_cast<int>(activeInfo.status) << "transition" << activeInfo.embeddedBlurTransitionProgress
                    << "embeddedEnabled" << _embeddedBlurEnabled << "embeddedValid" << _embeddedBlurValid;
            }
        }
    }

    effects->prePaintWindow(view, w, data);
}

void WobblyWindowsEffect::apply(EffectWindow *w, int mask, WindowPaintData &data, WindowQuadList &quads)
{
    BetterWobblyWindows::WriteHardTraceLine("APPLY HARD HIT");
    Q_UNUSED(mask)
    // This is the exact path that deforms the actual window. If a window is
    // visibly wobbling and this line does not appear in the trace file, then
    // the visible wobble is coming from another installed/enabled effect, not
    // from this better_wobblywindows plugin.
    if (BetterWobblyWindows::IsBlurTraceEnabled()) {
        const auto infoIt = _windows.constFind(w);
        const bool tracked = infoIt != _windows.constEnd();
        BetterWobblyWindows::WriteBlurTraceLine(
            QStringLiteral("APPLY HIT window=%1 tracked=%2 wobblying=%3 status=%4 forceBlur=%5 sharedSuppress=%6")
                .arg(BetterWobblyWindows::GetTraceWindowId(w))
                .arg(tracked)
                .arg(tracked ? infoIt->wobblying : false)
                .arg(tracked ? static_cast<int>(infoIt->status) : -1)
                .arg(w->data(WindowForceBlurRole).toString(), w->data(BetterBlurDxApi::RequestRole).toString()));
    }
    DeformWindowQuads(w, data, quads, true);
}

void WobblyWindowsEffect::postPaintScreen()
{
    if (!_updateRegion.isEmpty()) {
        effects->addRepaint(_updateRegion);
        _updateRegion = Region();
    }

    effects->postPaintScreen();
}

void WobblyWindowsEffect::SlotWindowAdded(EffectWindow *w)
{
    connect(w, &EffectWindow::windowStartUserMovedResized, this, &WobblyWindowsEffect::SlotWindowStartUserMovedResized);
    connect(w, &EffectWindow::windowStepUserMovedResized, this, &WobblyWindowsEffect::SlotWindowStepUserMovedResized);
    connect(w,
        &EffectWindow::windowFinishUserMovedResized,
        this,
        &WobblyWindowsEffect::SlotWindowFinishUserMovedResized);
    connect(w, &EffectWindow::windowMaximizedStateChanged, this, &WobblyWindowsEffect::SlotWindowMaximizeStateChanged);
}

void WobblyWindowsEffect::SlotWindowStartUserMovedResized(EffectWindow *w)
{
    if (w->isSpecialWindow()) {
        return;
    }

    const bool moving = w->isUserMove();
    const bool resizing = w->isUserResize();

    // Some KWin/patch combinations emit the start signal before isUserMove()
    // or isUserResize() flips. The signal itself is already specific to an
    // interactive move/resize, so fall back to the enabled settings instead of
    // missing the whole blur hand-off.
    if ((moving && _moveWobble) || (resizing && _resizeWobble) ||
        (!moving && !resizing && (_moveWobble || _resizeWobble))) {
        StartMovedResized(w);
    }
}

void WobblyWindowsEffect::SlotWindowStepUserMovedResized(EffectWindow *w, const RectF &geometry)
{
    if (!_windows.contains(w) && !w->isSpecialWindow()) {
        if ((w->isUserMove() && _moveWobble) || (w->isUserResize() && _resizeWobble)) {
            StartMovedResized(w);
        }
    }

    auto infoIt = _windows.find(w);
    if (infoIt != _windows.end()) {
        WindowWobblyInfos &wwi = infoIt.value();
        wwi.seenInteractiveMoveResize = true;
        wwi.missedInteractiveMoveResizeFrames = 0;
        const RectF rect = geometry;

        if (_visualOppositePointFollowOnResize && _resizeWobble && wwi.visualOppositePointAnchorValid) {
            if (!wwi.visualOppositePointLastRectValid) {
                wwi.visualOppositePointLastRect = rect;
                wwi.visualOppositePointLastRectValid = true;
            } else {
                const RectF previous = wwi.visualOppositePointLastRect;
                const qreal leftDelta = rect.left() - previous.left();
                const qreal rightDelta = rect.right() - previous.right();
                const qreal topDelta = rect.top() - previous.top();
                const qreal bottomDelta = rect.bottom() - previous.bottom();
                const qreal epsilon = BetterWobblyWindows::OppositePointFollowGeometryEpsilon;

                qreal activeDeltaX = 0.0;
                const bool translatedOnlyX = std::abs(leftDelta) > epsilon && std::abs(rightDelta) > epsilon &&
                    std::abs(leftDelta - rightDelta) <= epsilon;
                if (!translatedOnlyX && (std::abs(leftDelta) > epsilon || std::abs(rightDelta) > epsilon)) {
                    if (std::abs(leftDelta) >= std::abs(rightDelta)) {
                        activeDeltaX = leftDelta;
                        wwi.visualOppositePointNormalized.x = 1.0;
                    } else {
                        activeDeltaX = rightDelta;
                        wwi.visualOppositePointNormalized.x = 0.0;
                    }
                }

                qreal activeDeltaY = 0.0;
                const bool translatedOnlyY = std::abs(topDelta) > epsilon && std::abs(bottomDelta) > epsilon &&
                    std::abs(topDelta - bottomDelta) <= epsilon;
                if (!translatedOnlyY && (std::abs(topDelta) > epsilon || std::abs(bottomDelta) > epsilon)) {
                    if (std::abs(topDelta) >= std::abs(bottomDelta)) {
                        activeDeltaY = topDelta;
                        wwi.visualOppositePointNormalized.y = 1.0;
                    } else {
                        activeDeltaY = bottomDelta;
                        wwi.visualOppositePointNormalized.y = 0.0;
                    }
                }

                // Expansion moves the opposite edge outward; contraction moves
                // it inward. The minus sign mirrors the active edge displacement.
                const qreal impulseScale =
                    _visualOppositePointFollowStrength * BetterWobblyWindows::OppositePointFollowImpulseScale;
                wwi.visualOppositePointPendingImpulse.x += -activeDeltaX * impulseScale;
                wwi.visualOppositePointPendingImpulse.y += -activeDeltaY * impulseScale;

                const qreal pendingLength =
                    std::hypot(wwi.visualOppositePointPendingImpulse.x, wwi.visualOppositePointPendingImpulse.y);
                if (pendingLength > BetterWobblyWindows::OppositePointFollowMaximumPixels) {
                    const qreal scale = BetterWobblyWindows::OppositePointFollowMaximumPixels / pendingLength;
                    wwi.visualOppositePointPendingImpulse.x *= scale;
                    wwi.visualOppositePointPendingImpulse.y *= scale;
                }
            }
            wwi.visualOppositePointLastRect = rect;
        }

        if (rect.y() != wwi.resizeOriginalRect.y()) {
            wwi.canWobbleTop = true;
        }
        if (rect.x() != wwi.resizeOriginalRect.x()) {
            wwi.canWobbleLeft = true;
        }
        if (rect.right() != wwi.resizeOriginalRect.right()) {
            wwi.canWobbleRight = true;
        }
        if (rect.bottom() != wwi.resizeOriginalRect.bottom()) {
            wwi.canWobbleBottom = true;
        }
        // Match KWin's Wobbly Windows path while the mesh is moving. Rounding
        // here makes a fractionally positioned window jump when redirection
        // starts, before the spring has produced any visible deformation.
        setVertexSnappingMode(RenderGeometry::VertexSnappingMode::None);
    }
}

void WobblyWindowsEffect::SlotWindowFinishUserMovedResized(EffectWindow *w)
{
    auto infoIt = _windows.find(w);
    if (infoIt != _windows.end()) {
        // Keep the paired Better Blur DX pass disabled while the embedded cache
        // fades out; otherwise both layers overlap during the hand-off.
        SetExternalBlurSuppressed(w, true);
        effects->addRepaintFull();
        WindowWobblyInfos &wwi = infoIt.value();
        wwi.status = Free;
        const RectF rect = w->frameGeometry();
        wwi.visualOppositePointLastRect = rect;
        wwi.visualOppositePointLastRectValid = true;
        if (rect.y() != wwi.resizeOriginalRect.y()) {
            wwi.canWobbleTop = true;
        }
        if (rect.x() != wwi.resizeOriginalRect.x()) {
            wwi.canWobbleLeft = true;
        }
        if (rect.right() != wwi.resizeOriginalRect.right()) {
            wwi.canWobbleRight = true;
        }
        if (rect.bottom() != wwi.resizeOriginalRect.bottom()) {
            wwi.canWobbleBottom = true;
        }
    }
}

void WobblyWindowsEffect::SlotWindowMaximizeStateChanged(EffectWindow *w, bool horizontal, bool vertical)
{
    Q_UNUSED(horizontal)
    Q_UNUSED(vertical)

    if (w->isUserMove() || w->isSpecialWindow()) {
        return;
    }

    if (_moveWobble && _resizeWobble) {
        StepMovedResized(w);
    }

    auto infoIt = _windows.find(w);
    if (infoIt != _windows.end()) {
        WindowWobblyInfos &wwi = infoIt.value();
        const RectF rect = w->frameGeometry();
        if (rect.y() != wwi.resizeOriginalRect.y()) {
            wwi.canWobbleTop = true;
        }
        if (rect.x() != wwi.resizeOriginalRect.x()) {
            wwi.canWobbleLeft = true;
        }
        if (rect.right() != wwi.resizeOriginalRect.right()) {
            wwi.canWobbleRight = true;
        }
        if (rect.bottom() != wwi.resizeOriginalRect.bottom()) {
            wwi.canWobbleBottom = true;
        }
    }
}

void WobblyWindowsEffect::StartMovedResized(EffectWindow *w)
{
    BetterWobblyWindows::WriteHardTraceLine("START MOVED RESIZED");
    if (BetterWobblyWindows::IsBlurTraceEnabled()) {
        BetterWobblyWindows::WriteBlurTraceLine(
            QStringLiteral("startMovedResized window=%1 isUserMove=%2 isUserResize=%3 frame=%4,%5 %6x%7")
                .arg(BetterWobblyWindows::GetTraceWindowId(w))
                .arg(w->isUserMove())
                .arg(w->isUserResize())
                .arg(w->frameGeometry().x())
                .arg(w->frameGeometry().y())
                .arg(w->frameGeometry().width())
                .arg(w->frameGeometry().height()));
        qCWarning(KWIN_WOBBLYWINDOWS) << "startMovedResized"
                                      << "window" << w << "isUserMove" << w->isUserMove() << "isUserResize"
                                      << w->isUserResize() << "frame" << w->frameGeometry();
    }

    // Do not snap the redirected mesh on the first drag frame. Fractional
    // positions must be preserved or the whole window visibly shifts once.
    setVertexSnappingMode(RenderGeometry::VertexSnappingMode::None);

    if (_embeddedBlurReusableRenderData.lastSourceWindow != w) {
        _embeddedBlurReusableRenderData.hasValidSource = false;
        _embeddedBlurReusableRenderData.lastSourceWasClipped = false;
        _embeddedBlurReusableRenderData.lastRenderRect = QRect();
        _embeddedBlurReusableRenderData.lastRenderScale = 0.0;
        _embeddedBlurReusableRenderData.fallbackFramesRemaining = 0;
        _embeddedBlurReusableRenderData.lastSourceWindow = nullptr;
    }

    // The embedded pyramid is a strict fallback. Do not allocate or prewarm it
    // while the paired API V1 provider is loaded; this avoids keeping two complete
    // blur pipelines resident for the same moving window.
    const bool providerEffectLoaded =
        effects && effects->loadedEffects().contains(QStringLiteral("better_blur_dx_wobbly_api"));
    if (_embeddedBlurFallbackEnabled && !providerEffectLoaded) {
        PrewarmEmbeddedBlurRenderTargets(w);
    }

    SetExternalBlurSuppressed(w, true);
    effects->addRepaintFull();

    auto infoIt = _windows.find(w);
    if (infoIt == _windows.end()) {
        WindowWobblyInfos newWobblyInfo;
        InitWobblyInfo(newWobblyInfo, w->frameGeometry());
        infoIt = _windows.insert(w, std::move(newWobblyInfo));
        redirect(w);
    }

    WindowWobblyInfos &wwi = infoIt.value();
    wwi.status = Moving;
    wwi.seenInteractiveMoveResize = true;
    wwi.missedInteractiveMoveResizeFrames = 0;
    wwi.embeddedBlurSourceMissFrames = 0;
    std::fill(wwi.constraint.begin(), wwi.constraint.end(), false);
    const RectF &rect = w->frameGeometry();

    const qreal safeWidth = std::max<qreal>(1.0, rect.width());
    const qreal safeHeight = std::max<qreal>(1.0, rect.height());

    const Pair picked = {static_cast<qreal>(cursorPos().x()), static_cast<qreal>(cursorPos().y())};
    const qreal normalizedX = std::clamp((picked.x - rect.x()) / safeWidth, 0.0, 1.0);
    const qreal normalizedY = std::clamp((picked.y - rect.y()) / safeHeight, 0.0, 1.0);
    const int indexX = std::clamp(static_cast<int>(std::lround(normalizedX * static_cast<qreal>(wwi.width - 1))),
        0,
        static_cast<int>(wwi.width - 1));
    const int indexY = std::clamp(static_cast<int>(std::lround(normalizedY * static_cast<qreal>(wwi.height - 1))),
        0,
        static_cast<int>(wwi.height - 1));
    const int pickedPointIndex = indexY * static_cast<int>(wwi.width) + indexX;
#if defined VERBOSE_MODE
    qCDebug(KWIN_WOBBLYWINDOWS) << "Original Picked point -- x : " << picked.x << " - y : " << picked.y;
#endif
    wwi.constraint[pickedPointIndex] = true;

    // The elastic counter-resize starts from the point mirrored through the
    // window center. Actual edge deltas will refine the exact opposite edge on
    // every resize step; only the rendered mesh is affected.
    wwi.visualOppositePointNormalized = {1.0 - normalizedX, 1.0 - normalizedY};
    wwi.visualOppositePointFollowOffset = {0.0, 0.0};
    wwi.visualOppositePointFollowVelocity = {0.0, 0.0};
    wwi.visualOppositePointPendingImpulse = {0.0, 0.0};
    wwi.visualOppositePointLastRect = rect;
    wwi.visualOppositePointLastRectValid = true;
    wwi.visualOppositePointAnchorValid = true;

    // Capture the geometry at the start signal even on KWin builds where
    // isUserResize() becomes true one signal later.
    wwi.resizeOriginalRect = rect;
    if (w->isUserResize()) {
        // Keep each resize edge fixed until the user moves that edge.
        wwi.canWobbleTop = wwi.canWobbleLeft = wwi.canWobbleRight = wwi.canWobbleBottom = false;
    } else {
        wwi.canWobbleTop = wwi.canWobbleLeft = wwi.canWobbleRight = wwi.canWobbleBottom = true;
    }
}

void WobblyWindowsEffect::StepMovedResized(EffectWindow *w)
{
    SetExternalBlurSuppressed(w, true);

    const RectF newGeometry = w->frameGeometry();
    auto infoIt = _windows.find(w);
    if (infoIt == _windows.end()) {
        WindowWobblyInfos newWobblyInfo;
        InitWobblyInfo(newWobblyInfo, newGeometry);
        infoIt = _windows.insert(w, std::move(newWobblyInfo));
        redirect(w);
        const bool providerEffectLoaded =
            effects && effects->loadedEffects().contains(QStringLiteral("better_blur_dx_wobbly_api"));
        if (_embeddedBlurFallbackEnabled && !providerEffectLoaded) {
            PrewarmEmbeddedBlurRenderTargets(w);
        }
    }

    WindowWobblyInfos &wwi = infoIt.value();
    wwi.status = Free;
    std::fill(wwi.constraint.begin(), wwi.constraint.end(), false);

    const RectF maximizedArea = effects->clientArea(MaximizeArea, w);
    const bool throbDirectionOut =
        (newGeometry.top() == maximizedArea.top() && newGeometry.bottom() == maximizedArea.bottom()) ||
        (newGeometry.left() == maximizedArea.left() && newGeometry.right() == maximizedArea.right());
    const qreal magnitude = throbDirectionOut ? 10.0 : -30.0;
    for (unsigned int j = 0; j < wwi.height; ++j) {
        for (unsigned int i = 0; i < wwi.width; ++i) {
            Pair v = {magnitude * (i / static_cast<qreal>(wwi.width - 1) - 0.5),
                magnitude * (j / static_cast<qreal>(wwi.height - 1) - 0.5)};
            wwi.velocity[j * wwi.width + i] = v;
        }
    }

    // Pinning the center prevents asymmetric spring forces from drifting the mesh.
    for (unsigned int j = 1; j < wwi.height - 1; ++j) {
        for (unsigned int i = 1; i < wwi.width - 1; ++i) {
            wwi.constraint[j * wwi.width + i] = true;
        }
    }
}

} // namespace KWin
