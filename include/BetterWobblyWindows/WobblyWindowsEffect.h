/*
    KWin - the KDE window manager
    Portions of this file are adapted from the KDE KWin Wobbly Windows effect.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>
    Portions of this file are adapted from Better Blur DX and the KWin Blur effect.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
    SPDX-FileCopyrightText: 2024-2026 Better Blur DX contributors
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later

    Official KWin Wobbly Windows source:
    https://github.com/KDE/kwin/tree/master/src/plugins/wobblywindows
    Upstream authors are credited above according to the corresponding KDE files.

    Better Blur DX source: https://github.com/xarblu/kwin-effects-better-blur-dx
    Reference snapshot: 35451011b48bd01c2afa1a58ceeac46197147b81
    See AUTHORS.md and NOTICE.md for the complete provenance record.
*/
#pragma once

#include "BetterWobblyWindows/BetterBlurDxApi.h"
#include "BetterWobblyWindows/WobblyParameterSet.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"
#include "BetterWobblyWindows/DxBlurSettings.h"
#include "BetterWobblyWindows/KWinVersion.h"

#include "effect/offscreeneffect.h"
#include "effect/timeline.h"
#include "opengl/glutils.h"

#include <QHash>
#include <QMatrix4x4>
#include <QRect>
#include <QSize>
#include <QVector4D>
#include <QLoggingCategory>
#include <KConfigWatcher>
#include <QList>

#include <map>
#include <memory>
#include <vector>

namespace KWin
{

Q_DECLARE_LOGGING_CATEGORY(KWIN_WOBBLYWINDOWS)

class GLFramebuffer;
class GLShader;
class GLTexture;

/**
 * Effect that wobbles windows
 */
class WobblyWindowsEffect : public OffscreenEffect
{
    Q_OBJECT
    Q_PROPERTY(qreal stiffness READ Stiffness)
    Q_PROPERTY(qreal drag READ Drag)
    Q_PROPERTY(qreal moveFactor READ MoveFactor)
    Q_PROPERTY(qreal xTessellation READ XTessellation)
    Q_PROPERTY(qreal yTessellation READ YTessellation)
    Q_PROPERTY(qreal minVelocity READ MinVelocity)
    Q_PROPERTY(qreal maxVelocity READ MaxVelocity)
    Q_PROPERTY(qreal stopVelocity READ StopVelocity)
    Q_PROPERTY(qreal minAcceleration READ MinAcceleration)
    Q_PROPERTY(qreal maxAcceleration READ MaxAcceleration)
    Q_PROPERTY(qreal stopAcceleration READ StopAcceleration)
    Q_PROPERTY(bool moveWobble READ IsMoveWobble)
    Q_PROPERTY(bool resizeWobble READ IsResizeWobble)
    Q_PROPERTY(bool visualOppositePointFollowOnResize READ IsVisualOppositePointFollowOnResize)
    Q_PROPERTY(qreal visualOppositePointFollowStrength READ VisualOppositePointFollowStrength)

  public:
    WobblyWindowsEffect();
    ~WobblyWindowsEffect() override;

