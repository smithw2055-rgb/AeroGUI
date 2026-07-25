#include "GalleryRuntime.hpp"
#include "StatusBadge.hpp"

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Virtualization.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Markup/XamlCompiledDocument.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlTheme.hpp>
#include <Aero/Markup/XamlVisualTree.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>
#include <Aero/Presentation/Binding.hpp>

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Controls;
using namespace Core;
using namespace Markup;
using namespace Presentation;

Status Failure(const char* message) noexcept {
    return Status::Failure(
        ErrorCode::InvalidState, message);
}

bool ReadFile(
    const std::string& path,
    std::vector<std::uint8_t>& output) {
    std::ifstream stream(
        path, std::ios::binary);
    if (!stream) {
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff length =
        stream.tellg();
    if (length < 0 ||
        static_cast<std::uint64_t>(length) >
            UINT32_MAX) {
        return false;
    }
    stream.seekg(0, std::ios::beg);
    output.resize(
        static_cast<std::size_t>(length));
    return output.empty() ||
        static_cast<bool>(
            stream.read(
                reinterpret_cast<char*>(
                    output.data()),
                static_cast<std::streamsize>(
                    length)));
}

class GalleryItem final : public Object {
    AERO_TYPED_META_NAMED(
        GalleryItem,
        Object,
        "urn:aero-control-gallery",
        "GalleryItem")
public:
    GalleryItem() noexcept = default;
};

class GalleryItemsSource final
    : public IItemsSource {
public:
    Result<void> Initialize() noexcept {
        Result<Ref<GalleryItem>> made =
            MakeRef<GalleryItem>();
        if (!made) {
            return made.GetStatus();
        }
        item_ = Ref<Object>(
            std::move(made).Value());
        return {};
    }

    std::uint32_t Count()
        const noexcept override {
        return 10000U;
    }

    Ref<Object> ItemAt(
        std::uint32_t index)
        const noexcept override {
        return index < Count()
            ? item_
            : Ref<Object>();
    }

    Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler)
        noexcept override {
        return changed_.TryAdd(handler);
    }

    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler)
        noexcept override {
        return changed_.Remove(handler);
    }

private:
    Ref<Object> item_;
    ItemsChangedHandler changed_;
};

Result<Ref<Object>> MakeVirtualizedItem(
    const Ref<Object>&,
    void*) noexcept {
    Result<Ref<TextBlock>> made =
        MakeRef<TextBlock>();
    if (!made) {
        return made.GetStatus();
    }
    Ref<TextBlock> text =
        std::move(made).Value();
    Result<void> configured =
        text->SetText(
            "Virtualized gallery item");
    if (configured) {
        configured = text->SetHeight(24.0);
    }
    if (configured) {
        configured = text->SetWidth(340.0);
    }
    if (!configured) {
        return configured.GetStatus();
    }
    return Ref<Object>(std::move(text));
}

template <typename T>
T* FindNamed(
    XamlObjectWriter& writer,
    StringView name) noexcept {
    Object* object =
        writer.DocumentNameScope().Find(name);
    return object != nullptr &&
        object->RuntimeType() ==
            T::StaticTypeId()
        ? static_cast<T*>(object)
        : nullptr;
}

} // namespace

struct GalleryRuntime::Impl final {
    Dispatcher dispatcher;
    MetadataDomain metadata;
    XamlModuleCatalog modules;
    std::unique_ptr<MetadataRuntime>
        runtime;
    std::unique_ptr<ObjectServicesScope>
        objectServices;
    std::unique_ptr<EffectiveValueEngine>
        values;
    std::unique_ptr<ObjectTree> tree;
    std::unique_ptr<LayoutManager> layout;
    NullRenderBackend nullBackend;
    std::unique_ptr<RenderManager> renderer;
    std::unique_ptr<XamlSchemaContext>
        schema;
    std::unique_ptr<
        XamlActivationProviderRegistry>
        activation;
    std::unique_ptr<
        XamlDependencyPropertyBridge>
        dependencyProperties;
    std::unique_ptr<XamlVisualTreeHost>
        visualTree;
    std::unique_ptr<XamlObjectWriter> writer;
    BindingManager bindings{dispatcher};
    std::unique_ptr<TemplateManager>
        templates;
    std::unique_ptr<XamlTheme> theme;
    Base::Ref<Base::Object> root;
    Base::Vector<Control*> themedControls;
    GalleryItemsSource items;
    DataTemplate itemTemplate{
        &MakeVirtualizedItem, nullptr};
    std::unique_ptr<ItemContainerGenerator>
        generator;
    ListBox* listBox = nullptr;
    VirtualizingStackPanel*
        virtualizingPanel = nullptr;
    GallerySnapshot snapshot;
    bool mounted = false;
    bool bindingsInitialized = false;

