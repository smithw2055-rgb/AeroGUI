#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Rhi/Graphics.hpp>
#include <Aero/Render/Renderer.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>

#include <algorithm>
#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Rhi;
using namespace Aero::Render;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class RenderBox final : public FrameworkElement {
public:
    explicit RenderBox(TypeId type) noexcept
        : FrameworkElement(type) {}

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        return Size{std::min(32.0, available.width),
            std::min(24.0, available.height)};
    }

    Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        return finalSize;
    }

    Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override {
        Result<void> opacity = builder.PushOpacity(0.5);
        if (!opacity) return opacity;
        Result<void> fill = builder.FillRect(
            {0.0, 0.0, RenderSize().width, RenderSize().height},
            {0.2F, 0.4F, 0.6F, 1.0F});
        if (!fill) return fill;
        return builder.PopOpacity();
    }
};

struct Fixture final {
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typesBehaviors{types};
    MetadataRegistrationTypes typesRegistration{types, typesBehaviors};
    DependencyPropertyRegistry properties{types, typesBehaviors};
    Dispatcher dispatcher;
    ObjectServicesScope presentation{dispatcher, properties};
    TypeId objectType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;

    bool Build() {
        const StringView ns("urn:aero");
        objectType = MakeTypeId(ns, StringView("Object"));
        elementType = MakeTypeId(ns, StringView("FrameworkElement"));
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("Object"), InvalidTypeId, TypeFlags::None, nullptr)));
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("FrameworkElement"), objectType, TypeFlags::None, nullptr)));
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        return true;
    }
};

bool BuildPlan(RenderPlan& destination) {
    Fixture fixture;
    CHECK(fixture.Build());
    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());
    RenderManager renderer(fixture.dispatcher);
    CHECK(renderer.Initialize());

    RenderBox root(fixture.elementType);
    CHECK(tree.SetRoot(&root));
    CHECK(layout.SetRoot(&root, {64.0, 48.0}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(renderer.SetRoot(&root));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    destination = renderer.CurrentPlan();

    CHECK(renderer.SetRoot(nullptr));
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(root));
    return true;
}

RendererShaderSet MakeShaders() noexcept {
    static constexpr std::uint8_t VertexBytes[] = {0x01U, 0x02U, 0x03U};
    static constexpr std::uint8_t FragmentBytes[] = {0x04U, 0x05U, 0x06U};
    auto pair = [](std::uint64_t baseId) noexcept {
        RendererShaderPair result;
        result.vertex.stage = ShaderStage::Vertex;
        result.vertex.language = ShaderLanguage::Dxbc;
        result.vertex.bytecode = VertexBytes;
        result.vertex.bytecodeSize = sizeof(VertexBytes);
        result.vertex.entryPoint = "vs_main";
        result.vertex.stableId = baseId;
        result.fragment.stage = ShaderStage::Fragment;
        result.fragment.language = ShaderLanguage::Dxbc;
        result.fragment.bytecode = FragmentBytes;
        result.fragment.bytecodeSize = sizeof(FragmentBytes);
        result.fragment.entryPoint = "ps_main";
        result.fragment.stableId = baseId + 1U;
        return result;
    };
    RendererShaderSet shaders;
    shaders.rectangle = pair(100U);
    shaders.image = pair(200U);
    shaders.mesh = pair(300U);
    shaders.glyph = pair(400U);
    shaders.colorFormat = GraphicsTextureFormat::Rgba8Unorm;
    return shaders;
}

TextureResourceDescriptor MakeTargetDescriptor() noexcept {
    TextureResourceDescriptor descriptor;
    descriptor.width = 64U;
    descriptor.height = 48U;
    descriptor.format = GraphicsTextureFormat::Rgba8Unorm;
    descriptor.usage = TextureUsageBit(TextureUsage::RenderTarget) |
        TextureUsageBit(TextureUsage::CopySource);
    return descriptor;
}

