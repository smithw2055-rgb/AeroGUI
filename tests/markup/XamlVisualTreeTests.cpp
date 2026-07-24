#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Controls/RuntimeMetadata.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XamlVisualTree.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <memory>
#include <utility>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Controls;
using namespace Aero::Markup;

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    return false; } } while (false)

class TestLeaf final : public FrameworkElement {
public:
    explicit TestLeaf(TypeId type) noexcept
        : FrameworkElement(type) {}

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
    Dispatcher dispatcher;
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<EffectiveValueEngine> values;
    std::unique_ptr<ObjectTree> tree;
    std::unique_ptr<LayoutManager> layout;
    std::unique_ptr<RenderManager> renderer;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<XamlActivationProviderRegistry> activation;
    std::unique_ptr<XamlDependencyPropertyBridge> dependencyProperties;
    std::unique_ptr<XamlVisualTreeHost> visual;
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

    static Result<Ref<Object>> Activate(
        TypeId requestedType,
        const XamlActivationContext& activationContext,
        void*) noexcept {
        if (activationContext.dispatcher == nullptr ||
            activationContext.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "XAML visual-tree activation services are missing");
        }
        Result<Ref<TestLeaf>> made = MakeRef<TestLeaf>(requestedType);
        if (!made) return made.GetStatus();
        Ref<TestLeaf> leaf = std::move(made).Value();
        return Ref<Object>(std::move(leaf));
    }

    static Result<void> RegisterLeafModule(
        MetaRegistrationContext& context,
        void* userContext) noexcept {
        auto* fixture = static_cast<Fixture*>(userContext);
        const StringView ns("urn:xaml-visual");
        Result<TypeId> registered = context.Types().TryRegisterType(TypeRegistration::Object(ns, StringView("Leaf"), BuiltinTypes::FrameworkElement, TypeFlags::None, nullptr));
        if (!registered) return registered.GetStatus();
        return registered.Value() == fixture->leafType
            ? Result<void>()
            : Result<void>(Status::Failure(
                ErrorCode::IdCollision, "Unexpected Leaf TypeId"));
    }

    bool Build() {
        const StringView ns("urn:xaml-visual");
        objectType = BuiltinTypes::Object;
        doubleType = BuiltinTypes::Double;
        unsignedType = BuiltinTypes::UnsignedInteger;
        layoutType = BuiltinTypes::UIElement;
        presenterType = BuiltinTypes::ContentPresenter;
        borderType = BuiltinTypes::Border;
        stackPanelType = BuiltinTypes::StackPanel;
        canvasType = BuiltinTypes::Canvas;
        gridType = BuiltinTypes::Grid;
        leafType = MakeTypeId(ns, StringView("Leaf"));
        CHECK(Aero::Controls::TryRegisterBuiltInUiMetadata(metadata));
        const StringView moduleName("Tests.XamlVisualTree");
        CHECK(metadata.TryRegisterModule({
            MakeMetadataModuleId(moduleName), moduleName, 1U,
            &Fixture::RegisterLeafModule, this}));
        CHECK(metadata.Seal());

        stackPanelChildren = MakeMemberId(
            stackPanelType, MemberKind::Property, StringView("Children"));
        canvasChildren = MakeMemberId(
            canvasType, MemberKind::Property, StringView("Children"));
        gridChildren = MakeMemberId(
            gridType, MemberKind::Property, StringView("Children"));

        runtime = std::make_unique<MetadataRuntime>(metadata);
        values = std::make_unique<EffectiveValueEngine>(
            dispatcher, metadata.DependencyProperties());
        tree = std::make_unique<ObjectTree>(dispatcher, *values);
        layout = std::make_unique<LayoutManager>(dispatcher);
        renderer = std::make_unique<RenderManager>(dispatcher);
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);
        activation = std::make_unique<XamlActivationProviderRegistry>(*schema);
        dependencyProperties = std::make_unique<XamlDependencyPropertyBridge>(
            *schema, metadata.DependencyProperties());
        visual = std::make_unique<XamlVisualTreeHost>(
            *tree, *layout, *values, renderer.get());

        CHECK(values->Initialize());
        CHECK(tree->Initialize());
        CHECK(layout->Initialize());
        CHECK(renderer->Initialize());
        CHECK(TryRegisterAeroPresentationXaml(
            *dependencyProperties, *activation, visual.get()));
        CHECK(activation->TryRegister({leafType, &Activate, nullptr}));
        CHECK(runtime->Freeze());
        CHECK(schema->Freeze());
        CHECK(activation->Freeze());
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext result = XamlActivationContext::Create();
        result.dispatcher = &dispatcher;
        result.dependencyProperties = &metadata.DependencyProperties();
        return result;
    }
};

