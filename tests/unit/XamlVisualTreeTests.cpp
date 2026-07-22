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
    XamlVisualTreeHost visual{tree, layout, values, &renderer};
    TypeId objectType = InvalidTypeId;
    TypeId layoutType = InvalidTypeId;
    TypeId presenterType = InvalidTypeId;
    TypeId stackPanelType = InvalidTypeId;
    TypeId leafType = InvalidTypeId;
    MemberId stackPanelChildren = InvalidMemberId;

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
        if (requestedType == MakeTypeId(StringView("urn:xaml-visual"), StringView("StackPanel"))) {
            Result<Ref<StackPanel>> made = MakeRefWithAllocator<StackPanel>(allocator,
                *activationContext.dispatcher, *activationContext.dependencyProperties,
                requestedType, Orientation::Vertical, &allocator);
            if (!made) return made.GetStatus();
            Ref<StackPanel> panel = std::move(made).Value();
            return Ref<Object>(std::move(panel));
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

    bool Build() {
        const StringView ns("urn:xaml-visual");
        objectType = MakeTypeId(ns, StringView("Object"));
        layoutType = MakeTypeId(ns, StringView("LayoutElement"));
        presenterType = MakeTypeId(ns, StringView("ContentPresenter"));
        stackPanelType = MakeTypeId(ns, StringView("StackPanel"));
        leafType = MakeTypeId(ns, StringView("Leaf"));
        CHECK(types.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("LayoutElement"), objectType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("ContentPresenter"), layoutType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("StackPanel"), layoutType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("Leaf"), layoutType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterProperty(presenterType, {
            StringView("Content"), layoutType, PropertyFlags::None}));
        Result<MemberId> children = types.TryRegisterProperty(stackPanelType, {
            StringView("Children"), layoutType, PropertyFlags::None});
        CHECK(children);
        stackPanelChildren = children.Value();
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(values.Initialize());
        CHECK(tree.Initialize());
        CHECK(layout.Initialize());
        CHECK(renderer.Initialize());
        CHECK(visual.TryRegisterType({presenterType, &AsTreeNode, &AsLayout, &AsRender, nullptr}));
        CHECK(visual.TryRegisterType({stackPanelType, &AsTreeNode, &AsLayout, &AsRender, nullptr}));
        CHECK(visual.TryRegisterType({leafType, &AsTreeNode, &AsLayout, &AsRender, nullptr}));
        CHECK(visual.TryRegisterContentPresenter({presenterType, &AsPresenter, nullptr}));
        CHECK(visual.TryRegisterCollectionContent({stackPanelType, stackPanelChildren,
            &AsStackPanel, nullptr}));
        CHECK(visual.Register(schema));
        CHECK(activation.TryRegister({presenterType, &Activate, nullptr}));
        CHECK(activation.TryRegister({stackPanelType, &Activate, nullptr}));
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
    if (!TestXamlStackPanelCollectionMountLayoutRenderAndUnmount()) return 1;
    if (!TestFailedLoadDiscardsStagedEdges()) return 1;
    std::puts("Aero XAML visual-tree tests passed");
    return 0;
}