    void reconfigure(ReconfigureFlags) override;
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data) override;
#if KWIN_VERSION < KWIN_VERSION_CODE(6, 7, 80)
    void drawWindow(const RenderTarget &renderTarget,
        const RenderViewport &viewport,
        EffectWindow *w,
        int mask,
        const Region &deviceRegion,
        WindowPaintData &data) override;
#else
    bool drawWindow(const RenderTarget &renderTarget,
        const RenderViewport &viewport,
        EffectWindow *w,
        int mask,
        const Region &deviceRegion,
        WindowPaintData &data) override;
#endif
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        // Keep the same relative position as upstream Wobbly Windows. Blur /
        // Better Blur DX run earlier in the chain and skip transformed windows;
        // the embedded pass is then painted immediately before the deformed
        // window itself.
        return BetterWobblyWindows::EffectChainPosition;
    }

    void SetStiffness(qreal stiffness);
    void SetDrag(qreal drag);
    void SetVelocityThreshold(qreal velocityThreshold);
    void SetMoveFactor(qreal factor);

    struct Pair {
        qreal x;
        qreal y;
    };

    enum WindowStatus {
        Free,
        Moving,
    };

    static bool Supported();

    qreal Stiffness() const;
    qreal Drag() const;
    qreal MoveFactor() const;
    qreal XTessellation() const;
    qreal YTessellation() const;
    qreal MinVelocity() const;
    qreal MaxVelocity() const;
    qreal StopVelocity() const;
    qreal MinAcceleration() const;
    qreal MaxAcceleration() const;
    qreal StopAcceleration() const;
    bool IsMoveWobble() const;
    bool IsResizeWobble() const;
    bool IsVisualOppositePointFollowOnResize() const;
    qreal VisualOppositePointFollowStrength() const;

  protected:
    void apply(EffectWindow *w, int mask, WindowPaintData &data, WindowQuadList &quads) override;

  public Q_SLOTS:
    void SlotWindowAdded(KWin::EffectWindow *w);
    void SlotWindowStartUserMovedResized(KWin::EffectWindow *w);
    void SlotWindowStepUserMovedResized(KWin::EffectWindow *w, const RectF &geometry);
    void SlotWindowFinishUserMovedResized(KWin::EffectWindow *w);
    void SlotWindowMaximizeStateChanged(KWin::EffectWindow *w, bool horizontal, bool vertical);

  private:
    void StartMovedResized(EffectWindow *w);
    void StepMovedResized(EffectWindow *w);
    bool UpdateWindowWobblyDatas(EffectWindow *w, qreal time);

    struct BlurOffset {
        float minOffset;
        float maxOffset;
        int expandSize;
    };

    struct BlurStrengthValue {
        int iteration;
        float offset;
    };

    struct EmbeddedBlurPass {
        std::unique_ptr<GLShader> shader;
        int mvpMatrixLocation = -1;
        int colorMatrixLocation = -1;
        int offsetLocation = -1;
        int halfpixelLocation = -1;
        int boxLocation = -1;
        int cornerRadiusLocation = -1;
        int opacityLocation = -1;
        int textureSizeLocation = -1;
        int sourceClampLocation = -1;
        int edgeSoftnessLocation = -1;
        int sourceYFlipLocation = -1;
        int refractionEnabledLocation = -1;
        int refractionRectSizeLocation = -1;
        int refractionEdgeSizePixelsLocation = -1;
        int refractionCornerRadiusPixelsLocation = -1;
        int refractionStrengthLocation = -1;
        int refractionNormalPowLocation = -1;
        int refractionRgbFringingLocation = -1;
        int refractionTextureRepeatModeLocation = -1;
        int refractionModeLocation = -1;
    };

    struct EmbeddedNoisePass {
        std::unique_ptr<GLShader> shader;
        std::unique_ptr<GLTexture> noiseTexture;
        int mvpMatrixLocation = -1;
        int noiseTextureSizeLocation = -1;
        int blurTextureSizeLocation = -1;
        int renderScaleLocation = -1;
        int boxLocation = -1;
        int cornerRadiusLocation = -1;
        int opacityLocation = -1;
        int noiseModeLocation = -1;
        qreal noiseTextureScale = 0.0;
        int noiseTextureStrength = -1;
    };

    struct EmbeddedBlurRenderData {
        std::vector<std::unique_ptr<GLTexture>> textures;
        std::vector<std::unique_ptr<GLFramebuffer>> framebuffers;
        // Better Blur DX resolves its final Kawase/color/noise pass into a
        // full-resolution cache texture and only then composites that cache.
        // Keeping this extra target is required to match its filtering order.
        std::unique_ptr<GLTexture> compositeTexture;
        std::unique_ptr<GLFramebuffer> compositeFramebuffer;
        QSize size;
        unsigned int internalFormat = 0;

        // Guarded capture area in window-local logical coordinates. It stays
        // stable during a drag and grows only if deformation exceeds the guard.
        QRect localBackgroundRect;
        QSize sourceWindowSize;

        // Last successfully captured background source. The reusable source FBO
        // can be held for a few transient compositor frames when KWin gives an
        // empty/uncapturable render area while dragging downward or across an
        // output edge. Keeping the logical rect and clamp with the source makes
        // the fallback draw in the right screen position instead of stretching
        // stale pixels into the current capture rect.
        QRect lastBackgroundRect;
        // A reusable source belongs to one KWin render view/output. Reusing a
        // clipped source on another output (or at another scale) produces a
        // hard seam and stale pixels while a window crosses monitor borders.
        QRect lastRenderRect;
        qreal lastRenderScale = 0.0;
        QVector4D lastSourceClamp = QVector4D(0.0f, 0.0f, 1.0f, 1.0f);
        const EffectWindow *lastSourceWindow = nullptr;
        bool hasValidSource = false;
        bool lastSourceWasClipped = false;
        int fallbackFramesRemaining = 0;

        // Avoid retrying the same impossible VRAM allocation on every paint.
        QSize lastFailedAllocationSize;
        unsigned int lastFailedAllocationFormat = 0;
        int allocationRetryFramesRemaining = 0;
    };

    struct WindowWobblyInfos {
        QList<Pair> origin;
        QList<Pair> position;
        QList<Pair> velocity;
        QList<Pair> acceleration;
        QList<Pair> buffer;

        // Constrained points follow only the window-relative destination; neighboring springs do not influence them.
        QList<bool> constraint;

        unsigned int width;
        unsigned int height;
        unsigned int count;

        WindowStatus status;
        bool wobblying = false;

        // Tracks whether KWin has actually entered the interactive
        // move/resize state. If that state disappears without the normal
        // finish signal, the effect cancels the redirected/blur state to avoid
        // stale texture artifacts after screen locking or edge-snap grabs.
        bool seenInteractiveMoveResize = false;
        int missedInteractiveMoveResizeFrames = 0;

        // Cross-fade between the normal compositor blur state and the embedded
        // deformed blur. The same progress is reversed on release, so re-grabbing
        // a window during fade-out continues smoothly instead of restarting.
        qreal embeddedBlurTransitionProgress = 0.0;

        // Consecutive frames where the embedded blur had no fresh or held source.
        // After a short grace period the effect releases the external blur
        // suppression role, so a compositor-source failure degrades to the normal
        // blur path instead of leaving the window permanently unblurred until the
        // effect is reloaded.
        int embeddedBlurSourceMissFrames = 0;

        // A resize edge becomes elastic only after that edge has moved.
        bool canWobbleTop = false;
        bool canWobbleLeft = false;
        bool canWobbleRight = false;
        bool canWobbleBottom = false;
        RectF resizeOriginalRect;

        // Visual-only elastic counter-resize state. Geometry changes inject a
        // displacement into the opposite edge; a separate spring returns the
        // rendered mesh to the real frame geometry after motion stops.
        Pair visualOppositePointNormalized = {0.5, 0.5};
        Pair visualOppositePointFollowOffset = {0.0, 0.0};
        Pair visualOppositePointFollowVelocity = {0.0, 0.0};
        Pair visualOppositePointPendingImpulse = {0.0, 0.0};
        RectF visualOppositePointLastRect;
        bool visualOppositePointLastRectValid = false;
        bool visualOppositePointAnchorValid = false;

        AnimationClock clock;
    };

    QHash<const EffectWindow *, WindowWobblyInfos> _windows;

    Region _updateRegion;

    EmbeddedBlurPass _embeddedDownsamplePass;
    EmbeddedBlurPass _embeddedUpsamplePass;
    EmbeddedBlurPass _embeddedOnscreenPass;
    EmbeddedBlurPass _embeddedCompositePass;
    EmbeddedNoisePass _embeddedNoisePass;
    std::vector<BlurOffset> _embeddedBlurOffsets;
    std::vector<BlurStrengthValue> _embeddedBlurStrengthValues;
    std::map<const EffectWindow *, EmbeddedBlurRenderData> _embeddedBlurRenderData;
    // One reusable GPU render-target set for the embedded blur. A move/resize is
    // interactive for one window at a time, so sharing this pool avoids allocating
    // textures/FBOs for every newly dragged window and removes most drag-start
    // micro-stutters when the guarded required size is already warm.
    EmbeddedBlurRenderData _embeddedBlurReusableRenderData;
    std::vector<GLVertex2D> _embeddedBlurVertices;
    BetterBlurDxApi::MeshProvider _betterBlurDxProvider;
    std::vector<BetterBlurDxApi::MeshVertex> _betterBlurDxProviderVertices;
    QMatrix4x4 _embeddedBlurColorMatrix;
    int _embeddedBlurIterationCount = BetterWobblyWindows::InitialEmbeddedBlurIterationCount;
    float _embeddedBlurOffset = BetterWobblyWindows::InitialEmbeddedBlurOffset;
    int _embeddedBlurExpandSize = BetterWobblyWindows::InitialEmbeddedBlurExpandSize;
    int _embeddedBlurNoiseStrength = 0;
    qreal _embeddedBlurCornerRadius = 0.0;
    bool _embeddedRefractionEnabled = false;
    qreal _embeddedRefractionEdgeSizePixels = 0.0;
    qreal _embeddedRefractionCornerRadiusPixels = 0.0;
    qreal _embeddedRefractionStrength = 0.0;
    qreal _embeddedRefractionNormalPow = 0.0;
    qreal _embeddedRefractionRgbFringing = 0.0;
    int _embeddedRefractionTextureRepeatMode = 0;
    int _embeddedRefractionMode = 0;
    // 0 = exact Better Blur DX color matrix, 1 = clamp boosts above 100%, 2 = no color transform.
    int _embeddedBlurColorMode = 0;
    // 0 = exact Better Blur DX additive noise, 1 = alpha-safe premultiplied noise.
    int _embeddedBlurNoiseMode = 0;
    int _embeddedBlurHandoffFrames = BetterWobblyWindows::DefaultHandoffFrames;
    int _embeddedBlurEdgeSoftnessPixels = 0;
    bool _embeddedBlurClearIntermediateTargets = false;
    int _embeddedBlurFallbackFrames = BetterWobblyWindows::DefaultFallbackFrames;
    bool _embeddedBlurSourceYFlip = BetterWobblyWindows::DefaultSourceYFlip;
    int _maxPhysicsStepsPerFrame = BetterWobblyWindows::DefaultPhysicsStepsPerFrame;
    bool _embeddedBlurValid = false;
    bool _embeddedBlurEnabled = true;
    bool _embeddedBlurFallbackEnabled = true;

    bool _embeddedBlurUseBetterBlurDXSettings = true;
    KConfigWatcher::Ptr _configWatcher;

    qreal _stiffness = 0.15;
    qreal _drag = 0.80;
    qreal _moveFactor = 0.10;

    // qreal avoids repeated conversions and accidental integer division in the
    // mesh and physics calculations.
    qreal _xTessellation = 20.0;
    qreal _yTessellation = 20.0;

    qreal _minVelocity = 0.0;
    qreal _maxVelocity = 1000.0;
    qreal _stopVelocity = 0.5;
    qreal _minAcceleration = 0.0;
    qreal _maxAcceleration = 1000.0;
    qreal _stopAcceleration = 0.5;

    bool _moveWobble = true;
    bool _resizeWobble = true;
    bool _visualOppositePointFollowOnResize = false;
    qreal _visualOppositePointFollowStrength = 0.08;

    void InitEmbeddedBlur();
    void ReconfigureEmbeddedBlur();
    void InitEmbeddedBlurStrengthValues();
    void DrawEmbeddedBlur(const RenderTarget &renderTarget,
        const RenderViewport &viewport,
        EffectWindow *w,
        int mask,
        const Region &deviceRegion,
        WindowPaintData &data);
    bool EnsureEmbeddedBlurRenderTargets(
        EmbeddedBlurRenderData &renderData, const QSize &size, unsigned int textureFormat);
    void ReleaseEmbeddedBlurRenderTargets(EmbeddedBlurRenderData &renderData);
    quint64 EmbeddedBlurRenderTargetBytes(const EmbeddedBlurRenderData &renderData) const;
    void PrewarmEmbeddedBlurRenderTargets(EffectWindow *w);
    GLTexture *EnsureEmbeddedBlurNoiseTexture();
    WindowQuadList MakeWindowQuads(EffectWindow *w) const;
    bool DeformWindowQuads(EffectWindow *w,
        WindowPaintData &data,
        WindowQuadList &quads,
        bool updateRepaintRegion,
        bool forceGrid = false,
        bool quadsAlreadyTessellated = false);
    bool ShouldPaintEmbeddedBlur(EffectWindow *w) const;
    static bool BuildBetterBlurDxMesh(
        quintptr context, const BetterBlurDxApi::MeshBuildInput *input, BetterBlurDxApi::MeshView *output);
    void SetExternalBlurSuppressed(EffectWindow *w, bool suppressed);
    void CleanupWindow(EffectWindow *w);

    void InitWobblyInfo(WindowWobblyInfos &wwi, RectF geometry) const;

    WobblyWindowsEffect::Pair ComputeBezierPoint(const WindowWobblyInfos &wwi, Pair point) const;

    static void HeightRingLinearMean(QList<Pair> &data, WindowWobblyInfos &wwi);

    void SetParameterSet(const BetterWobblyWindows::WobblyParameterSet &parameterSet);
};

} // namespace KWin