bool TestXamlContentMountLayoutRenderAndUnmount() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<ContentPresenter xmlns=\"urn:aero\" xmlns:local=\"urn:xaml-visual\">"
        "<ContentPresenter><local:Leaf/>"
        "</ContentPresenter></ContentPresenter>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        *fixture.visual, writer, reader, *fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    ContentPresenter* root = static_cast<ContentPresenter*>(loaded.Value().Get());
    CHECK(root != nullptr && root->Content() != nullptr);
    ContentPresenter* nested = static_cast<ContentPresenter*>(root->Content());
    CHECK(nested->Content() != nullptr);
    TestLeaf* leaf = static_cast<TestLeaf*>(nested->Content());
    Ref<Object> nestedKeep = root->OwnedContent();
    Ref<Object> leafKeep = nested->OwnedContent();
    CHECK(nestedKeep.Get() == nested && leafKeep.Get() == leaf);
    CHECK(fixture.visual->StagedContentCount() == 2U);
    CHECK(fixture.visual->Mount(*root, fixture.presenterType, {80.0, 40.0}));
    CHECK(fixture.tree->Root() == root);
    CHECK(nested->LogicalParent() == root && nested->VisualParent() == root);
    CHECK(leaf->LogicalParent() == nested && leaf->VisualParent() == nested);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(root->IsArrangeValid() && leaf->IsArrangeValid());
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer->CurrentPlan().Nodes().Size() == 3U);
    CHECK(fixture.visual->Unmount());
    CHECK(fixture.tree->Root() == nullptr);
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
        "<Border xmlns=\"urn:aero\" xmlns:local=\"urn:xaml-visual\">"
        "<local:Leaf/></Border>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        *fixture.visual, writer, reader, *fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Border* root = static_cast<Border*>(loaded.Value().Get());
    CHECK(root != nullptr);
    CHECK(fixture.visual->Mount(*root, fixture.borderType, {80.0, 40.0}));
    CHECK(root->LogicalChildren().Size() == 1U && root->VisualChildren().Size() == 1U);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer->CurrentPlan().Nodes().Size() == 2U);
    CHECK(fixture.visual->Unmount());
    CHECK(fixture.tree->Root() == nullptr);
    return true;
}

bool TestXamlStackPanelCollectionMountLayoutRenderAndUnmount() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<StackPanel xmlns=\"urn:aero\" xmlns:local=\"urn:xaml-visual\">"
        "<local:Leaf/><local:Leaf/></StackPanel>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        *fixture.visual, writer, reader, *fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    StackPanel* root = static_cast<StackPanel*>(loaded.Value().Get());
    CHECK(root != nullptr && root->OwnedChildCount() == 2U);
    CHECK(fixture.visual->StagedContentCount() == 2U);
    CHECK(fixture.visual->Mount(*root, fixture.stackPanelType, {80.0, 40.0}));
    CHECK(root->LogicalChildren().Size() == 2U && root->VisualChildren().Size() == 2U);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(root->DesiredSize().width == 20.0 && root->DesiredSize().height == 20.0);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer->CurrentPlan().Nodes().Size() == 3U);
    CHECK(fixture.visual->Unmount());
    CHECK(root->OwnedChildCount() == 0U);
    CHECK(fixture.tree->Root() == nullptr);
    return true;
}