    ~Impl() {
        Cleanup();
    }

    XamlActivationContext
    ActivationContext() noexcept {
        XamlActivationContext context =
            XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties =
            &metadata.DependencyProperties();
        return context;
    }

    Result<void> LoadDocument(
        const std::string& assetDirectory,
        GalleryLoadMode mode) noexcept {
        const std::string sourcePath =
            assetDirectory +
            "/ControlGallery.xaml";
        std::vector<std::uint8_t> source;
        if (mode == GalleryLoadMode::Runtime) {
            if (!ReadFile(sourcePath, source)) {
                return Failure(
                    "ControlGallery runtime XAML is unavailable");
            }
            DiagnosticBag diagnostics;
            Utf8XmlTokenizer tokenizer;
            Result<void> reset = tokenizer.Reset({
                reinterpret_cast<const char*>(
                    source.data()),
                static_cast<std::uint32_t>(
                    source.size())},
                &diagnostics);
            if (!reset) {
                return reset.GetStatus();
            }
            XamlNodeReader reader(
                tokenizer, &diagnostics);
            Result<Ref<Object>> loaded =
                LoadXamlVisualTreeWithActivation(
                    *visualTree,
                    *writer,
                    reader,
                    *activation,
                    ActivationContext());
            if (!loaded) {
                return loaded.GetStatus();
            }
            root = std::move(loaded).Value();
            return {};
        }

        const std::string compiledPath =
            assetDirectory +
            "/ControlGallery.axir";
        if (!ReadFile(compiledPath, source)) {
            return Failure(
                "ControlGallery compiled XAML is unavailable");
        }
        Result<XamlCompiledDocument> document =
            XamlCompiledDocument::Deserialize(
                {source.data(),
                 static_cast<std::uint32_t>(
                     source.size())},
                metadata,
                {},
                modules.ManifestHash());
        if (!document) {
            return document.GetStatus();
        }
        Result<Ref<Object>> loaded =
            LoadXamlVisualTreeWithActivation(
                *visualTree,
                *writer,
                document.Value(),
                *activation,
                ActivationContext());
        if (!loaded) {
            return loaded.GetStatus();
        }
        root = std::move(loaded).Value();
        return {};
    }

    Result<void> LoadTheme(
        const std::string& assetDirectory,
        GalleryTheme requested) noexcept {
        std::vector<std::uint8_t> generic;
        std::vector<std::uint8_t> palette;
        if (!ReadFile(
                assetDirectory +
                    "/themes/Generic.xaml",
                generic) ||
            !ReadFile(
                assetDirectory +
                    (requested ==
                         GalleryTheme::Light
                     ? "/themes/Light.xaml"
                     : "/themes/Dark.xaml"),
                palette)) {
            return Failure(
                "ControlGallery theme assets are unavailable");
        }
        Result<std::unique_ptr<XamlTheme>>
            loaded = XamlTheme::Load(
                {reinterpret_cast<const char*>(
                     generic.data()),
                 static_cast<std::uint32_t>(
                     generic.size())},
                {reinterpret_cast<const char*>(
                     palette.data()),
                 static_cast<std::uint32_t>(
                     palette.size())},
                metadata.DependencyProperties());
        if (!loaded) {
            return loaded.GetStatus();
        }
        theme = std::move(loaded).Value();
        return {};
    }

