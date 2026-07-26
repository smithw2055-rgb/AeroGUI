#include <Aero/Diagnostics/ProductionDiagnostics.hpp>
#include <Aero/RuntimeServices.hpp>
#include <Aero/Render/ProductionRendering.hpp>
#include <Aero/Rhi/BackendLifecycle.hpp>
#include <Aero/Text/UnicodeRuntime.hpp>

namespace {

using Aero::Base::ErrorCode;
using Aero::Base::MakeRef;
using Aero::Base::Object;
using Aero::Base::Ref;
using Aero::Base::Result;
using Aero::Base::Span;
using Aero::Base::Vector;
using Aero::Controls::Border;
using Aero::Controls::StackPanel;
using Aero::Diagnostics::AccessibilityAction;
using Aero::Diagnostics::AccessibilityActionBit;
using Aero::Diagnostics::AccessibilityNode;
using Aero::Diagnostics::AccessibilityRole;
using Aero::Diagnostics::AccessibilityTree;
using Aero::Diagnostics::CapabilityManifest;
using Aero::Diagnostics::FuzzHarness;
using Aero::Diagnostics::PerformanceGate;
using Aero::Diagnostics::PerformanceMetric;
using Aero::Diagnostics::RuntimeInspectorSnapshot;
using Aero::Diagnostics::StabilityCounter;
using Aero::MountEdgeState;
using Aero::MountTransactionService;
using Aero::RuntimeHost;
using Aero::RuntimeObjectStateStore;
using Aero::Presentation::RenderPlan;
using Aero::Render::AnimationClock;
using Aero::Render::AnimationFillMode;
using Aero::Render::FrozenRenderPacket;
using Aero::Render::ProductionPassDescriptor;
using Aero::Render::ProductionRenderGraph;
using Aero::Render::RenderCacheBudgetManager;
using Aero::Render::RenderCacheKind;
using Aero::Render::RenderEffectKind;
using Aero::Render::RenderMaskKind;
using Aero::Render::ScalarAnimation;
using Aero::Rhi::AndroidLifecycleHost;
using Aero::Rhi::BackendLifecycleState;
using Aero::Rhi::BackendOwnership;
using Aero::Rhi::D3D12BackendContract;
using Aero::Rhi::MetalBackendContract;
using Aero::Rhi::PortableResourceState;
using Aero::Rhi::VulkanBackendContract;
using Aero::Rhi::WebGl2RuntimeContract;
using Aero::Text::ParagraphDirection;
using Aero::Text::UnicodeScript;
using Aero::Text::UnicodeTextAnalysis;

bool TestUnifiedMountTransactionAndSidecars() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());

    Result<Ref<StackPanel>> parentMade = MakeRef<StackPanel>();
    Result<Ref<Border>> childMade = MakeRef<Border>();
    CHECK(parentMade);
    CHECK(childMade);
    Ref<StackPanel> parent = std::move(parentMade).Value();
    Ref<Border> child = std::move(childMade).Value();
    Ref<Object> root(parent);
    CHECK(runtime.Mount(root, {400.0, 240.0}));

    MountTransactionService mounts(
        *runtime.Tree(), *runtime.Layout(), runtime.Renderer());
    Result<MountEdgeState> attached = mounts.Attach(*parent, *child);
    CHECK(attached);
    CHECK(attached.Value().logicalAttached);
    CHECK(attached.Value().visualAttached);
    CHECK(attached.Value().layoutAttached);
    CHECK(attached.Value().renderAttached);
    CHECK(child->LogicalParent() == parent.Get());
    CHECK(child->VisualParent() == parent.Get());

    RuntimeObjectStateStore sidecars;
    Result<Aero::RuntimeObjectState*> state =
        sidecars.Ensure(attached.Value().child);
    CHECK(state);
    state.Value()->measureQueued = true;
    state.Value()->renderQueued = true;
    CHECK(sidecars.Size() == 1U);
    CHECK(sidecars.Find(attached.Value().child) != nullptr);

    CHECK(runtime.RunFrame());
    MountEdgeState edge = attached.Value();
    CHECK(mounts.Detach(*parent, *child, &edge));
    CHECK(child->LogicalParent() == nullptr);
    CHECK(child->VisualParent() == nullptr);
    CHECK(sidecars.Prune(*runtime.Tree()) == 1U);
    CHECK(sidecars.Size() == 0U);

    CHECK(runtime.Unmount());
    runtime.Shutdown();
    return true;
}