bool TestXamlCanvasGenericCollectionMountLayoutRenderAndUnmount() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<Canvas xmlns=\"urn:aero\" xmlns:local=\"urn:xaml-visual\" "
        "xmlns:aero=\"urn:aero\"><local:Leaf aero:Canvas.Left=\"8\" "
        "aero:Canvas.Top=\"9\"/></Canvas>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        *fixture.visual, writer, reader, *fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Canvas* root = static_cast<Canvas*>(loaded.Value().Get());
    CHECK(root != nullptr);
    CHECK(fixture.visual->StagedContentCount() == 1U);
    CHECK(fixture.visual->Mount(*root, fixture.canvasType, {80.0, 40.0}));
    CHECK(root->LogicalChildren().Size() == 1U && root->VisualChildren().Size() == 1U);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    const Rect childSlot = static_cast<UIElement*>(root->VisualChildren()[0])->
        LayoutSlot();
    CHECK(childSlot.x == 8.0 && childSlot.y == 9.0);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer->CurrentPlan().Nodes().Size() == 2U);
    CHECK(fixture.visual->Unmount());
    CHECK(fixture.tree->Root() == nullptr);
    return true;
}

bool TestXamlGridGenericCollectionMountLayoutRenderAndUnmount() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<Grid xmlns=\"urn:aero\" xmlns:local=\"urn:xaml-visual\" "
        "xmlns:aero=\"urn:aero\"><local:Leaf aero:Grid.Row=\"1\" "
        "aero:Grid.Column=\"1\"/></Grid>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        *fixture.visual, writer, reader, *fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Grid* root = static_cast<Grid*>(loaded.Value().Get());
    CHECK(root != nullptr);
    const GridLength tracks[] = {GridLength::Star(), GridLength::Star()};
    CHECK(root->SetColumnDefinitions({tracks, 2U}));
    CHECK(root->SetRowDefinitions({tracks, 2U}));
    CHECK(fixture.visual->Mount(*root, fixture.gridType, {80.0, 40.0}));
    CHECK(root->LogicalChildren().Size() == 1U && root->VisualChildren().Size() == 1U);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    const Rect childSlot = static_cast<UIElement*>(root->VisualChildren()[0])->
        LayoutSlot();
    CHECK(childSlot.x == 40.0 && childSlot.y == 20.0 &&
        childSlot.width == 40.0 && childSlot.height == 20.0);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer->CurrentPlan().Nodes().Size() == 2U);
    CHECK(fixture.visual->Unmount());
    CHECK(fixture.tree->Root() == nullptr);
    return true;
}

