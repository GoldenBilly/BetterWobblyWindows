/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/BetterBlurDxApi.h"
#include "TestSupport.h"

#include <QVariantMap>

#include <array>
#include <cstddef>
#include <type_traits>

static_assert(BetterBlurDxApi::Version == 1);
static_assert(std::is_standard_layout_v<BetterBlurDxApi::MeshVertex>);
static_assert(std::is_standard_layout_v<BetterBlurDxApi::MeshBuildInput>);
static_assert(std::is_standard_layout_v<BetterBlurDxApi::MeshView>);
static_assert(std::is_standard_layout_v<BetterBlurDxApi::MeshProvider>);
static_assert(BetterBlurDxApi::RequestRole != BetterBlurDxApi::ResultRole);

namespace
{
bool BuildTestMesh(quintptr context, const BetterBlurDxApi::MeshBuildInput *input, BetterBlurDxApi::MeshView *output)
{
    static constexpr std::array<BetterBlurDxApi::MeshVertex, 3> Vertices{
        BetterBlurDxApi::MeshVertex{0.0f, 0.0f, 10.0f, 20.0f},
        BetterBlurDxApi::MeshVertex{1.0f, 0.0f, 11.0f, 20.0f},
        BetterBlurDxApi::MeshVertex{0.0f, 1.0f, 10.0f, 21.0f},
    };

    if (context != 0x1234 || !input || !output || input->textureWidth <= 0 || input->textureHeight <= 0) {
        return false;
    }

    output->vertices = Vertices.data();
    output->vertexCount = static_cast<int>(Vertices.size());
    output->opacity = 0.75f;
    return true;
}
} // namespace

