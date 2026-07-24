#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Render/Renderer.hpp>
#include <Aero/Rhi/Graphics.hpp>

#include <algorithm>
#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Render;
using namespace Aero::Rhi;

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

    Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override {
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
        CHECK(typesRegistration.TryRegisterType(
            TypeRegistration::Object(ns, StringView("Object"),
                InvalidTypeId, TypeFlags::None, nullptr)));
        CHECK(typesRegistration.TryRegisterType(
            TypeRegistration::Object(ns, StringView("FrameworkElement"),
                objectType, TypeFlags::None, nullptr)));
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
    NullRenderBackend renderBackend;
    RenderManager manager(fixture.dispatcher, renderBackend);
    CHECK(manager.Initialize());

    RenderBox root(fixture.elementType);
    CHECK(tree.SetRoot(&root));
    CHECK(layout.SetRoot(&root, {64.0, 48.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(manager.SetRoot(&root));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::RenderCommit));
    destination = manager.CurrentPlan();

    CHECK(manager.SetRoot(nullptr));
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(root));
    return true;
}

ShaderDescriptor MakeShader(
    ShaderStage stage,
    std::uint64_t stableId) noexcept {
    static const std::uint8_t Bytecode[] = {1U, 2U, 3U, 4U};
    ShaderDescriptor shader;
    shader.stage = stage;
    shader.language = ShaderLanguage::SpirV;
    shader.bytecode = Bytecode;
    shader.bytecodeSize = sizeof(Bytecode);
    shader.entryPoint = stage == ShaderStage::Vertex
        ? StringView("vs_main")
        : StringView("fs_main");
    shader.stableId = stableId;
    return shader;
}

RendererShaderSet MakeShaders() noexcept {
    RendererShaderSet shaders;
    shaders.rectangleVertex = MakeShader(ShaderStage::Vertex, 1U);
    shaders.rectangleFragment = MakeShader(ShaderStage::Fragment, 2U);
    shaders.imageVertex = MakeShader(ShaderStage::Vertex, 3U);
    shaders.imageFragment = MakeShader(ShaderStage::Fragment, 4U);
    shaders.meshVertex = MakeShader(ShaderStage::Vertex, 5U);
    shaders.meshFragment = MakeShader(ShaderStage::Fragment, 6U);
    shaders.glyphVertex = MakeShader(ShaderStage::Vertex, 7U);
    shaders.glyphFragment = MakeShader(ShaderStage::Fragment, 8U);
    shaders.colorFormat = GraphicsTextureFormat::Rgba8Unorm;
    return shaders;
}

bool TestRendererRecordingAndSubmission() {
    RenderPlan plan;
    CHECK(BuildPlan(plan));

    NullGraphicsBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());

    TextureResourceDescriptor targetDescriptor;
    targetDescriptor.width = 64U;
    targetDescriptor.height = 48U;
    targetDescriptor.format = GraphicsTextureFormat::Rgba8Unorm;
    targetDescriptor.usage = TextureUsageBit(TextureUsage::RenderTarget);
    Result<ResourceHandle> target =
        device.CreateRenderTarget(targetDescriptor);
    CHECK(target);

    Renderer renderer(device, MakeShaders());
    CHECK(renderer.Initialize());
    Result<CommandList> first = renderer.Record(
        plan, {target.Value(), 64U, 48U});
    CHECK(first);
    CHECK(first.Value().CommandCount() > plan.Commands().Size());
    const std::uint64_t hash = first.Value().StableHash();

    Result<CommandList> second = renderer.Record(
        plan, {target.Value(), 64U, 48U});
    CHECK(second);
    CHECK(second.Value().StableHash() == hash);

    Result<FenceValue> fence = device.Submit(first.Value());
    CHECK(fence && fence.Value() == 1U);
    CHECK(backend.SubmissionCount() == 1U);
    CHECK(backend.LastGraphicsHash() == hash);
    CHECK(renderer.LastStatistics().drawCallCount == 1U);
    CHECK(renderer.LastStatistics().rectangleInstanceCount == 1U);

    renderer.Shutdown();
    CHECK(device.DestroyResource(target.Value(), fence.Value()));
    backend.CompleteThrough(fence.Value());
    CHECK(device.CollectGarbage());
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

    CHECK(device.DestroyResource(first.Value()));
    CHECK(!device.IsAlive(first.Value()));
    CHECK(device.PendingDestroyCount() == 1U);
    Result<std::uint32_t> collected = device.CollectGarbage();
    CHECK(collected && collected.Value() == 1U);

    Result<ResourceHandle> second = device.CreateBuffer(buffer);
    CHECK(second);
    CHECK(second.Value().index == first.Value().index);
    CHECK(second.Value().generation > first.Value().generation);
    CHECK(!device.IsAlive(first.Value()));
    CHECK(device.IsAlive(second.Value()));
    CHECK(!device.DestroyResource(first.Value()));

    TextureResourceDescriptor invalidTexture;
    invalidTexture.width = 0U;
    invalidTexture.height = 32U;
    CHECK(!device.CreateTexture(invalidTexture));

    CHECK(device.DestroyResource(second.Value()));
    CHECK(device.CollectGarbage());
    return true;
}

bool TestFenceRetirementAndDeviceLoss() {
    NullGraphicsBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());

    BufferDescriptor buffer;
    buffer.sizeBytes = 64U;
    buffer.usage = BufferUsage::Vertex;
    Result<ResourceHandle> handle = device.CreateBuffer(buffer);
    CHECK(handle);

    CommandEncoder encoder;
    Result<CommandList> commands = encoder.Finish();
    CHECK(commands);
    Result<FenceValue> fence = device.Submit(commands.Value());
    CHECK(fence && fence.Value() == 1U);
    CHECK(device.DestroyResource(handle.Value(), fence.Value()));
    CHECK(device.CollectGarbage().Value() == 0U);
    backend.CompleteThrough(fence.Value());
    CHECK(device.CollectGarbage().Value() == 1U);

    backend.SimulateDeviceLoss();
    Result<FenceValue> lost = device.Submit(commands.Value());
    CHECK(!lost);
    CHECK(lost.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

bool TestSingleSubmissionTimeline() {
    NullGraphicsBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());
    CommandEncoder encoder;
    Result<CommandList> commands = encoder.Finish();
    CHECK(commands);

    for (FenceValue expected = 1U; expected <= 3U; ++expected) {
        Result<FenceValue> submitted = device.Submit(commands.Value());
        CHECK(submitted && submitted.Value() == expected);
    }
    CHECK(backend.LastSubmittedFence() == 3U);
    CHECK(backend.SubmissionCount() == 3U);
    return true;
}

} // namespace

int main() {
    if (!TestRendererRecordingAndSubmission()) return 1;
    if (!TestResourceGenerationsAndDeferredDestroy()) return 1;
    if (!TestFenceRetirementAndDeviceLoss()) return 1;
    if (!TestSingleSubmissionTimeline()) return 1;
    std::puts("Aero RHI tests passed");
    return 0;
}