bool TestXamlScrollViewerMountAndOffset() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<ScrollViewer xmlns=\"urn:aero\" "
        "xmlns:local=\"urn:xaml-visual\" "
        "CanHorizontallyScroll=\"False\">"
        "<StackPanel><local:Leaf/><local:Leaf/>"
        "</StackPanel></ScrollViewer>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded =
        LoadXamlVisualTreeWithActivation(
            *fixture.visual,
            writer,
            reader,
            *fixture.activation,
            fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    auto* root =
        static_cast<ScrollViewer*>(
            loaded.Value().Get());
    CHECK(root != nullptr);
    CHECK(!root->CanHorizontallyScroll());
    CHECK(root->Child() != nullptr);
    CHECK(fixture.visual->Mount(
        *root,
        BuiltinTypes::ScrollViewer,
        {20.0, 8.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(root->ExtentHeight() == 20.0);
    CHECK(root->ViewportHeight() == 8.0);
    CHECK(root->SetVerticalOffset(12.0));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(root->VerticalOffset() == 12.0);
    CHECK(root->Child()->LayoutSlot().y == -12.0);
    CHECK(fixture.visual->Unmount());
    CHECK(fixture.tree->Root() == nullptr);
    return true;
}

bool TestXamlItemsControlCollection() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<ItemsControl xmlns=\"urn:aero\">"
        "<TextBlock Text=\"One\"/>"
        "<Button/>"
        "</ItemsControl>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded =
        LoadXamlVisualTreeWithActivation(
            *fixture.visual,
            writer,
            reader,
            *fixture.activation,
            fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    auto* root =
        static_cast<ItemsControl*>(
            loaded.Value().Get());
    CHECK(root != nullptr);
    CHECK(root->ItemCount() == 2U);
    CHECK(root->ItemAt(0U)->RuntimeType() ==
        TextBlock::StaticTypeId());
    CHECK(root->ItemAt(1U)->RuntimeType() ==
        Button::StaticTypeId());
    CHECK(fixture.visual->StagedContentCount() == 0U);
    CHECK(fixture.visual->Mount(
        *root,
        BuiltinTypes::ItemsControl,
        {80.0, 40.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(fixture.visual->Unmount());
    return true;
}

bool TestXamlListBoxCollectionAndSelectionMode() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<ListBox xmlns=\"urn:aero\" "
        "SelectionMode=\"Extended\" "
        "SelectedIndex=\"1\">"
        "<TextBlock Text=\"One\"/>"
        "<TextBlock Text=\"Two\"/>"
        "</ListBox>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded =
        LoadXamlVisualTreeWithActivation(
            *fixture.visual,
            writer,
            reader,
            *fixture.activation,
            fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    auto* root =
        static_cast<ListBox*>(
            loaded.Value().Get());
    CHECK(root != nullptr);
    CHECK(root->ItemCount() == 2U);
    CHECK(root->GetSelectionMode() ==
        SelectionMode::Extended);
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    if (root->SelectedIndex() != 1U) {
        std::fprintf(stderr,
            "XAML ListBox selected index=%u count=%u error=%s\n",
            root->SelectedIndex(),
            root->SelectedCount(),
            root->LastSelectionError().message);
    }
    CHECK(root->SelectedIndex() == 1U);
    CHECK(root->SelectedCount() == 1U);
    CHECK(root->SelectedItem().Get() ==
        root->ItemAt(1U).Get());
    CHECK(fixture.visual->StagedContentCount() == 0U);
    CHECK(fixture.visual->Mount(
        *root,
        BuiltinTypes::ListBox,
        {80.0, 40.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(fixture.visual->Unmount());
    return true;
}

bool TestFailedLoadDiscardsStagedEdges() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<ContentPresenter xmlns=\"urn:aero\" xmlns:local=\"urn:xaml-visual\">"
        "<local:Leaf/></ContentPresenter>"
        "<local:Leaf xmlns:local=\"urn:xaml-visual\"/>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        *fixture.visual, writer, reader, *fixture.activation, fixture.Activation());
    CHECK(!loaded);
    CHECK(fixture.visual->StagedContentCount() == 0U);
    CHECK(!fixture.visual->IsMounted());
    CHECK(fixture.tree->Root() == nullptr);
    return true;
}

bool TestXamlGridRejectsOutOfRangeCellOnFirstLayout() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<Grid xmlns=\"urn:aero\" xmlns:local=\"urn:xaml-visual\" "
        "xmlns:aero=\"urn:aero\"><local:Leaf aero:Grid.Row=\"2\"/>"
        "</Grid>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        *fixture.visual, writer, reader, *fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Grid* root = static_cast<Grid*>(loaded.Value().Get());
    const GridLength track[] = {GridLength::Star()};
    CHECK(root->SetColumnDefinitions({track, 1U}));
    CHECK(root->SetRowDefinitions({track, 1U}));
    CHECK(fixture.visual->Mount(*root, fixture.gridType, {80.0, 40.0}));
    Result<std::uint32_t> phase = fixture.layout->Flush();
    const ErrorCode layoutError = phase ? ErrorCode::Ok : phase.GetStatus().code;
    CHECK(fixture.visual->Unmount());
    CHECK(!phase && layoutError == ErrorCode::OutOfRange);
    return true;
}

} // namespace

int main() {
    if (!TestXamlContentMountLayoutRenderAndUnmount()) return 1;
    if (!TestXamlBorderContentMountLayoutRenderAndUnmount()) return 1;
    if (!TestXamlStackPanelCollectionMountLayoutRenderAndUnmount()) return 1;
    if (!TestXamlCanvasGenericCollectionMountLayoutRenderAndUnmount()) return 1;
    if (!TestXamlGridGenericCollectionMountLayoutRenderAndUnmount()) return 1;
    if (!TestXamlScrollViewerMountAndOffset()) return 1;
    if (!TestXamlItemsControlCollection()) return 1;
    if (!TestXamlListBoxCollectionAndSelectionMode()) return 1;
    if (!TestXamlGridRejectsOutOfRangeCellOnFirstLayout()) return 1;
    if (!TestFailedLoadDiscardsStagedEdges()) return 1;
    std::puts("Aero XAML visual-tree tests passed");
    return 0;
}
