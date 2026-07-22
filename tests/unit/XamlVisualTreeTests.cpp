#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Controls.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/EffectiveValueEngine.hpp>
#include <Aero/Core/ObjectTree.hpp>
#include <Aero/Core/Rendering.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlPanelLayout.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XamlVisualTree.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <utility>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Markup;

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    return false; } } while (false)

class TestLeaf final : public RenderElement {
public:
    TestLeaf(Dispatcher& dispatcher, DependencyPropertyRegistry& properties,
        TypeId type, IAllocator* allocator) noexcept
        : RenderElement(dispatcher, properties, type, allocator) {}

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        return Size{available.width < 20.0 ? available.width : 20.0,
            available.height < 10.0 ? available.height : 10.0};
    }
    Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override {
        return builder.FillRect({0.0, 0.0, RenderSize().width, RenderSize().height},
            {0.2F, 0.4F, 0.8F, 1.0F});
    }
};

struct Fixture final {
    Fixture() noexcept
        : panelLayout(
            MakeTypeId(StringView("urn:xaml-visual"), StringView("Canvas")),
            MakeTypeId(StringView("urn:xaml-visual"), StringView("Grid"))) {}

    Dispatcher dispatcher;
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    EffectiveValueEngine values{dispatcher, properties};
    ObjectTree tree{dispatcher, values};
    LayoutManager layout{dispatcher};
    NullRenderBackend backend;
    RenderManager renderer{dispatcher, backend};
    XamlSchemaContext schema{types};
    XamlActivationProviderRegistry activation{schema};
    XamlPanelLayoutExtension panelLayout;
    XamlVisualTreeHost visual{tree, layout, values, &renderer};
    TypeId objectType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId unsignedType = InvalidTypeId;
    TypeId layoutType = InvalidTypeId;
    TypeId presenterType = InvalidTypeId;
    TypeId borderType = InvalidTypeId;
    TypeId stackPanelType = InvalidTypeId;
    TypeId canvasType = InvalidTypeId;
    TypeId gridType = InvalidTypeId;
    TypeId leafType = InvalidTypeId;
    MemberId stackPanelChildren = InvalidMemberId;
    MemberId canvasChildren = InvalidMemberId;
    MemberId gridChildren = InvalidMemberId;