bool TestRendererRecordAndDeviceSubmission() {
    RenderPlan plan;
    CHECK(BuildPlan(plan));

    NullGraphicsBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());
    Result<ResourceHandle> target =
        device.CreateRenderTarget(MakeTargetDescriptor());
    CHECK(target);

    Renderer renderer(device, MakeShaders());
    CHECK(renderer.Initialize());
    RenderTarget frameTarget;
    frameTarget.color = target.Value();
    frameTarget.width = 64U;
    frameTarget.height = 48U;
    frameTarget.format = GraphicsTextureFormat::Rgba8Unorm;

    Result<GraphicsCommandBuffer> first =
        renderer.Record(plan, frameTarget);
    CHECK(first);
    CHECK(first.Value().CommandCount() > 0U);
    const std::uint64_t hash = first.Value().StableHash();
    Result<GraphicsCommandBuffer> second =
        renderer.Record(plan, frameTarget);
    CHECK(second && second.Value().StableHash() == hash);

    Result<FenceValue> fence = device.Submit(first.Value());
    CHECK(fence && fence.Value() == 1U);
    CHECK(backend.SubmissionCount() == 1U);
    CHECK(backend.LastGraphicsHash() == hash);
    CHECK(device.LastCapture().commandHash == hash);
    CHECK(renderer.LastStatistics().renderPassCount == 1U);
    CHECK(renderer.LastStatistics().drawCallCount == 1U);

    renderer.Shutdown();
    CHECK(device.DestroyResource(target.Value(), fence.Value()));
    backend.CompleteThrough(fence.Value());
    CHECK(device.CollectGarbage().Value() > 0U);
    return true;
}

bool TestResourceGenerationsAndDeferredDestroy() {
    NullGraphicsBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());

    BufferDescriptor buffer;
    buffer.sizeBytes = 4096U;
    buffer.usage = BufferUsage::Vertex;
    Result<ResourceHandle> first = device.CreateBuffer(buffer);
    CHECK(first);
    CHECK(device.IsAlive(first.Value()));
    CHECK(device.LiveResourceCount() == 1U);
    CHECK(backend.LiveBackendResourceCount() == 1U);

    CHECK(device.DestroyResource(first.Value(), 0U));
    CHECK(!device.IsAlive(first.Value()));
    CHECK(device.PendingDestroyCount() == 1U);
    CHECK(device.CollectGarbage().Value() == 1U);
    CHECK(backend.LiveBackendResourceCount() == 0U);

    Result<ResourceHandle> second = device.CreateBuffer(buffer);
    CHECK(second);
    CHECK(second.Value().index == first.Value().index);
    CHECK(second.Value().generation > first.Value().generation);
    CHECK(!device.IsAlive(first.Value()));
    CHECK(device.IsAlive(second.Value()));
    CHECK(!device.DestroyResource(first.Value(), 0U));

    TextureResourceDescriptor invalidTexture;
    invalidTexture.width = 0U;
    invalidTexture.height = 32U;
    CHECK(!device.CreateTexture(invalidTexture));

    CHECK(device.DestroyResource(second.Value(), 0U));
    CHECK(device.CollectGarbage());
    return true;
}

bool TestFenceRetirementAndDeviceLoss() {
    NullGraphicsBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());

    TextureResourceDescriptor texture;
    texture.width = 64U;
    texture.height = 64U;
    Result<ResourceHandle> handle = device.CreateTexture(texture);
    CHECK(handle);

    GraphicsCommandEncoder encoder;
    Result<GraphicsCommandBuffer> commands = encoder.Finish();
    CHECK(commands);
    Result<FenceValue> fence = device.Submit(commands.Value());
    CHECK(fence && fence.Value() == 1U);
    CHECK(device.DestroyResource(handle.Value(), fence.Value()));
    CHECK(device.CollectGarbage().Value() == 0U);
    backend.CompleteThrough(fence.Value());
    CHECK(device.CollectGarbage().Value() == 1U);

    backend.SimulateDeviceLoss();
    CHECK(!device.Submit(commands.Value()));
    return true;
}

bool TestUnifiedSubmissionTimeline() {
    NullGraphicsBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());
    GraphicsCommandEncoder encoder;
    Result<GraphicsCommandBuffer> commands = encoder.Finish();
    CHECK(commands);

    Result<FenceValue> first = device.Submit(commands.Value());
    Result<FenceValue> second = device.Submit(commands.Value());
    CHECK(first && first.Value() == 1U);
    CHECK(second && second.Value() == 2U);
    CHECK(backend.LastSubmittedFence() == 2U);
    CHECK(backend.SubmissionCount() == 2U);
    return true;
}

} // namespace

int main() {
    if (!TestRendererRecordAndDeviceSubmission()) return 1;
    if (!TestResourceGenerationsAndDeferredDestroy()) return 1;
    if (!TestFenceRetirementAndDeviceLoss()) return 1;
    if (!TestUnifiedSubmissionTimeline()) return 1;
    return 0;
}
