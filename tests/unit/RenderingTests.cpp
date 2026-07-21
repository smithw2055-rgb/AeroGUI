#include <Aero/Core/Rendering.hpp>

#include <algorithm>
#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;

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
    RenderBox(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId type, Size desired, Color color) noexcept
        : RenderElement(dispatcher, registry, type), desired_(desired), color_(color) {}

    void SetColor(Color value) noexcept { color_ = value; }

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        return Size{std::min(desired_.width, available.width),
            std::min(desired_.height, available.height)};
    }
    Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        return finalSize;
    }
    Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override {
        Result<void> result = builder.PushOpacity(0.75);
        if (!result) return result;
        result = builder.PushClip({0.0, 0.0, RenderSize().width, RenderSize().height});
        if (!result) return result;
        result = builder.FillRect(
            {0.0, 0.0, RenderSize().width, RenderSize().height}, color_);
        if (!result) return result;
        result = builder.PopClip();
        if (!result) return result;
        return builder.PopOpacity();
    }

private:
    Size desired_;
    Color color_;
};

class RenderPanel final : public RenderElement {
public:
    RenderPanel(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId type) noexcept : RenderElement(dispatcher, registry, type) {}

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        double width = 0.0;
        double height = 0.0;
        for (LayoutElement* child : LayoutChildren()) {
            Result<void> measured = MeasureChild(*child, available);
            if (!measured) return measured.GetStatus();
            width = std::max(width, child->DesiredSize().width);
            height += child->DesiredSize().height;
        }
        return Size{std::min(width, available.width),
            std::min(height, available.height)};
    }
    Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        double y = 0.0;
        for (LayoutElement* child : LayoutChildren()) {
            const double height = child->DesiredSize().height;
            Result<void> arranged = ArrangeChild(
                *child, {0.0, y, finalSize.width, height});
            if (!arranged) return arranged.GetStatus();
            y += height;
        }
        return finalSize;
    }
    Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override {
        return builder.StrokeRect(
            {0.0, 0.0, RenderSize().width, RenderSize().height},
            {1.0F, 1.0F, 1.0F, 1.0F}, 1.0);
    }
};

struct Fixture final {
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    Dispatcher dispatcher;
    TypeId objectType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    TypeId panelType = InvalidTypeId;

    bool Build() {
        const StringView ns("urn:aero");
        objectType = MakeTypeId(ns, StringView("Object"));
        elementType = MakeTypeId(ns, StringView("RenderElement"));
        panelType = MakeTypeId(ns, StringView("RenderPanel"));
        CHECK(types.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("RenderElement"), objectType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("RenderPanel"), elementType,
            TypeFlags::None, nullptr}));
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        return true;
    }
};

bool TestDisplayListValidation() {
    DisplayListBuilder builder;
    CHECK(builder.PushTransform({1.0, 0.0, 0.0, 1.0, 4.0, 5.0}));
    CHECK(builder.PushClip({0.0, 0.0, 20.0, 10.0}));
    CHECK(builder.FillRect({0.0, 0.0, 20.0, 10.0},
        {0.25F, 0.5F, 0.75F, 1.0F}));
    CHECK(builder.PopClip());
    CHECK(builder.PopTransform());
    Result<DisplayList> list = builder.Finish();
    CHECK(list);
    CHECK(list.Value().CommandCount() == 5U);
    CHECK(list.Value().StableHash() != 0U);

    DisplayListBuilder unbalanced;
    CHECK(unbalanced.PushOpacity(0.5));
    Result<DisplayList> invalid = unbalanced.Finish();
    CHECK(!invalid && invalid.GetStatus().code == ErrorCode::InvalidState);

    DisplayListBuilder badColor;
    Result<void> invalidColor = badColor.FillRect(
        {0.0, 0.0, 1.0, 1.0}, {2.0F, 0.0F, 0.0F, 1.0F});
    CHECK(!invalidColor && invalidColor.GetStatus().code == ErrorCode::InvalidArgument);
    return true;
}

bool TestRenderCommitAndInvalidation() {
    Fixture fixture;
    CHECK(fixture.Build());
    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());
    NullRenderBackend backend;
    RenderManager renderer(fixture.dispatcher, backend);
    CHECK(renderer.Initialize());

    RenderPanel root(fixture.dispatcher, fixture.properties, fixture.panelType);
    RenderBox first(fixture.dispatcher, fixture.properties, fixture.elementType,
        {30.0, 10.0}, {1.0F, 0.0F, 0.0F, 1.0F});
    RenderBox second(fixture.dispatcher, fixture.properties, fixture.elementType,
        {40.0, 15.0}, {0.0F, 0.0F, 1.0F, 1.0F});

    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, first));
    CHECK(tree.AttachLogical(root, second));
    CHECK(tree.AttachVisual(root, first));
    CHECK(tree.AttachVisual(root, second));
    CHECK(layout.Attach(root, first));
    CHECK(layout.Attach(root, second));
    CHECK(layout.SetRoot(&root, {100.0, 80.0}));
    CHECK(renderer.SetRoot(&root));
    CHECK(renderer.Attach(root, first));
    CHECK(renderer.Attach(root, second));

    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(backend.SubmissionCount() == 1U);
    CHECK(renderer.CurrentPlan().Nodes().Size() == 3U);
    CHECK(renderer.CurrentPlan().Commands().Size() == 11U);
    const std::uint64_t firstHash = renderer.CurrentPlan().StableHash();
    CHECK(firstHash == backend.LastHash());
    CHECK(renderer.Diagnostics().dirtyCount == 0U);

    first.SetColor({0.0F, 1.0F, 0.0F, 1.0F});
    CHECK(first.InvalidateRender());
    CHECK(!first.IsRenderValid() && !root.IsRenderValid());
    CHECK(renderer.Diagnostics().dirtyCount == 2U);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(backend.SubmissionCount() == 2U);
    CHECK(renderer.CurrentPlan().StableHash() != firstHash);
    CHECK(renderer.CurrentPlan().Version() == 2U);

    CHECK(renderer.SetRoot(nullptr));
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    CHECK(layout.Detach(root, second));
    CHECK(layout.Detach(root, first));
    CHECK(tree.DetachVisual(root, second));
    CHECK(tree.DetachVisual(root, first));
    CHECK(tree.DetachLogical(root, second));
    CHECK(tree.DetachLogical(root, first));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(first));
    CHECK(values.DetachObject(second));
    CHECK(values.DetachObject(root));
    return true;
}

bool TestRenderRequiresArrange() {
    Fixture fixture;
    CHECK(fixture.Build());
    NullRenderBackend backend;
    RenderManager renderer(fixture.dispatcher, backend);
    CHECK(renderer.Initialize());
    RenderPanel root(fixture.dispatcher, fixture.properties, fixture.panelType);
    CHECK(renderer.SetRoot(&root));
    Result<std::uint32_t> commit = renderer.Commit();
    CHECK(!commit && commit.GetStatus().code == ErrorCode::InvalidState);
    CHECK(renderer.SetRoot(nullptr));
    return true;
}

} // namespace

int main() {
    if (!TestDisplayListValidation()) return 1;
    if (!TestRenderCommitAndInvalidation()) return 1;
    if (!TestRenderRequiresArrange()) return 1;
    std::puts("Aero rendering tests passed");
    return 0;
}