    static Result<Ref<Object>> Activate(TypeId requestedType,
        const XamlActivationContext& activationContext,
        IAllocator& allocator, void*) noexcept {
        if (activationContext.dispatcher == nullptr ||
            activationContext.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "XAML visual-tree activation services are missing");
        }
        if (requestedType == MakeTypeId(StringView("urn:xaml-visual"), StringView("ContentPresenter"))) {
            Result<Ref<ContentPresenter>> made = MakeRefWithAllocator<ContentPresenter>(allocator,
                *activationContext.dispatcher, *activationContext.dependencyProperties,
                requestedType, &allocator);
            if (!made) return made.GetStatus();
            Ref<ContentPresenter> presenter = std::move(made).Value();
            return Ref<Object>(std::move(presenter));
        }
        if (requestedType == MakeTypeId(StringView("urn:xaml-visual"), StringView("Border"))) {
            Result<Ref<Border>> made = MakeRefWithAllocator<Border>(allocator,
                *activationContext.dispatcher, *activationContext.dependencyProperties,
                requestedType, &allocator);
            if (!made) return made.GetStatus();
            Ref<Border> border = std::move(made).Value();
            return Ref<Object>(std::move(border));
        }
        if (requestedType == MakeTypeId(StringView("urn:xaml-visual"), StringView("StackPanel"))) {
            Result<Ref<StackPanel>> made = MakeRefWithAllocator<StackPanel>(allocator,
                *activationContext.dispatcher, *activationContext.dependencyProperties,
                requestedType, Orientation::Vertical, &allocator);
            if (!made) return made.GetStatus();
            Ref<StackPanel> panel = std::move(made).Value();
            return Ref<Object>(std::move(panel));
        }
        if (requestedType == MakeTypeId(StringView("urn:xaml-visual"), StringView("Canvas"))) {
            Result<Ref<Canvas>> made = MakeRefWithAllocator<Canvas>(allocator,
                *activationContext.dispatcher, *activationContext.dependencyProperties,
                requestedType, &allocator);
            if (!made) return made.GetStatus();
            Ref<Canvas> canvas = std::move(made).Value();
            return Ref<Object>(std::move(canvas));
        }
        if (requestedType == MakeTypeId(StringView("urn:xaml-visual"), StringView("Grid"))) {
            Result<Ref<Grid>> made = MakeRefWithAllocator<Grid>(allocator,
                *activationContext.dispatcher, *activationContext.dependencyProperties,
                requestedType, &allocator);
            if (!made) return made.GetStatus();
            Ref<Grid> grid = std::move(made).Value();
            return Ref<Object>(std::move(grid));
        }
        Result<Ref<TestLeaf>> made = MakeRefWithAllocator<TestLeaf>(allocator,
            *activationContext.dispatcher, *activationContext.dependencyProperties,
            requestedType, &allocator);
        if (!made) return made.GetStatus();
        Ref<TestLeaf> leaf = std::move(made).Value();
        return Ref<Object>(std::move(leaf));
    }

    static TreeNode* AsTreeNode(Object& object, void*) noexcept {
        return &static_cast<LayoutElement&>(object);
    }
    static LayoutElement* AsLayout(Object& object, void*) noexcept {
        return &static_cast<LayoutElement&>(object);
    }
    static RenderElement* AsRender(Object& object, void*) noexcept {
        return &static_cast<RenderElement&>(object);
    }
    static ContentPresenter* AsPresenter(Object& object, void*) noexcept {
        return &static_cast<ContentPresenter&>(object);
    }
    static StackPanel* AsStackPanel(Object& object, void*) noexcept {
        return &static_cast<StackPanel&>(object);
    }
    static LayoutElement* AsBorder(Object& object, void*) noexcept {
        return &static_cast<Border&>(object);
    }
    static LayoutElement* AsCanvas(Object& object, void*) noexcept {
        return &static_cast<Canvas&>(object);
    }
    static LayoutElement* AsGrid(Object& object, void*) noexcept {
        return &static_cast<Grid&>(object);
    }

    bool Build() {
        const StringView ns("urn:xaml-visual");
        objectType = MakeTypeId(ns, StringView("Object"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        unsignedType = MakeTypeId(ns, StringView("Unsigned"));
        layoutType = MakeTypeId(ns, StringView("LayoutElement"));
        presenterType = MakeTypeId(ns, StringView("ContentPresenter"));
        borderType = MakeTypeId(ns, StringView("Border"));
        stackPanelType = MakeTypeId(ns, StringView("StackPanel"));
        canvasType = MakeTypeId(ns, StringView("Canvas"));
        gridType = MakeTypeId(ns, StringView("Grid"));
        leafType = MakeTypeId(ns, StringView("Leaf"));
        CHECK(types.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("Double"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("Unsigned"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("LayoutElement"), objectType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("ContentPresenter"), layoutType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("Border"), layoutType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("StackPanel"), layoutType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("Canvas"), layoutType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("Grid"), layoutType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("Leaf"), layoutType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterProperty(presenterType, {
            StringView("Content"), layoutType, PropertyFlags::None}));
        CHECK(types.TryRegisterProperty(borderType, {
            StringView("Content"), layoutType, PropertyFlags::None}));
        Result<MemberId> children = types.TryRegisterProperty(stackPanelType, {
            StringView("Children"), layoutType, PropertyFlags::None});
        CHECK(children);
        stackPanelChildren = children.Value();
        Result<MemberId> canvasChildrenResult = types.TryRegisterProperty(canvasType, {
            StringView("Children"), layoutType, PropertyFlags::None});
        CHECK(canvasChildrenResult);
        canvasChildren = canvasChildrenResult.Value();
        Result<MemberId> gridChildrenResult = types.TryRegisterProperty(gridType, {
            StringView("Children"), layoutType, PropertyFlags::None});
        CHECK(gridChildrenResult);
        gridChildren = gridChildrenResult.Value();
        CHECK(types.TryRegisterProperty(canvasType, {
            StringView("Left"), doubleType, PropertyFlags::Attached}));
        CHECK(types.TryRegisterProperty(canvasType, {
            StringView("Top"), doubleType, PropertyFlags::Attached}));
        CHECK(types.TryRegisterProperty(gridType, {
            StringView("Row"), unsignedType, PropertyFlags::Attached}));
        CHECK(types.TryRegisterProperty(gridType, {
            StringView("Column"), unsignedType, PropertyFlags::Attached}));
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(schema.TryRegisterScalarType(doubleType, XamlScalarKind::Double));
        CHECK(schema.TryRegisterScalarType(unsignedType, XamlScalarKind::UnsignedInteger));
        CHECK(values.Initialize());
        CHECK(tree.Initialize());
        CHECK(layout.Initialize());
        CHECK(renderer.Initialize());
        CHECK(visual.TryRegisterType({presenterType, &AsTreeNode, &AsLayout, &AsRender, nullptr}));
        CHECK(visual.TryRegisterType({borderType, &AsTreeNode, &AsLayout, &AsRender, nullptr}));
        CHECK(visual.TryRegisterType({stackPanelType, &AsTreeNode, &AsLayout, &AsRender, nullptr}));
        CHECK(visual.TryRegisterType({canvasType, &AsTreeNode, &AsLayout, &AsRender, nullptr}));
        CHECK(visual.TryRegisterType({gridType, &AsTreeNode, &AsLayout, &AsRender, nullptr}));
        CHECK(visual.TryRegisterType({leafType, &AsTreeNode, &AsLayout, &AsRender, nullptr}));
        CHECK(visual.TryRegisterContentPresenter({presenterType, &AsPresenter, nullptr}));
        CHECK(visual.TryRegisterContentPresenter({borderType, nullptr, nullptr, &AsBorder}));
        CHECK(visual.TryRegisterCollectionContent({stackPanelType, stackPanelChildren,
            &AsStackPanel, nullptr}));
        CHECK(visual.TryRegisterCollectionContent({canvasType, canvasChildren,
            nullptr, &panelLayout, &AsCanvas,
            &XamlPanelLayoutExtension::ConfigureCanvasChild}));
        CHECK(visual.TryRegisterCollectionContent({gridType, gridChildren,
            nullptr, &panelLayout, &AsGrid,
            &XamlPanelLayoutExtension::ConfigureGridChild}));
        CHECK(visual.Register(schema));
        CHECK(panelLayout.TryRegisterType({leafType, &AsLayout, nullptr}));
        CHECK(panelLayout.Register(schema));
        CHECK(activation.TryRegister({presenterType, &Activate, nullptr}));
        CHECK(activation.TryRegister({borderType, &Activate, nullptr}));
        CHECK(activation.TryRegister({stackPanelType, &Activate, nullptr}));
        CHECK(activation.TryRegister({canvasType, &Activate, nullptr}));
        CHECK(activation.TryRegister({gridType, &Activate, nullptr}));
        CHECK(activation.TryRegister({leafType, &Activate, nullptr}));
        CHECK(schema.Freeze());
        CHECK(activation.Freeze());
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext result = XamlActivationContext::Create();
        result.dispatcher = &dispatcher;
        result.dependencyProperties = &properties;
        return result;
    }
};

bool TestXamlContentMountLayoutRenderAndUnmount() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<ContentPresenter xmlns=\"urn:xaml-visual\"><ContentPresenter><Leaf/>"
        "</ContentPresenter></ContentPresenter>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        fixture.visual, writer, reader, fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    ContentPresenter* root = static_cast<ContentPresenter*>(loaded.Value().Get());
    CHECK(root != nullptr && root->Content() != nullptr);
    ContentPresenter* nested = static_cast<ContentPresenter*>(root->Content());
    CHECK(nested->Content() != nullptr);
    TestLeaf* leaf = static_cast<TestLeaf*>(nested->Content());
    Ref<Object> nestedKeep = root->OwnedContent();
    Ref<Object> leafKeep = nested->OwnedContent();
    CHECK(nestedKeep.Get() == nested && leafKeep.Get() == leaf);
    CHECK(fixture.visual.StagedContentCount() == 2U);
    CHECK(fixture.visual.Mount(*root, fixture.presenterType, {80.0, 40.0}));
    CHECK(fixture.tree.Root() == root);
    CHECK(nested->LogicalParent() == root && nested->VisualParent() == root);
    CHECK(leaf->LogicalParent() == nested && leaf->VisualParent() == nested);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(root->IsArrangeValid() && leaf->IsArrangeValid());
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer.CurrentPlan().Nodes().Size() == 3U);
    CHECK(fixture.backend.SubmissionCount() == 1U);
    CHECK(fixture.visual.Unmount());
    CHECK(fixture.tree.Root() == nullptr);
    CHECK(root->Content() == nullptr);
    CHECK(nested->Content() == nullptr);
    CHECK(leaf->LogicalParent() == nullptr && leaf->VisualParent() == nullptr);
    return true;
}

bool TestXamlBorderContentMountLayoutRenderAndUnmount() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<Border xmlns=\"urn:xaml-visual\"><Leaf/></Border>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        fixture.visual, writer, reader, fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Border* root = static_cast<Border*>(loaded.Value().Get());
    CHECK(root != nullptr);
    CHECK(fixture.visual.Mount(*root, fixture.borderType, {80.0, 40.0}));
    CHECK(root->LogicalChildren().Size() == 1U && root->VisualChildren().Size() == 1U);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer.CurrentPlan().Nodes().Size() == 2U);
    CHECK(fixture.backend.SubmissionCount() == 1U);
    CHECK(fixture.visual.Unmount());
    CHECK(fixture.tree.Root() == nullptr);
    return true;
}