    Result<void> ApplyTheme() noexcept {
        const StringView names[] = {
            "PrimaryButton",
            "FeatureCheck",
            "LightChoice",
            "DarkChoice",
            "BigList"};
        for (StringView name : names) {
            Object* object =
                writer->DocumentNameScope().
                    Find(name);
            if (object == nullptr ||
                !metadata.Descriptors().
                    IsDerivedFrom(
                        object->RuntimeType(),
                        Control::StaticTypeId())) {
                return Failure(
                    "ControlGallery themed control is missing");
            }
            auto* control =
                static_cast<Control*>(object);
            if (theme->FindTemplate(
                    control->RuntimeType()) ==
                nullptr) {
                continue;
            }
            Result<TemplateHandle> applied =
                theme->Apply(
                    *templates, *control);
            if (!applied) {
                return applied.GetStatus();
            }
            Result<void> tracked =
                themedControls.TryPushBack(
                    control);
            if (!tracked) {
                return tracked.GetStatus();
            }
        }
        return {};
    }

    Result<void> ConfigureBinding() noexcept {
        TextBox* input =
            FindNamed<TextBox>(
                *writer, "Input");
        TextBlock* mirror =
            FindNamed<TextBlock>(
                *writer, "BindingMirror");
        if (input == nullptr ||
            mirror == nullptr) {
            return Failure(
                "ControlGallery binding endpoints are missing");
        }
        BindingDescriptor descriptor;
        descriptor.source = input;
        descriptor.sourceProperty =
            TextBox::TextProperty;
        descriptor.target = mirror;
        descriptor.targetProperty =
            TextBlock::TextProperty;
        descriptor.mode = BindingMode::OneWay;
        Result<BindingHandle> attached =
            bindings.Attach(descriptor);
        if (!attached) {
            return attached.GetStatus();
        }
        Result<void> changed =
            input->SetText(
                "Binding validation passed");
        if (!changed) {
            return changed.GetStatus();
        }
        Result<std::uint32_t> flushed =
            bindings.Flush();
        if (!flushed) {
            return flushed.GetStatus();
        }
        if (mirror->Text() != input->Text()) {
            return Failure(
                "ControlGallery binding validation failed");
        }
        return {};
    }

    Result<void> ConfigureVirtualization() noexcept {
        listBox = FindNamed<ListBox>(
            *writer, "BigList");
        virtualizingPanel =
            FindNamed<VirtualizingStackPanel>(
                *writer, "BigListPanel");
        if (listBox == nullptr ||
            virtualizingPanel == nullptr) {
            return Failure(
                "ControlGallery virtualization endpoints are missing");
        }
        Result<void> initialized =
            items.Initialize();
        if (!initialized) {
            return initialized.GetStatus();
        }
        listBox->SetItemTemplate(
            &itemTemplate);
        Result<void> source =
            listBox->SetItemsSource(&items);
        if (!source) {
            return source.GetStatus();
        }
        Result<void> extent =
            virtualizingPanel->
                SetEstimatedItemExtent(24.0);
        if (extent) {
            extent =
                virtualizingPanel->
                    SetOverscanCount(2U);
        }
        if (!extent) {
            return extent.GetStatus();
        }
        Result<bool> viewport =
            virtualizingPanel->SetViewport(
                {360.0, 120.0});
        if (!viewport) {
            return viewport.GetStatus();
        }
        generator =
            std::make_unique<
                ItemContainerGenerator>(
                *tree,
                *layout,
                *values,
                nullptr,
                renderer.get());
        Result<void> attached =
            generator->AttachVirtualized(
                *listBox,
                *virtualizingPanel);
        if (!attached) {
            return attached.GetStatus();
        }
        Result<bool> scrolled =
            virtualizingPanel->
                SetVerticalOffset(4800.0);
        if (!scrolled) {
            return scrolled.GetStatus();
        }
        return {};
    }

