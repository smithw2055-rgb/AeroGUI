#include <Aero/Rhi/Graphics.hpp>
#include <Aero/Render/RenderPlanTranslator.hpp>
#include <Aero/Core/Presentation.hpp>

#include <algorithm>
#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
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

class RenderBox final : public RenderElement {
public:
    explicit RenderBox(TypeId type) noexcept
        : RenderElement(type) {}

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
    DependencyPropertyRegistry properties{types};
    Dispatcher dispatcher;
    PresentationContextScope presentation{dispatcher, properties};
    TypeId objectType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;

    bool Build() {
        const StringView ns("urn:aero");
        objectType = MakeTypeId(ns, StringView("Object"));
        elementType = MakeTypeId(ns, StringView("RenderElement"));
        CHECK(types.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("RenderElement"), objectType,
            TypeFlags::None, nullptr}));
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
    RenderManager renderer(fixture.dispatcher, renderBackend);
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

bool TestUploadArena() {
    UploadArena arena(128U);
    CHECK(arena.Initialize());
    Result<UploadSlice> first = arena.Allocate(12U, 16U);
    CHECK(first);
    CHECK(first.Value().offset == 0U);
    Result<UploadSlice> second = arena.Allocate(8U, 32U);
    CHECK(second);
    CHECK(second.Value().offset == 32U);
    CHECK(arena.Used() == 40U);
    Result<UploadSlice> overflow = arena.Allocate(100U, 8U);
    CHECK(!overflow);
    CHECK(overflow.GetStatus().code == ErrorCode::OutOfMemory);
    arena.Reset();
    CHECK(arena.Used() == 0U);
    return true;
}

bool TestTranslationAndSubmission() {
    RenderPlan plan;
    CHECK(BuildPlan(plan));
    RenderPlanTranslator translator;
    Result<CommandBuffer> translated = translator.Translate(plan);
    CHECK(translated);
    CHECK(translated.Value().CommandCount() == plan.Commands().Size() + 2U);
    const std::uint64_t hash = translated.Value().StableHash();
    Result<CommandBuffer> translatedAgain = translator.Translate(plan);
    CHECK(translatedAgain);
    CHECK(translatedAgain.Value().StableHash() == hash);

    NullRhiBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());
    Result<FrameContext> frame = device.BeginFrame();
    CHECK(frame);
    Result<UploadSlice> upload = frame.Value().uploadArena->Allocate(64U, 16U);
    CHECK(upload);
    Result<FenceValue> fence = device.Submit(frame.Value(), translated.Value());
    CHECK(fence);
    CHECK(fence.Value() == 1U);
    CHECK(backend.SubmissionCount() == 1U);
    CHECK(backend.LastCommandHash() == hash);
    CHECK(!device.Submit(frame.Value(), translated.Value()));
    return true;
}

bool TestResourceGenerationsAndDeferredDestroy() {
    NullRhiBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());

    ResourceDescriptor buffer;
    buffer.type = ResourceType::Buffer;
    buffer.buffer.sizeBytes = 4096U;
    buffer.buffer.usage = BufferUsage::Vertex;
    Result<ResourceHandle> first = device.CreateResource(buffer);
    CHECK(first);
    CHECK(device.IsAlive(first.Value()));
    CHECK(device.LiveResourceCount() == 1U);
    CHECK(backend.LiveBackendResourceCount() == 1U);

    CHECK(device.DestroyResource(first.Value(), 0U));
    CHECK(!device.IsAlive(first.Value()));
    CHECK(device.PendingDestroyCount() == 1U);
    CHECK(backend.LiveBackendResourceCount() == 1U);
    Result<std::uint32_t> collected = device.CollectGarbage();
    CHECK(collected && collected.Value() == 1U);
    CHECK(backend.LiveBackendResourceCount() == 0U);

    Result<ResourceHandle> second = device.CreateResource(buffer);
    CHECK(second);
    CHECK(second.Value().index == first.Value().index);
    CHECK(second.Value().generation > first.Value().generation);
    CHECK(!device.IsAlive(first.Value()));
    CHECK(device.IsAlive(second.Value()));
    CHECK(!device.DestroyResource(first.Value(), 0U));

    ResourceDescriptor invalidTexture;
    invalidTexture.type = ResourceType::Texture;
    invalidTexture.texture.width = 0U;
    invalidTexture.texture.height = 32U;
    CHECK(!device.CreateResource(invalidTexture));

    CHECK(device.DestroyResource(second.Value(), 0U));
    CHECK(device.CollectGarbage());
    return true;
}

bool TestFenceRetirementAndDeviceLoss() {
    RenderPlan plan;
    CHECK(BuildPlan(plan));
    RenderPlanTranslator translator;
    Result<CommandBuffer> commands = translator.Translate(plan);
    CHECK(commands);

    NullRhiBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());
    ResourceDescriptor texture;
    texture.type = ResourceType::Texture;
    texture.texture.width = 64U;
    texture.texture.height = 64U;
    Result<ResourceHandle> handle = device.CreateResource(texture);
    CHECK(handle);

    Result<FrameContext> frame = device.BeginFrame();
    CHECK(frame);
    Result<FenceValue> fence = device.Submit(frame.Value(), commands.Value());
    CHECK(fence);
    CHECK(device.DestroyResource(handle.Value(), fence.Value()));
    CHECK(device.CollectGarbage().Value() == 0U);
    CHECK(backend.LiveBackendResourceCount() == 1U);
    backend.CompleteThrough(fence.Value());
    CHECK(device.CollectGarbage().Value() == 1U);
    CHECK(backend.LiveBackendResourceCount() == 0U);

    backend.SimulateDeviceLoss();
    Result<FrameContext> lostFrame = device.BeginFrame();
    CHECK(!lostFrame);
    CHECK(lostFrame.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

bool TestMixedSubmissionFenceTimeline() {
    RenderPlan plan;
    CHECK(BuildPlan(plan));
    RenderPlanTranslator translator;
    Result<CommandBuffer> legacyCommands = translator.Translate(plan);
    CHECK(legacyCommands);
    GraphicsCommandEncoder encoder;
    Result<GraphicsCommandBuffer> graphicsCommands = encoder.Finish();
    CHECK(graphicsCommands);

    NullGraphicsBackend backend;
    RhiDevice device(backend);
    GraphicsQueue graphicsQueue(backend);
    CHECK(device.Initialize());
    CHECK(graphicsQueue.Initialize());

    Result<FenceValue> graphicsFence =
        graphicsQueue.Submit(graphicsCommands.Value());
    CHECK(graphicsFence && graphicsFence.Value() == 1U);
    Result<FrameContext> frame = device.BeginFrame();
    CHECK(frame);
    Result<FenceValue> legacyFence =
        device.Submit(frame.Value(), legacyCommands.Value());
    CHECK(legacyFence && legacyFence.Value() == 2U);
    graphicsFence = graphicsQueue.Submit(graphicsCommands.Value());
    CHECK(graphicsFence && graphicsFence.Value() == 3U);
    CHECK(backend.LastSubmittedFence() == 3U);
    CHECK(backend.SubmissionCount() == 3U);
    return true;
}

} // namespace

int main() {
    if (!TestUploadArena()) return 1;
    if (!TestTranslationAndSubmission()) return 1;
    if (!TestResourceGenerationsAndDeferredDestroy()) return 1;
    if (!TestFenceRetirementAndDeviceLoss()) return 1;
    if (!TestMixedSubmissionFenceTimeline()) return 1;
    std::puts("Aero RHI tests passed");
    return 0;
}