bool TestXamlStackPanelCollectionMountLayoutRenderAndUnmount() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<StackPanel xmlns=\"urn:xaml-visual\"><Leaf/><Leaf/></StackPanel>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        fixture.visual, writer, reader, fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    StackPanel* root = static_cast<StackPanel*>(loaded.Value().Get());
    CHECK(root != nullptr && root->OwnedChildCount() == 2U);
    CHECK(fixture.visual.StagedContentCount() == 2U);
    CHECK(fixture.visual.Mount(*root, fixture.stackPanelType, {80.0, 40.0}));
    CHECK(root->LogicalChildren().Size() == 2U && root->VisualChildren().Size() == 2U);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(root->DesiredSize().width == 20.0 && root->DesiredSize().height == 20.0);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer.CurrentPlan().Nodes().Size() == 3U);
    CHECK(fixture.backend.SubmissionCount() == 1U);
    CHECK(fixture.visual.Unmount());
    CHECK(root->OwnedChildCount() == 0U);
    CHECK(fixture.tree.Root() == nullptr);
    return true;
}

bool TestXamlCanvasGenericCollectionMountLayoutRenderAndUnmount() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<Canvas xmlns=\"urn:xaml-visual\"><Leaf Canvas.Left=\"8\" "
        "Canvas.Top=\"9\"/></Canvas>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        fixture.visual, writer, reader, fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Canvas* root = static_cast<Canvas*>(loaded.Value().Get());
    CHECK(root != nullptr);
    CHECK(fixture.visual.StagedContentCount() == 1U);
    CHECK(fixture.visual.Mount(*root, fixture.canvasType, {80.0, 40.0}));
    CHECK(root->LogicalChildren().Size() == 1U && root->VisualChildren().Size() == 1U);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    const Rect childSlot = static_cast<LayoutElement*>(root->VisualChildren()[0])->
        LayoutSlot();
    CHECK(childSlot.x == 8.0 && childSlot.y == 9.0);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer.CurrentPlan().Nodes().Size() == 2U);
    CHECK(fixture.backend.SubmissionCount() == 1U);
    CHECK(fixture.visual.Unmount());
    CHECK(fixture.tree.Root() == nullptr);
    return true;
}

