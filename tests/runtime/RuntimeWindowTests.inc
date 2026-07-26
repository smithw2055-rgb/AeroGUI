namespace {

bool RunRuntimeWindowTests() {
    using namespace Aero::Base;
    using namespace Aero::Controls;
    using namespace Aero::Markup;
    using namespace Aero::Presentation;

    RuntimeHost runtime;
    CHECK(runtime.Initialize());

    Result<Ref<Border>> made = MakeRef<Border>();
    CHECK(made);
    Ref<Border> border = std::move(made).Value();
    CHECK(border->SetBackground(
        {0.1F, 0.2F, 0.3F, 1.0F}));
    Ref<Object> root(std::move(border));
    CHECK(runtime.Mount(root, {320.0, 200.0}));

    Result<RuntimeFrameResult> first = runtime.RunFrame();
    CHECK(first);
    const RenderPlan& firstPlan =
        runtime.Renderer()->CurrentPlan();
    CHECK(!firstPlan.Nodes().Empty());
    const std::uint64_t firstVersion = firstPlan.Version();
    CHECK(firstPlan.Nodes()[0U].layoutSlot.width == 320.0);
    CHECK(firstPlan.Nodes()[0U].layoutSlot.height == 200.0);

    CHECK(runtime.Resize({640.0, 360.0}));
    Result<RuntimeFrameResult> second = runtime.RunFrame();
    CHECK(second);
    CHECK(second.Value().frameNumber == 2U);
    CHECK(second.Value().layout.arrangedCount >= 1U);
    const RenderPlan& secondPlan =
        runtime.Renderer()->CurrentPlan();
    CHECK(secondPlan.Version() > firstVersion);
    CHECK(!secondPlan.Nodes().Empty());
    CHECK(secondPlan.Nodes()[0U].layoutSlot.width == 640.0);
    CHECK(secondPlan.Nodes()[0U].layoutSlot.height == 360.0);

    CHECK(!runtime.Resize({-1.0, 100.0}));
    CHECK(runtime.Unmount());
    runtime.Shutdown();
    return true;
}

} // namespace