    Result<void> RunFrame() noexcept {
        const DispatcherFramePhase phases[] = {
            DispatcherFramePhase::
                PropertyChanges,
            DispatcherFramePhase::DataBind,
            DispatcherFramePhase::Lifecycle,
            DispatcherFramePhase::Layout,
            DispatcherFramePhase::
                RenderCommit};
        for (DispatcherFramePhase phase :
             phases) {
            Result<std::uint32_t> ran =
                dispatcher.RunFramePhase(
                    phase);
            if (!ran) {
                return ran.GetStatus();
            }
        }
        if (renderer->CurrentPlan().
                Nodes().Empty()) {
            Result<std::uint32_t> committed =
                renderer->Commit();
            if (!committed) {
                return committed.GetStatus();
            }
        }
        return {};
    }

    void Cleanup() noexcept {
        if (generator) {
            static_cast<void>(
                generator->Detach());
            generator.reset();
        }
        if (listBox != nullptr) {
            static_cast<void>(
                listBox->SetItemsSource(
                    nullptr));
            listBox->SetItemTemplate(nullptr);
        }
        listBox = nullptr;
        virtualizingPanel = nullptr;
        if (bindingsInitialized) {
            bindings.Shutdown();
            bindingsInitialized = false;
        }
        if (templates) {
            for (Control* control :
                 themedControls) {
                if (control != nullptr) {
                    static_cast<void>(
                        templates->Clear(
                            *control));
                }
            }
        }
        themedControls.Clear();
        if (visualTree && mounted) {
            static_cast<void>(
                visualTree->Unmount());
            mounted = false;
        } else if (visualTree) {
            static_cast<void>(
                visualTree->
                    DiscardStaged());
        }
        root = Ref<Object>();
    }
};

GalleryRuntime::GalleryRuntime() noexcept =
    default;

GalleryRuntime::~GalleryRuntime() {
    Shutdown();
}