bool TestXamlGridGenericCollectionMountLayoutRenderAndUnmount() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<Grid xmlns=\"urn:xaml-visual\"><Leaf Grid.Row=\"1\" "
        "Grid.Column=\"1\"/></Grid>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        fixture.visual, writer, reader, fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Grid* root = static_cast<Grid*>(loaded.Value().Get());
    CHECK(root != nullptr);
    const GridLength tracks[] = {GridLength::Star(), GridLength::Star()};
    CHECK(root->SetColumnDefinitions({tracks, 2U}));
    CHECK(root->SetRowDefinitions({tracks, 2U}));
    CHECK(fixture.visual.Mount(*root, fixture.gridType, {80.0, 40.0}));
    CHECK(root->LogicalChildren().Size() == 1U && root->VisualChildren().Size() == 1U);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    const Rect childSlot = static_cast<LayoutElement*>(root->VisualChildren()[0])->
        LayoutSlot();
    CHECK(childSlot.x == 40.0 && childSlot.y == 20.0 &&
        childSlot.width == 40.0 && childSlot.height == 20.0);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer.CurrentPlan().Nodes().Size() == 2U);
    CHECK(fixture.backend.SubmissionCount() == 1U);
    CHECK(fixture.visual.Unmount());
    CHECK(fixture.tree.Root() == nullptr);
    return true;
}

bool TestFailedLoadDiscardsStagedEdges() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<ContentPresenter xmlns=\"urn:xaml-visual\"><Leaf/></ContentPresenter>"
        "<Leaf xmlns=\"urn:xaml-visual\"/>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        fixture.visual, writer, reader, fixture.activation, fixture.Activation());
    CHECK(!loaded);
    CHECK(fixture.visual.StagedContentCount() == 0U);
    CHECK(!fixture.visual.IsMounted());
    CHECK(fixture.tree.Root() == nullptr);
    return true;
}

} // namespace

int main() {
    if (!TestXamlContentMountLayoutRenderAndUnmount()) return 1;
    if (!TestXamlBorderContentMountLayoutRenderAndUnmount()) return 1;
    if (!TestXamlStackPanelCollectionMountLayoutRenderAndUnmount()) return 1;
    if (!TestXamlCanvasGenericCollectionMountLayoutRenderAndUnmount()) return 1;
    if (!TestXamlGridGenericCollectionMountLayoutRenderAndUnmount()) return 1;
    if (!TestFailedLoadDiscardsStagedEdges()) return 1;
    std::puts("Aero XAML visual-tree tests passed");
    return 0;
}
