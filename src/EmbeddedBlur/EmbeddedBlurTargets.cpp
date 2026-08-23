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

#include <limits>

namespace KWin
{
namespace
{
quint64 TextureFormatBytesPerPixel(unsigned int textureFormat)
{
    switch (textureFormat) {
    case GL_RGBA32F:
        return 16;
    case GL_RGBA16F:
        return 8;
    case GL_RGBA8:
#ifdef GL_SRGB8_ALPHA8
    case GL_SRGB8_ALPHA8:
#endif
#ifdef GL_RGB10_A2
    case GL_RGB10_A2:
#endif
        return 4;
    default:
        // Unknown compositor formats are treated conservatively. Most formats
        // reaching this path are 4 or 8 bytes per pixel.
        return 8;
    }
}

quint64 EstimateTexturePyramidBytes(const QSize &baseSize, int levelCount, unsigned int textureFormat)
{
    if (baseSize.isEmpty() || levelCount <= 0) {
        return 0;
    }

    const quint64 bytesPerPixel = TextureFormatBytesPerPixel(textureFormat);
    quint64 total = 0;
    for (int level = 0; level < levelCount; ++level) {
        const QSize levelSize = BetterWobblyWindows::DownsampleTextureSize(baseSize, level);
        const quint64 pixels = static_cast<quint64>(levelSize.width()) * static_cast<quint64>(levelSize.height());
        if (pixels > (std::numeric_limits<quint64>::max() - total) / bytesPerPixel) {
            return std::numeric_limits<quint64>::max();
        }
        total += pixels * bytesPerPixel;
    }
    return total;
}
} // namespace

void WobblyWindowsEffect::ReleaseEmbeddedBlurRenderTargets(EmbeddedBlurRenderData &renderData)
{
    // Destroy FBOs before their attached textures.
    renderData.compositeFramebuffer.reset();
    renderData.compositeTexture.reset();
    renderData.framebuffers.clear();
    renderData.textures.clear();
    renderData.size = QSize();
    renderData.internalFormat = 0;
    renderData.localBackgroundRect = QRect();
    renderData.sourceWindowSize = QSize();
    renderData.lastBackgroundRect = QRect();
    renderData.lastRenderRect = QRect();
    renderData.lastRenderScale = 0.0;
    renderData.lastSourceClamp = QVector4D(0.0f, 0.0f, 1.0f, 1.0f);
    renderData.lastSourceWindow = nullptr;
    renderData.hasValidSource = false;
    renderData.lastSourceWasClipped = false;
    renderData.fallbackFramesRemaining = 0;
    renderData.lastFailedAllocationSize = QSize();
    renderData.lastFailedAllocationFormat = 0;
    renderData.allocationRetryFramesRemaining = 0;
}

quint64 WobblyWindowsEffect::EmbeddedBlurRenderTargetBytes(const EmbeddedBlurRenderData &renderData) const
{
    const quint64 pyramid = EstimateTexturePyramidBytes(renderData.size,
        static_cast<int>(std::min(renderData.textures.size(), renderData.framebuffers.size())),
        renderData.internalFormat);
    const quint64 composite = renderData.compositeTexture ? static_cast<quint64>(renderData.size.width()) *
            static_cast<quint64>(renderData.size.height()) * TextureFormatBytesPerPixel(renderData.internalFormat)
                                                          : 0;
    return pyramid > std::numeric_limits<quint64>::max() - composite ? std::numeric_limits<quint64>::max()
                                                                     : pyramid + composite;
}

bool WobblyWindowsEffect::EnsureEmbeddedBlurRenderTargets(
    EmbeddedBlurRenderData &renderData, const QSize &size, unsigned int textureFormat)
{
    if (size.isEmpty()) {
        return false;
    }

    const int requiredLevelCount = std::max(1, _embeddedBlurIterationCount + 1);
    const QSize requestedSize = BetterWobblyWindows::BucketEmbeddedBlurTextureSize(size);

    const bool sameFormat = renderData.internalFormat == textureFormat;
    const bool exactSizeMatch = sameFormat && renderData.size == requestedSize;
    const bool hasRequiredLevels = renderData.textures.size() >= static_cast<std::size_t>(requiredLevelCount) &&
        renderData.framebuffers.size() >= static_cast<std::size_t>(requiredLevelCount) && renderData.compositeTexture &&
        renderData.compositeFramebuffer && renderData.compositeFramebuffer->valid() &&
        renderData.compositeTexture->size() == requestedSize &&
        renderData.compositeTexture->internalFormat() == textureFormat;

    if (exactSizeMatch && hasRequiredLevels) {
        // A lower blur strength must release the now-unused levels instead of
        // keeping their VRAM until the effect is unloaded.
        if (renderData.framebuffers.size() > static_cast<std::size_t>(requiredLevelCount)) {
            renderData.framebuffers.resize(static_cast<std::size_t>(requiredLevelCount));
        }
        if (renderData.textures.size() > static_cast<std::size_t>(requiredLevelCount)) {
            renderData.textures.resize(static_cast<std::size_t>(requiredLevelCount));
        }
        renderData.lastFailedAllocationSize = QSize();
        renderData.lastFailedAllocationFormat = 0;
        renderData.allocationRetryFramesRemaining = 0;
        return true;
    }

    const QSize allocationSize = requestedSize;
    const quint64 pyramidBytes = EstimateTexturePyramidBytes(allocationSize, requiredLevelCount, textureFormat);
    const quint64 compositeBytes = static_cast<quint64>(allocationSize.width()) *
        static_cast<quint64>(allocationSize.height()) * TextureFormatBytesPerPixel(textureFormat);
    const quint64 requestedBytes = pyramidBytes > std::numeric_limits<quint64>::max() - compositeBytes
        ? std::numeric_limits<quint64>::max()
        : pyramidBytes + compositeBytes;

    if (renderData.allocationRetryFramesRemaining > 0 && renderData.lastFailedAllocationFormat == textureFormat) {
        const quint64 failedPyramidBytes =
            EstimateTexturePyramidBytes(renderData.lastFailedAllocationSize, requiredLevelCount, textureFormat);
        const quint64 failedCompositeBytes = static_cast<quint64>(renderData.lastFailedAllocationSize.width()) *
            static_cast<quint64>(renderData.lastFailedAllocationSize.height()) *
            TextureFormatBytesPerPixel(textureFormat);
        const quint64 failedBytes = failedPyramidBytes > std::numeric_limits<quint64>::max() - failedCompositeBytes
            ? std::numeric_limits<quint64>::max()
            : failedPyramidBytes + failedCompositeBytes;
        if (requestedBytes >= failedBytes) {
            --renderData.allocationRetryFramesRemaining;
            return false;
        }
    }

    GLint maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    const bool exceedsDimensionLimit = maximumTextureSize > 0 &&
        (allocationSize.width() > maximumTextureSize || allocationSize.height() > maximumTextureSize);
    const bool exceedsMemoryBudget = requestedBytes > BetterWobblyWindows::MaximumEmbeddedBlurAllocationBytes;

    const auto rememberAllocationFailure = [&](const char *reason) {
        renderData.lastFailedAllocationSize = allocationSize;
        renderData.lastFailedAllocationFormat = textureFormat;
        renderData.allocationRetryFramesRemaining = BetterWobblyWindows::EmbeddedBlurAllocationRetryFrames;
        renderData.hasValidSource = false;
        renderData.lastRenderRect = QRect();
        renderData.lastRenderScale = 0.0;
        renderData.lastSourceWindow = nullptr;

        BetterWobblyWindows::WriteBlurTraceLine(
            QStringLiteral("ensureRenderTargets FAILED %1 requested=%2x%3 bytes=%4 maxTexture=%5")
                .arg(QString::fromLatin1(reason))
                .arg(allocationSize.width())
                .arg(allocationSize.height())
                .arg(requestedBytes)
                .arg(maximumTextureSize));
    };

    if (exceedsDimensionLimit || exceedsMemoryBudget) {
        rememberAllocationFailure(exceedsDimensionLimit ? "texture-size-limit" : "memory-budget");
        return false;
    }

    const auto createLevel = [&](const QSize &baseSize,
                                 int level,
                                 std::unique_ptr<GLTexture> &texture,
                                 std::unique_ptr<GLFramebuffer> &framebuffer) -> bool {
        const QSize textureSize = BetterWobblyWindows::DownsampleTextureSize(baseSize, level);
        texture = GLTexture::allocate(textureFormat, textureSize);
        if (!texture) {
            static bool kernelErrorLogged = false;
            if (!kernelErrorLogged) {
                BetterWobblyWindows::WriteKernelLogLine("failed to allocate embedded blur texture",
                    BetterWobblyWindows::KernelLogPriority::Error);
                qCWarning(KWIN_WOBBLYWINDOWS) << "Failed to allocate embedded blur texture" << textureSize;
                kernelErrorLogged = true;
            }
            return false;
        }

        texture->setFilter(GL_LINEAR);
        texture->setWrapMode(GL_CLAMP_TO_EDGE);
        texture->setSwizzle(GL_RED, GL_GREEN, GL_BLUE, GL_ONE);

        framebuffer = std::make_unique<GLFramebuffer>(texture.get());
        if (!framebuffer->valid()) {
            static bool kernelErrorLogged = false;
            if (!kernelErrorLogged) {
                BetterWobblyWindows::WriteKernelLogLine("failed to create embedded blur framebuffer",
                    BetterWobblyWindows::KernelLogPriority::Error);
                qCWarning(KWIN_WOBBLYWINDOWS) << "Failed to create embedded blur framebuffer";
                kernelErrorLogged = true;
            }
            framebuffer.reset();
            texture.reset();
            return false;
        }
        return true;
    };

    if (exactSizeMatch && renderData.compositeTexture && renderData.compositeFramebuffer &&
        renderData.compositeFramebuffer->valid()) {
        // Increasing strength only needs the missing downsample levels. Preserve
        // level zero and its captured source instead of reallocating the pool.
        const std::size_t firstMissingLevel = std::min(renderData.textures.size(), renderData.framebuffers.size());
        renderData.framebuffers.resize(firstMissingLevel);
        renderData.textures.resize(firstMissingLevel);
        std::vector<std::unique_ptr<GLTexture>> newTextures;
        std::vector<std::unique_ptr<GLFramebuffer>> newFramebuffers;
        newTextures.reserve(static_cast<std::size_t>(requiredLevelCount) - firstMissingLevel);
        newFramebuffers.reserve(static_cast<std::size_t>(requiredLevelCount) - firstMissingLevel);

        for (int level = static_cast<int>(firstMissingLevel); level < requiredLevelCount; ++level) {
            std::unique_ptr<GLTexture> texture;
            std::unique_ptr<GLFramebuffer> framebuffer;
            if (!createLevel(renderData.size, level, texture, framebuffer)) {
                rememberAllocationFailure("append-level");
                return false;
            }
            newTextures.push_back(std::move(texture));
            newFramebuffers.push_back(std::move(framebuffer));
        }

        for (auto &texture : newTextures) {
            renderData.textures.push_back(std::move(texture));
        }
        for (auto &framebuffer : newFramebuffers) {
            renderData.framebuffers.push_back(std::move(framebuffer));
        }
        renderData.lastFailedAllocationSize = QSize();
        renderData.lastFailedAllocationFormat = 0;
        renderData.allocationRetryFramesRemaining = 0;
        return true;
    }

    // A different base size must build a new exact DX-sized pyramid. Reusing a
    // larger allocation changes the level dimensions and therefore the blur kernel.
    // Release first to avoid a transient double-allocation while preserving parity.
    ReleaseEmbeddedBlurRenderTargets(renderData);

    std::vector<std::unique_ptr<GLTexture>> textures;
    std::vector<std::unique_ptr<GLFramebuffer>> framebuffers;
    textures.reserve(static_cast<std::size_t>(requiredLevelCount));
    framebuffers.reserve(static_cast<std::size_t>(requiredLevelCount));

    for (int level = 0; level < requiredLevelCount; ++level) {
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> framebuffer;
        if (!createLevel(requestedSize, level, texture, framebuffer)) {
            renderData.lastFailedAllocationSize = requestedSize;
            renderData.lastFailedAllocationFormat = textureFormat;
            renderData.allocationRetryFramesRemaining = BetterWobblyWindows::EmbeddedBlurAllocationRetryFrames;
            BetterWobblyWindows::WriteBlurTraceLine(
                QStringLiteral("ensureRenderTargets FAILED allocate level=%1 requested=%2x%3")
                    .arg(level)
                    .arg(requestedSize.width())
                    .arg(requestedSize.height()));
            return false;
        }
        textures.push_back(std::move(texture));
        framebuffers.push_back(std::move(framebuffer));
    }

    std::unique_ptr<GLTexture> compositeTexture = GLTexture::allocate(textureFormat, requestedSize);
    if (!compositeTexture) {
        rememberAllocationFailure("composite-texture");
        return false;
    }
    compositeTexture->setFilter(GL_LINEAR);
    compositeTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    std::unique_ptr<GLFramebuffer> compositeFramebuffer = std::make_unique<GLFramebuffer>(compositeTexture.get());
    if (!compositeFramebuffer->valid()) {
        rememberAllocationFailure("composite-framebuffer");
        return false;
    }
    GLFramebuffer::pushFramebuffer(compositeFramebuffer.get());
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GLFramebuffer::popFramebuffer();

    renderData.textures = std::move(textures);
    renderData.framebuffers = std::move(framebuffers);
    renderData.compositeTexture = std::move(compositeTexture);
    renderData.compositeFramebuffer = std::move(compositeFramebuffer);
    renderData.size = requestedSize;
    renderData.internalFormat = textureFormat;
    renderData.hasValidSource = false;
    renderData.lastSourceWasClipped = false;
    renderData.lastRenderRect = QRect();
    renderData.lastRenderScale = 0.0;
    renderData.fallbackFramesRemaining = 0;
    renderData.lastBackgroundRect = QRect();
    renderData.lastSourceClamp = QVector4D(0.0f, 0.0f, 1.0f, 1.0f);
    renderData.lastSourceWindow = nullptr;
    renderData.lastFailedAllocationSize = QSize();
    renderData.lastFailedAllocationFormat = 0;
    renderData.allocationRetryFramesRemaining = 0;

    if (BetterWobblyWindows::IsBlurTraceEnabled()) {
        BetterWobblyWindows::WriteBlurTraceLine(
            QStringLiteral("ensureRenderTargets allocated requested=%1x%2 exact=%3x%4 levels=%5 bytes=%6")
                .arg(size.width())
                .arg(size.height())
                .arg(requestedSize.width())
                .arg(requestedSize.height())
                .arg(requiredLevelCount)
                .arg(requestedBytes));
    }
    return true;
}

void WobblyWindowsEffect::PrewarmEmbeddedBlurRenderTargets(EffectWindow *w)
{
    if (!w || !_embeddedBlurEnabled || !_embeddedBlurFallbackEnabled || !_embeddedBlurValid) {
        return;
    }

    const qreal windowLogicalWidth = std::max<qreal>(1.0, w->width());
    const qreal windowLogicalHeight = std::max<qreal>(1.0, w->height());
    const int blurKernelPadding = std::max(_embeddedBlurExpandSize * 2,
        static_cast<int>(std::ceil((_embeddedBlurOffset + BetterWobblyWindows::BlurKernelOffsetBias) *
            BetterWobblyWindows::BlurKernelScaleMultiplier)));
    const int wobblePadding = std::clamp(static_cast<int>(std::ceil(std::max(windowLogicalWidth, windowLogicalHeight) *
                                             BetterWobblyWindows::WobblePaddingWindowRatio)),
        BetterWobblyWindows::MinimumWobblePaddingPixels,
        BetterWobblyWindows::MaximumWobblePaddingPixels);
    const int capturePadding = blurKernelPadding + wobblePadding;
    const QSize requestedSize(std::max(BetterWobblyWindows::MinimumTextureDimension,
                                  static_cast<int>(std::ceil(windowLogicalWidth + capturePadding * 2))),
        std::max(BetterWobblyWindows::MinimumTextureDimension,
            static_cast<int>(std::ceil(windowLogicalHeight + capturePadding * 2))));

    BetterWobblyWindows::ScopedEmbeddedBlurGlState embeddedBlurGlState;

    if (_embeddedBlurReusableRenderData.internalFormat != 0) {
        EnsureEmbeddedBlurRenderTargets(_embeddedBlurReusableRenderData,
            requestedSize,
            _embeddedBlurReusableRenderData.internalFormat);
    }
    EnsureEmbeddedBlurNoiseTexture();
}

GLTexture *WobblyWindowsEffect::EnsureEmbeddedBlurNoiseTexture()
{
    if (_embeddedBlurNoiseStrength <= 0 || !_embeddedNoisePass.shader) {
        _embeddedNoisePass.noiseTexture.reset();
        _embeddedNoisePass.noiseTextureScale = 0.0;
        _embeddedNoisePass.noiseTextureStrength = -1;
        return nullptr;
    }

    // Better Blur DX keys its noise texture specifically to primary-screen
    // logical DPI, not the maximum devicePixelRatio across outputs. Using the
    // latter changes the noise frequency and perceived brightness on mixed-DPI
    // setups even when all blur/color settings are identical.
    const QScreen *primaryScreen = QGuiApplication::primaryScreen();
    const qreal scale = primaryScreen
        ? std::max<qreal>(1.0, primaryScreen->logicalDotsPerInch() / BetterWobblyWindows::StandardDpi)
        : 1.0;

    if (_embeddedNoisePass.noiseTexture && qFuzzyCompare(_embeddedNoisePass.noiseTextureScale, scale) &&
        _embeddedNoisePass.noiseTextureStrength == _embeddedBlurNoiseStrength) {
        return _embeddedNoisePass.noiseTexture.get();
    }

    // Match Better Blur DX noise generation: grayscale 256x256 image, random
    // values in [0, NoiseStrength), DPI-scaled, then uploaded with nearest +
    // repeat sampling. Using RGB32 here changed the sampled noise texture and
    // made the moving blur look brighter/different from DX.
    std::srand(static_cast<uint>(QTime::currentTime().msec()));
    QImage noiseImage(QSize(BetterWobblyWindows::NoiseTextureWidth, BetterWobblyWindows::NoiseTextureHeight),
        QImage::Format_Grayscale8);
    for (int y = 0; y < noiseImage.height(); ++y) {
        uchar *line = noiseImage.scanLine(y);
        for (int x = 0; x < noiseImage.width(); ++x) {
            line[x] = static_cast<uchar>(std::rand() % _embeddedBlurNoiseStrength);
        }
    }

    if (!qFuzzyCompare(scale, 1.0)) {
        noiseImage = noiseImage.scaled(noiseImage.size() * scale);
    }

    _embeddedNoisePass.noiseTexture = GLTexture::upload(noiseImage);
    if (!_embeddedNoisePass.noiseTexture) {
        return nullptr;
    }

    _embeddedNoisePass.noiseTexture->setFilter(GL_NEAREST);
    _embeddedNoisePass.noiseTexture->setWrapMode(GL_REPEAT);
    _embeddedNoisePass.noiseTextureScale = scale;
    _embeddedNoisePass.noiseTextureStrength = _embeddedBlurNoiseStrength;
    return _embeddedNoisePass.noiseTexture.get();
}

} // namespace KWin
