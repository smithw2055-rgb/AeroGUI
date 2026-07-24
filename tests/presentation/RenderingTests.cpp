#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>

#include <algorithm>
#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Controls;

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
    RenderBox(TypeId type, Size desired, Color color) noexcept
        : FrameworkElement(type), desired_(desired), color_(color) {}

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

class RenderPanel final : public FrameworkElement {
public:
    explicit RenderPanel(TypeId type) noexcept : FrameworkElement(type) {}

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        double width = 0.0;
        double height = 0.0;
        for (UIElement* child : LayoutChildren()) {
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
        for (UIElement* child : LayoutChildren()) {
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

class TestBorder final : public Border {
public:
    explicit TestBorder(TypeId type) noexcept : Border(type) {}
    using Border::BuildDisplayList;
};

struct Fixture final {
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typesBehaviors{types};
    MetadataRegistrationTypes typesRegistration{types, typesBehaviors};
    MetadataValueRegistrationStore valueRegistrations{types};
    DependencyPropertyRegistry properties{types, typesBehaviors};
    Dispatcher dispatcher;
    ObjectServicesScope presentation{dispatcher, properties, valueRegistrations};
    TypeId objectType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    TypeId panelType = InvalidTypeId;
    TypeId textBlockType = InvalidTypeId;
    TypeId borderType = InvalidTypeId;

    bool Build() {
        MetaRegistrationContext registrationContext(
            types, typesBehaviors, valueRegistrations, properties);
        CHECK(Aero::Core::Detail::PopulateCoreMetadata(
            registrationContext));
        CHECK(Aero::Presentation::Detail::PopulatePresentationMetadata(
            registrationContext));
        CHECK(Aero::Controls::Detail::PopulateControlsMetadata(
            registrationContext));
        const StringView ns("urn:render-tests");
        objectType = BuiltinTypes::Object;
        elementType = BuiltinTypes::FrameworkElement;
        panelType = MakeTypeId(ns, StringView("RenderPanel"));
        textBlockType = BuiltinTypes::TextBlock;
        borderType = BuiltinTypes::Border;
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("RenderPanel"), elementType, TypeFlags::None, nullptr)));
        CHECK(types.Freeze()); CHECK(typesBehaviors.Freeze()); CHECK(valueRegistrations.Freeze());
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
    CHECK(builder.FillRoundedRect({2.0, 2.0, 8.0, 6.0},
        {0.75F, 0.5F, 0.25F, 1.0F}, 3.0));
    CHECK(builder.DrawImage(1U, {2.0, 2.0, 8.0, 6.0},
        {0.0, 0.0, 1.0, 1.0}));
    CHECK(builder.DrawMesh(1U));
    CHECK(builder.DrawGlyphRun(1U));
    CHECK(builder.PopClip());
    CHECK(builder.PopTransform());
    Result<DisplayList> list = builder.Finish();
    CHECK(list);
    CHECK(list.Value().CommandCount() == 9U);
    CHECK(list.Value().StableHash() != 0U);

    DisplayListBuilder unbalanced;
    CHECK(unbalanced.PushOpacity(0.5));
    Result<DisplayList> invalid = unbalanced.Finish();
    CHECK(!invalid && invalid.GetStatus().code == ErrorCode::InvalidState);

    DisplayListBuilder badColor;
    Result<void> invalidColor = badColor.FillRect(
        {0.0, 0.0, 1.0, 1.0}, {2.0F, 0.0F, 0.0F, 1.0F});
    CHECK(!invalidColor && invalidColor.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> invalidCornerRadius = badColor.FillRoundedRect(
        {0.0, 0.0, 4.0, 2.0}, {0.0F, 0.0F, 0.0F, 1.0F}, 1.5);
    CHECK(!invalidCornerRadius &&
        invalidCornerRadius.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> invalidImage = badColor.DrawImage(
        InvalidRenderImageId, {0.0, 0.0, 1.0, 1.0}, {0.0, 0.0, 1.0, 1.0});
    CHECK(!invalidImage && invalidImage.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> invalidImageUv = badColor.DrawImage(
        1U, {0.0, 0.0, 1.0, 1.0}, {0.5, 0.0, 0.75, 1.0});
    CHECK(!invalidImageUv && invalidImageUv.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> negativeImageUv = badColor.DrawImage(
        1U, {0.0, 0.0, 1.0, 1.0}, {-0.25, 0.0, 0.5, 1.0});
    CHECK(!negativeImageUv &&
        negativeImageUv.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> invalidMesh = badColor.DrawMesh(InvalidRenderMeshId);
    CHECK(!invalidMesh && invalidMesh.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> invalidGlyphRun = badColor.DrawGlyphRun(InvalidRenderGlyphRunId);
    CHECK(!invalidGlyphRun &&
        invalidGlyphRun.GetStatus().code == ErrorCode::InvalidArgument);
    return true;
}

bool TestBorderDisplayList() {
    Fixture fixture;
    CHECK(fixture.Build());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());
    TestBorder border(fixture.borderType);
    CHECK(border.SetBackground({0.1F, 0.2F, 0.3F, 1.0F}));
    CHECK(border.SetStroke({1.0F, 1.0F, 1.0F, 1.0F}, 2.0));
    CHECK(!border.SetBackground({2.0F, 0.0F, 0.0F, 1.0F}));
    CHECK(layout.SetRoot(&border, {40.0, 30.0}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    DisplayListBuilder builder;
    CHECK(border.BuildDisplayList(builder));
    Result<DisplayList> list = builder.Finish();
    CHECK(list);
    CHECK(list.Value().CommandCount() == 2U);
    CHECK(list.Value().Commands()[0].kind == RenderCommandKind::FillRect);
    CHECK(list.Value().Commands()[1].kind == RenderCommandKind::StrokeRect);
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
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

    RenderPanel root(fixture.panelType);
    RenderBox first(fixture.elementType,
        {30.0, 10.0}, {1.0F, 0.0F, 0.0F, 1.0F});
    RenderBox second(fixture.elementType,
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
    RenderPanel root(fixture.panelType);
    CHECK(renderer.SetRoot(&root));
    Result<std::uint32_t> commit = renderer.Commit();
    CHECK(!commit && commit.GetStatus().code == ErrorCode::InvalidState);
    CHECK(renderer.SetRoot(nullptr));
    return true;
}

bool TestTextBlockGlyphRunRendering() {
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
    TextBlock text;
    CHECK(text.SetText(StringView("Hello")));
    CHECK(text.Text() == StringView("Hello"));
    CHECK(!text.SetGlyphRun(InvalidRenderGlyphRunId, {12.0, 8.0}));
    CHECK(text.SetGlyphRun(7U, {12.0, 8.0}));
    CHECK(text.SetForeground({0.25F, 0.5F, 0.75F, 1.0F}));
    CHECK(tree.SetRoot(&text));
    CHECK(layout.SetRoot(&text, {40.0, 20.0}));
    CHECK(renderer.SetRoot(&text));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(text.DesiredSize().width == 12.0 && text.DesiredSize().height == 8.0);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    const RenderPlan& plan = renderer.CurrentPlan();
    CHECK(plan.Nodes().Size() == 1U);
    CHECK(plan.Commands().Size() == 1U);
    CHECK(plan.Commands()[0].kind == RenderCommandKind::DrawGlyphRun);
    CHECK(plan.Commands()[0].glyphRun == 7U);
    CHECK(plan.Commands()[0].color.red == 0.25F);
    CHECK(renderer.SetRoot(nullptr));
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(text));
    return true;
}

} // namespace

int main() {
    if (!TestDisplayListValidation()) return 1;
    if (!TestBorderDisplayList()) return 1;
    if (!TestRenderCommitAndInvalidation()) return 1;
    if (!TestRenderRequiresArrange()) return 1;
    if (!TestTextBlockGlyphRunRendering()) return 1;
    std::puts("Aero rendering tests passed");
    return 0;
}