Result<void> GalleryRuntime::Initialize(
    StringView assetDirectory,
    GalleryLoadMode loadMode,
    GalleryTheme requestedTheme) noexcept {
    if (impl_) {
        return Failure(
            "ControlGallery runtime is already initialized");
    }
    if (assetDirectory.Empty()) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "ControlGallery asset directory is empty");
    }
    std::unique_ptr<Impl> state =
        std::make_unique<Impl>();
    Result<void> status =
        state->modules.TryAdd(
            MakeStatusBadgeModuleManifest());
    if (status) {
        status =
            state->modules.RegisterMetadata(
                state->metadata);
    }
    if (status) {
        status = state->metadata.Seal();
    }
    if (!status) {
        return status.GetStatus();
    }

    state->runtime =
        std::make_unique<MetadataRuntime>(
            state->metadata);
    state->objectServices =
        std::make_unique<ObjectServicesScope>(
            state->dispatcher,
            state->metadata.
                DependencyProperties(),
            *state->runtime);
    state->values =
        std::make_unique<EffectiveValueEngine>(
            state->dispatcher,
            state->metadata.
                DependencyProperties());
    status = state->values->Initialize();
    if (!status) {
        return status.GetStatus();
    }
    state->tree =
        std::make_unique<ObjectTree>(
            state->dispatcher,
            *state->values);
    status = state->tree->Initialize();
    if (!status) {
        return status.GetStatus();
    }
    state->layout =
        std::make_unique<LayoutManager>(
            state->dispatcher);
    status = state->layout->Initialize();
    if (!status) {
        return status.GetStatus();
    }
    state->renderer =
        std::make_unique<RenderManager>(
            state->dispatcher,
            state->nullBackend);
    status = state->renderer->Initialize();
    if (!status) {
        return status.GetStatus();
    }
    status = state->bindings.Initialize();
    if (!status) {
        return status.GetStatus();
    }
    state->bindingsInitialized = true;
    state->templates =
        std::make_unique<TemplateManager>(
            *state->tree,
            *state->values,
            state->metadata.
                DependencyProperties(),
            state->layout.get(),
            state->renderer.get());
    state->schema =
        std::make_unique<XamlSchemaContext>(
            state->metadata,
            *state->runtime);
    state->activation =
        std::make_unique<
            XamlActivationProviderRegistry>(
                *state->schema);
    state->dependencyProperties =
        std::make_unique<
            XamlDependencyPropertyBridge>(
                *state->schema,
                state->metadata.
                    DependencyProperties());
    state->visualTree =
        std::make_unique<XamlVisualTreeHost>(
            *state->tree,
            *state->layout,
            *state->values,
            state->renderer.get());
    Result<std::uint32_t> presentation =
        TryRegisterAeroPresentationXaml(
            *state->dependencyProperties,
            *state->activation,
            state->visualTree.get());
    if (!presentation) {
        return presentation.GetStatus();
    }
    status = state->modules.ConfigureXaml(
        *state->schema,
        *state->activation);
    if (status) {
        status = state->runtime->Freeze();
    }
    if (status) {
        status = state->schema->Freeze();
    }
    if (status) {
        status = state->activation->Freeze();
    }
    if (!status) {
        return status.GetStatus();
    }
    state->writer =
        std::make_unique<XamlObjectWriter>(
            *state->schema);

    const std::string assetPath(
        assetDirectory.Data(),
        assetDirectory.SizeBytes());
    status = state->LoadTheme(
        assetPath, requestedTheme);
    if (status) {
        status = state->LoadDocument(
            assetPath, loadMode);
    }
    if (!status) {
        return status.GetStatus();
    }
    if (!state->root ||
        state->root->RuntimeType() !=
            Border::StaticTypeId()) {
        return Failure(
            "ControlGallery root is not a Border");
    }
    status = state->visualTree->Mount(
        *state->root,
        state->root->RuntimeType(),
        {900.0, 640.0});
    if (!status) {
        return status.GetStatus();
    }
    state->mounted = true;
    status = state->ApplyTheme();
    if (status) {
        status = state->ConfigureBinding();
    }
    if (status) {
        status =
            state->ConfigureVirtualization();
    }
    if (status) {
        status = state->RunFrame();
    }
    if (!status) {
        return status.GetStatus();
    }

    const RenderPlan& plan =
        state->renderer->CurrentPlan();
    state->snapshot.planHash =
        plan.StableHash();
    state->snapshot.nodeCount =
        plan.Nodes().Size();
    state->snapshot.commandCount =
        plan.Commands().Size();
    state->snapshot.namedObjectCount =
        state->writer->
            DocumentNameScope().Size();
    state->snapshot.itemCount =
        state->items.Count();
    state->snapshot.realizedItemCount =
        state->generator->
            GeneratedCount();
    state->snapshot.createdContainerCount =
        state->generator->
            CreatedContainerCount();
    state->snapshot.loadMode = loadMode;
    state->snapshot.theme =
        requestedTheme;
    if (state->snapshot.nodeCount == 0U ||
        state->snapshot.commandCount == 0U ||
        state->snapshot.namedObjectCount <
            10U ||
        state->snapshot.itemCount !=
            10000U ||
        state->snapshot.realizedItemCount ==
            0U ||
        state->snapshot.realizedItemCount >=
            state->snapshot.itemCount ||
        state->snapshot.createdContainerCount >
            16U) {
        std::fprintf(
            stderr,
            "ControlGallery metrics: "
            "nodes=%u commands=%u names=%u "
            "items=%u realized=%u created=%u\n",
            state->snapshot.nodeCount,
            state->snapshot.commandCount,
            state->snapshot.namedObjectCount,
            state->snapshot.itemCount,
            state->snapshot.realizedItemCount,
            state->snapshot.createdContainerCount);
        return Failure(
            "ControlGallery acceptance metrics are invalid");
    }

    impl_ = std::move(state);
    return {};
}

void GalleryRuntime::Shutdown() noexcept {
    impl_.reset();
}

const GallerySnapshot&
GalleryRuntime::Snapshot() const noexcept {
    static const GallerySnapshot empty;
    return impl_
        ? impl_->snapshot
        : empty;
}

const RenderPlan&
GalleryRuntime::Plan() const noexcept {
    static const RenderPlan empty;
    return impl_
        ? impl_->renderer->CurrentPlan()
        : empty;
}

} // namespace Aero::Samples::ControlGallery