bool TestUnicodeLineBreakBidiAndClusters() {
    UnicodeTextAnalysis mixed;
    CHECK(mixed.Analyze(
        u8"ABC العربية 中文",
        ParagraphDirection::Auto));
    CHECK(mixed.Scalars().Size() > 10U);
    CHECK(mixed.LogicalRuns().Size() >= 3U);
    CHECK(mixed.ParagraphLevel() == 0U);
    CHECK(mixed.BreakOpportunities().Size() ==
        mixed.Scalars().Size() + 1U);

    bool hasArabicRtl = false;
    bool hasHan = false;
    bool hasInteriorBreak = false;
    for (const auto& run : mixed.LogicalRuns()) {
        if (run.script == UnicodeScript::Arabic && run.rightToLeft) {
            hasArabicRtl = true;
        }
        if (run.script == UnicodeScript::Han) hasHan = true;
    }
    for (const auto& opportunity : mixed.BreakOpportunities()) {
        if (opportunity.allowed && !opportunity.mandatory &&
            opportunity.scalarIndex != 0U) {
            hasInteriorBreak = true;
        }
    }
    CHECK(hasArabicRtl);
    CHECK(hasHan);
    CHECK(hasInteriorBreak);

    UnicodeTextAnalysis rtl;
    CHECK(rtl.Analyze(u8"العربية ABC", ParagraphDirection::Auto));
    CHECK(rtl.ParagraphLevel() == 1U);
    CHECK(rtl.VisualScalarOrder().Size() == rtl.Scalars().Size());
    CHECK(rtl.LogicalScalarFromVisual(0U) != UINT32_MAX);
    CHECK(rtl.VisualScalarFromLogical(0U) != UINT32_MAX);

    UnicodeTextAnalysis combining;
    CHECK(combining.Analyze(u8"Á"));
    CHECK(combining.Scalars().Size() == 2U);
    CHECK(combining.ClusterCount() == 1U);
    CHECK(combining.ClusterMap().Size() == 1U);
    return true;
}

bool TestAndroidGlesLifecycle() {
    AndroidLifecycleHost host;
    CHECK(host.Create(1280U, 720U));
    CHECK(host.Lifecycle().State() == BackendLifecycleState::Ready);
    const std::uint64_t generation =
        host.Lifecycle().Statistics().generation;
    CHECK(host.Pause());
    CHECK(host.Lifecycle().State() == BackendLifecycleState::Suspended);
    CHECK(host.Resume());
    CHECK(host.Lifecycle().Statistics().generation > generation);
    CHECK(host.LoseSurface());
    CHECK(host.Lifecycle().State() == BackendLifecycleState::SurfaceLost);
    CHECK(host.RestoreSurface(720U, 1280U));
    CHECK(host.Lifecycle().State() == BackendLifecycleState::Ready);
    CHECK(host.Lifecycle().Statistics().width == 720U);
    CHECK(host.Lifecycle().Statistics().height == 1280U);
    return true;
}

bool TestVulkanResourceAndFenceContract() {
    VulkanBackendContract vulkan;
    CHECK(vulkan.Initialize(
        BackendOwnership::Borrowed, 800U, 600U));
    CHECK(vulkan.States().Register(42U));
    CHECK(vulkan.States().Transition(
        42U,
        PortableResourceState::Undefined,
        PortableResourceState::CopyDestination));
    CHECK(vulkan.States().Transition(
        42U,
        PortableResourceState::CopyDestination,
        PortableResourceState::ShaderRead));
    Result<Aero::Rhi::FenceValue> submitted = vulkan.Submit();
    CHECK(submitted);
    CHECK(vulkan.Retirement().Retire(42U, submitted.Value()));
    CHECK(vulkan.Collect() == 0U);
    vulkan.Complete(submitted.Value());
    CHECK(vulkan.Collect() == 1U);
    CHECK(vulkan.Retirement().Pending() == 0U);
    return true;
}