int main()
{
    using namespace BetterBlurDxApi;

    const MeshVertex defaultVertex;
    CHECK(defaultVertex.x == 0.0f);
    CHECK(defaultVertex.y == 0.0f);
    CHECK(defaultVertex.cacheX == 0.0f);
    CHECK(defaultVertex.cacheY == 0.0f);

    const MeshBuildInput defaultInput;
    CHECK(defaultInput.effectWindow == 0);
    CHECK(defaultInput.windowPaintData == 0);
    CHECK(defaultInput.renderScale == 1.0);
    CHECK(defaultInput.cacheOriginX == 0.0);
    CHECK(defaultInput.cacheOriginY == 0.0);
    CHECK(defaultInput.textureWidth == 0);
    CHECK(defaultInput.textureHeight == 0);

    const MeshView defaultView;
    CHECK(defaultView.vertices == nullptr);
    CHECK(defaultView.vertexCount == 0);
    CHECK(defaultView.opacity == 1.0f);

    MeshProvider provider;
    provider.context = 0x1234;
    provider.BuildMesh = &BuildTestMesh;

    CHECK(provider.apiVersion == Version);
    CHECK(provider.structSize == sizeof(MeshProvider));

    const QVariant request = MakeRequest(&provider);
    CHECK(IsRequest(request));
    CHECK(SuppressDefaultComposite(request));
    CHECK(RequireFreshCache(request));
    CHECK(DecodeProvider(request) == &provider);

    const QVariantMap requestMap = request.toMap();
    CHECK(requestMap.size() == 4);
    CHECK(requestMap.value(QStringLiteral("apiVersion")).toInt() == Version);
    CHECK(requestMap.value(QStringLiteral("provider")).toULongLong() ==
        static_cast<qulonglong>(reinterpret_cast<quintptr>(&provider)));

    const QVariant permissiveRequest = MakeRequest(&provider, false, false);
    CHECK(IsRequest(permissiveRequest));
    CHECK(!SuppressDefaultComposite(permissiveRequest));
    CHECK(!RequireFreshCache(permissiveRequest));
    CHECK(DecodeProvider(permissiveRequest) == &provider);

    MeshBuildInput input;
    input.textureWidth = 640;
    input.textureHeight = 480;
    MeshView output;
    CHECK(provider.BuildMesh(provider.context, &input, &output));
    CHECK(output.vertices != nullptr);
    CHECK(output.vertexCount == 3);
    CHECK(output.opacity == 0.75f);

    CHECK(!IsRequest(QVariant()));
    CHECK(!SuppressDefaultComposite(QVariant()));
    CHECK(!RequireFreshCache(QVariant()));
    CHECK(DecodeProvider(QVariant()) == nullptr);

    const QVariant wrongVersion = QVariantMap{
        {QStringLiteral("apiVersion"), Version + 1},
        {QStringLiteral("provider"), static_cast<qulonglong>(reinterpret_cast<quintptr>(&provider))},
        {QStringLiteral("suppressDefaultComposite"), true},
        {QStringLiteral("requireFreshCache"), true},
    };
    CHECK(!IsRequest(wrongVersion));
    CHECK(!SuppressDefaultComposite(wrongVersion));
    CHECK(!RequireFreshCache(wrongVersion));
    CHECK(DecodeProvider(wrongVersion) == nullptr);

    const QVariant missingProvider = QVariantMap{{QStringLiteral("apiVersion"), Version}};
    CHECK(IsRequest(missingProvider));
    CHECK(DecodeProvider(missingProvider) == nullptr);

    MeshProvider wrongProviderVersion = provider;
    wrongProviderVersion.apiVersion = Version + 1;
    CHECK(DecodeProvider(MakeRequest(&wrongProviderVersion)) == nullptr);

    MeshProvider undersizedProvider = provider;
    undersizedProvider.structSize = static_cast<quint32>(sizeof(MeshProvider) - 1);
    CHECK(DecodeProvider(MakeRequest(&undersizedProvider)) == nullptr);

    MeshProvider callbacklessProvider = provider;
    callbacklessProvider.BuildMesh = nullptr;
    CHECK(DecodeProvider(MakeRequest(&callbacklessProvider)) == nullptr);

    const QVariant rendered = EncodeProviderResult(ProviderResult::Rendered);
    CHECK(DecodeProviderResult(rendered) == ProviderResult::Rendered);
    CHECK(ProviderRendered(rendered));
    CHECK(!ProviderDeclinedByPolicy(rendered));

    const QVariant technicalDecline = EncodeProviderResult(ProviderResult::Declined);
    CHECK(DecodeProviderResult(technicalDecline) == ProviderResult::Declined);
    CHECK(!ProviderRendered(technicalDecline));
    CHECK(!ProviderDeclinedByPolicy(technicalDecline));

    const QVariant policyDecline = EncodeProviderResult(ProviderResult::DeclinedByPolicy);
    CHECK(DecodeProviderResult(policyDecline) == ProviderResult::DeclinedByPolicy);
    CHECK(!ProviderRendered(policyDecline));
    CHECK(ProviderDeclinedByPolicy(policyDecline));

    const QVariant noResult = EncodeProviderResult(ProviderResult::None);
    CHECK(DecodeProviderResult(noResult) == ProviderResult::None);
    CHECK(!ProviderRendered(noResult));
    CHECK(!ProviderDeclinedByPolicy(noResult));

    const QVariant unknownResult = QVariantMap{
        {QStringLiteral("apiVersion"), Version},
        {QStringLiteral("result"), 999},
    };
    CHECK(DecodeProviderResult(unknownResult) == ProviderResult::None);
    CHECK(!ProviderRendered(unknownResult));
    CHECK(!ProviderDeclinedByPolicy(unknownResult));

    const QVariant staleResult = QVariantMap{
        {QStringLiteral("apiVersion"), Version + 1},
        {QStringLiteral("result"), static_cast<int>(ProviderResult::Rendered)},
    };
    CHECK(DecodeProviderResult(staleResult) == ProviderResult::None);
    CHECK(!ProviderRendered(staleResult));
    CHECK(!ProviderDeclinedByPolicy(staleResult));

    CHECK(DecodeProviderResult(QVariant()) == ProviderResult::None);
    CHECK(!ProviderRendered(QVariant()));
    CHECK(!ProviderDeclinedByPolicy(QVariant()));

    return 0;
}