bool TestD3D12AndMetalContracts() {
    D3D12BackendContract d3d12;
    CHECK(d3d12.Initialize(
        BackendOwnership::Owned, 8U, 1920U, 1080U));
    Result<std::uint32_t> first = d3d12.AllocateDescriptors(3U);
    CHECK(first);
    CHECK(first.Value() == 0U);
    CHECK(d3d12.DescriptorUsed() == 3U);
    CHECK(!d3d12.AllocateDescriptors(6U));
    d3d12.ResetDescriptorHeap();
    CHECK(d3d12.DescriptorUsed() == 0U);

    MetalBackendContract metal;
    CHECK(metal.Initialize(
        BackendOwnership::Borrowed, 1170U, 2532U));
    CHECK(metal.BeginCommandBuffer());
    CHECK(metal.CommitCommandBuffer());
    CHECK(metal.SetBackgrounded(true));
    CHECK(!metal.BeginCommandBuffer());
    CHECK(metal.SetBackgrounded(false));
    CHECK(metal.BeginCommandBuffer());
    CHECK(metal.CommitCommandBuffer());
    return true;
}

bool TestWebGl2ContextAndRafContract() {
    WebGl2RuntimeContract rejected;
    CHECK(!rejected.Initialize(1U, 1.0, 640U, 480U));

    WebGl2RuntimeContract web;
    CHECK(web.Initialize(2U, 2.0, 640U, 360U));
    CHECK(web.PixelWidth() == 1280U);
    CHECK(web.PixelHeight() == 720U);
    Result<std::uint64_t> raf = web.RequestAnimationFrame();
    CHECK(raf);
    CHECK(!web.RequestAnimationFrame());
    CHECK(web.CompleteAnimationFrame(raf.Value()));
    const std::uint64_t generation = web.ResourceGeneration();
    CHECK(web.LoseContext());
    CHECK(web.Lifecycle().State() == BackendLifecycleState::SurfaceLost);
    CHECK(web.RestoreContext());
    CHECK(web.ResourceGeneration() > generation);
    CHECK(web.Lifecycle().State() == BackendLifecycleState::Ready);
    return true;
}

bool TestProductionRenderPacketGraphCacheAndAnimation() {
    RenderPlan plan;
    FrozenRenderPacket packet;
    CHECK(packet.Freeze(plan, 7U));
    CHECK(packet.IsFrozen());
    CHECK(packet.PlanHash() == plan.StableHash());
    CHECK(packet.ResourceGeneration() == 7U);

    ProductionRenderGraph graph;
    ProductionPassDescriptor root;
    root.id = 1U;
    root.bounds = {0.0, 0.0, 800.0, 600.0};
    CHECK(graph.TryAddPass(root));
    ProductionPassDescriptor effect;
    effect.id = 2U;
    effect.parent = 1U;
    effect.bounds = {20.0, 20.0, 200.0, 120.0};
    effect.effect = RenderEffectKind::Blur;
    effect.mask = RenderMaskKind::RoundedRectangle;
    effect.offscreen = true;
    CHECK(graph.TryAddPass(effect));
    CHECK(graph.OffscreenPassCount() == 1U);

    RenderCacheBudgetManager cache;
    CHECK(cache.SetBudget(100U));
    CHECK(cache.InsertOrTouch(
        1U, 70U, RenderCacheKind::Image, 1U));
    CHECK(cache.InsertOrTouch(
        2U, 50U, RenderCacheKind::Geometry, 2U));
    Vector<std::uint64_t> evicted;
    Result<std::uint32_t> trimmed = cache.Trim(0U, &evicted);
    CHECK(trimmed);
    CHECK(trimmed.Value() == 1U);
    CHECK(evicted.Size() == 1U);
    CHECK(evicted[0] == 1U);
    CHECK(cache.Statistics().usedBytes == 50U);

    AnimationClock animation;
    ScalarAnimation descriptor;
    descriptor.from = 0.0;
    descriptor.to = 10.0;
    descriptor.durationMicroseconds = 1000U;
    descriptor.repeatCount = 1U;
    descriptor.fillMode = AnimationFillMode::HoldEnd;
    CHECK(animation.Start(descriptor, 100U));
    const auto halfway = animation.Sample(600U);
    CHECK(halfway.active);
    CHECK(halfway.value == 5.0);
    const auto completed = animation.Sample(1200U);
    CHECK(completed.completed);
    CHECK(completed.value == 10.0);
    return true;
}

Result<void> AcceptFuzzCase(
    Span<const std::uint8_t> input,
    void*) noexcept {
    return input.Size() != 0U && input[0] == 0xFFU
        ? Result<void>(Aero::Base::Status::Failure(
            ErrorCode::ValidationFailed,
            "Expected synthetic fuzz rejection"))
        : Result<void>();
}

bool TestAccessibilityInspectorAndReleaseGates() {
    AccessibilityTree accessibility;
    AccessibilityNode window;
    window.id = 1U;
    window.role = AccessibilityRole::Window;
    window.bounds = {0.0, 0.0, 800.0, 600.0};
    CHECK(window.name.TryAssign("ControlGallery"));
    CHECK(accessibility.TryAdd(std::move(window)));

    AccessibilityNode button;
    button.id = 2U;
    button.parent = 1U;
    button.role = AccessibilityRole::Button;
    button.actions = AccessibilityActionBit(AccessibilityAction::Invoke) |
        AccessibilityActionBit(AccessibilityAction::Focus);
    button.bounds = {20.0, 20.0, 120.0, 40.0};
    button.focusable = true;
    CHECK(button.name.TryAssign("Run"));
    CHECK(accessibility.TryAdd(std::move(button)));
    CHECK(accessibility.Nodes().Size() == 2U);

    RuntimeHost runtime;
    CHECK(runtime.Initialize());
    Result<Ref<Border>> rootMade = MakeRef<Border>();
    CHECK(rootMade);
    Ref<Border> border = std::move(rootMade).Value();
    Ref<Object> root(border);
    CHECK(runtime.Mount(root, {320.0, 200.0}));
    CHECK(runtime.RunFrame());
    RuntimeInspectorSnapshot inspector;
    CHECK(inspector.Capture(
        *runtime.Tree(),
        &runtime.Renderer()->CurrentPlan()));
    CHECK(inspector.Nodes().Size() == 1U);
    CHECK(inspector.Render().nodeCount >= 1U);
    CHECK(runtime.Unmount());
    runtime.Shutdown();

    PerformanceGate performance;
    CHECK(performance.SetBudget(
        PerformanceMetric::LayoutMicroseconds, 1000.0));
    CHECK(performance.Record(
        PerformanceMetric::LayoutMicroseconds, 750.0));
    Result<Aero::Diagnostics::PerformanceGateResult> gate =
        performance.Evaluate();
    CHECK(gate);
    CHECK(gate.Value().failed == 0U);
    CHECK(gate.Value().worstRatio == 0.75);

    FuzzHarness fuzz;
    CHECK(fuzz.TryAddTarget("decoder", &AcceptFuzzCase));
    const std::uint8_t goodBytes[] = {0x41U, 0x42U};
    const std::uint8_t badBytes[] = {0xFFU};
    const Span<const std::uint8_t> cases[] = {
        {goodBytes, 2U},
        {badBytes, 1U}};
    const auto fuzzResult = fuzz.Run({cases, 2U});
    CHECK(fuzzResult.targetCount == 1U);
    CHECK(fuzzResult.caseCount == 2U);
    CHECK(fuzzResult.failureCount == 1U);

    CapabilityManifest capabilities;
    CHECK(!capabilities.ValidateProductionM4());
    capabilities.d3d11 = true;
    capabilities.openGl33 = true;
    capabilities.gles30 = true;
    capabilities.vulkan = true;
    capabilities.d3d12 = true;
    capabilities.metal = true;
    capabilities.webGl2 = true;
    capabilities.accessibility = true;
    capabilities.inspector = true;
    CHECK(capabilities.ValidateProductionM4());

    StabilityCounter stability;
    for (std::uint32_t frame = 0U; frame < 1000U; ++frame) {
        CHECK(stability.RecordFrame(100U + frame % 3U, 4096U));
    }
    CHECK(stability.Frames() == 1000U);
    CHECK(stability.IsStable(102U, 4096U));
    return true;
}

bool RunM1M4ClosureTests() {
    if (!TestUnifiedMountTransactionAndSidecars()) return false;
    if (!TestUnicodeLineBreakBidiAndClusters()) return false;
    if (!TestAndroidGlesLifecycle()) return false;
    if (!TestVulkanResourceAndFenceContract()) return false;
    if (!TestD3D12AndMetalContracts()) return false;
    if (!TestWebGl2ContextAndRafContract()) return false;
    if (!TestProductionRenderPacketGraphCacheAndAnimation()) return false;
    if (!TestAccessibilityInspectorAndReleaseGates()) return false;
    return true;
}

} // namespace
