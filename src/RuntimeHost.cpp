#include <Aero/RuntimeHost.hpp>
#include <Aero/SchemaBundle.hpp>

#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Markup/Runtime/XamlActivation.hpp>
#include <Aero/Markup/Runtime/XamlLoader.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Platform/Clipboard.hpp>
#include <Aero/Platform/Ime.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/Commands.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Resources.hpp>
#include <Aero/Presentation/Style.hpp>
#include <Aero/Presentation/VisualTreeMount.hpp>
#include <Aero/BuiltinThemes.generated.hpp>

#include "runtime/RuntimePresentationServices.hpp"

#include <new>
#include <utility>

namespace Aero {
using namespace Markup;
namespace {

Base::Status RuntimeInvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status RuntimeNotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

Base::Result<Base::ResourceUri> BuiltInThemeUri(
    Base::StringView name) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.TryAssign(
        Base::StringView(
            "pack://application:,,,/Aero.Themes;component/"));
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> appended = text.TryAppend(name);
    if (!appended) return appended.GetStatus();
    return Base::ResourceUri::Parse(text.View());
}

template<class T, class... TArgs>
Base::Result<void> CreateRuntimeObject(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& output,
    TArgs&&... arguments) noexcept {
    if (output != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Runtime service is already allocated");
    }
    void* memory = allocator.Allocate({
        sizeof(T), alignof(T), tag});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Runtime service allocation failed");
    }
    output = new (memory) T(
        std::forward<TArgs>(arguments)...);
    return {};
}

template<class T>
void DestroyRuntimeObject(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& object) noexcept {
    if (object == nullptr) return;
    object->~T();
    allocator.Deallocate(
        object, sizeof(T), alignof(T), tag);
    object = nullptr;
}

} // namespace

struct RuntimeHost::Impl final {
    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value), schemaBundle(&value) {}

    Base::IAllocator* allocator = nullptr;
    Core::Dispatcher dispatcher;
    SchemaBundle schemaBundle;
    Core::MetadataDomain* metadata = nullptr;
    ModuleCatalog modules;
    RuntimeHostOptions options;
    Presentation::NullRenderBackend nullBackend;

    Core::MetadataRuntime* metadataRuntime = nullptr;
    Core::ObjectServicesScope* objectServices = nullptr;
    Core::EffectiveValueEngine* values = nullptr;
    Presentation::ObjectTree* tree = nullptr;
    Presentation::LayoutManager* layout = nullptr;
    Presentation::RenderManager* renderer = nullptr;
    Presentation::BindingManager* bindings = nullptr;
    Presentation::RoutedEventManager* events = nullptr;
    Presentation::CommandManager* commands = nullptr;
    Controls::TemplateManager* templates = nullptr;
    Controls::VisualStateManager* visualStates = nullptr;
    Presentation::StyleManager* styles = nullptr;
    Detail::RuntimePresentationServices presentationServices;

    XamlSchemaContext* schema = nullptr;
    Core::ActivationProviderRegistry* activation = nullptr;
    Presentation::VisualTreeMount* visualMount = nullptr;
    XamlSourceProviderRegistry xamlSources;
    EmbeddedXamlSourceProvider embeddedXaml;
    FileXamlSourceProvider fileXaml;
    Presentation::ResourceDictionary applicationResources;
    Presentation::ResourceDictionary themeResources;
    Presentation::ResourceDictionary systemResources;
    Presentation::ResourceDictionary dynamicResourceEnvironment;

    Presentation::HitTestManager hitTests;
    Presentation::FocusManager* focus = nullptr;
    Presentation::PointerInputManager* pointer = nullptr;
    Presentation::KeyboardInputManager* keyboard = nullptr;
    Presentation::TextInputManager* textInput = nullptr;
    Controls::ControlInteractionManager* controlInteractions = nullptr;
    Controls::TextBoxInteractionManager* textBoxInteractions = nullptr;

    XamlLoadResult loadedDocument;
    Base::Ref<Base::Object> root;
    std::uint64_t frameNumber = 0U;
    bool initialized = false;
    bool mounted = false;
    bool terminal = false;

    Presentation::IRenderBackend& SelectedBackend() noexcept {
        return options.renderBackend != nullptr
            ? *options.renderBackend
            : static_cast<Presentation::IRenderBackend&>(nullBackend);
    }

    XamlActivationContext ActivationContext() noexcept {
        XamlActivationContext context =
            XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties =
            &metadata->DependencyProperties();
        context.applicationServices =
            options.applicationServices;
        context.hostContext = options.hostContext;
        return context;
    }

    Base::Result<void> EnsureDefaultXamlProviders() noexcept {
        Base::Result<Base::ResourceUri> light =
            BuiltInThemeUri(Base::StringView("Light.xaml"));
        if (!light) return light.GetStatus();
        Base::Result<void> status = embeddedXaml.TryAdd(
            light.Value(),
            {Detail::AeroThemeLightSource,
             static_cast<std::uint32_t>(
                 sizeof(Detail::AeroThemeLightSource))});
        if (!status) return status.GetStatus();
        Base::Result<Base::ResourceUri> dark =
            BuiltInThemeUri(Base::StringView("Dark.xaml"));
        if (!dark) return dark.GetStatus();
        status = embeddedXaml.TryAdd(
            dark.Value(),
            {Detail::AeroThemeDarkSource,
             static_cast<std::uint32_t>(
                 sizeof(Detail::AeroThemeDarkSource))});
        if (!status) return status.GetStatus();
        Base::Result<Base::ResourceUri> generic =
            BuiltInThemeUri(Base::StringView("Generic.xaml"));
        if (!generic) return generic.GetStatus();
        status = embeddedXaml.TryAdd(
            generic.Value(),
            {Detail::AeroThemeGenericSource,
             static_cast<std::uint32_t>(
                 sizeof(Detail::AeroThemeGenericSource))});
        if (!status) return status.GetStatus();

        status = xamlSources.TryRegister(
                embeddedXaml, Base::StringView("pack"));
        if (!status &&
            status.GetStatus().code !=
                Base::ErrorCode::AlreadyExists) {
            return status.GetStatus();
        }
        status = xamlSources.TryRegister(
            fileXaml, Base::StringView("file"));
        if (!status &&
            status.GetStatus().code !=
                Base::ErrorCode::AlreadyExists) {
            return status.GetStatus();
        }
        status = xamlSources.TryRegister(fileXaml);
        if (!status &&
            status.GetStatus().code !=
                Base::ErrorCode::AlreadyExists) {
            return status.GetStatus();
        }
        return {};
    }

    Base::Result<void> BeginDocumentLoad() noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "RuntimeHost must be initialized before XAML loading");
        }
        if (mounted || root || loadedDocument.root) {
            return RuntimeInvalidState(
                "RuntimeHost already owns a loaded document");
        }
        return {};
    }

    Base::Result<Base::Ref<Base::Object>> CompleteDocumentLoad(
        Base::Result<XamlLoadResult> loaded) noexcept {
        if (!loaded) {
            return loaded.GetStatus();
        }
        XamlLoadResult result = std::move(loaded).Value();
        loadedDocument = std::move(result);
        return loadedDocument.root;
    }

    Base::Result<XamlLoadOptions> LoadOptions(
        XamlActivationContext& context) noexcept {
        XamlLoadOptions result;
        result.resources = &dynamicResourceEnvironment;
        result.effectiveValues = values;
        result.bindings = bindings;
        result.fallbackResources = &dynamicResourceEnvironment;
        result.activationFacets =
            activation;
        result.activation = &context;
        return result;
    }

    void ClearLoadedDocument() noexcept {
        loadedDocument.Clear();
    }

    Presentation::ResourceEnvironment ResourceEnvironment() const noexcept {
        return {
            &applicationResources,
            &themeResources,
            &systemResources};
    }

    Base::Result<Presentation::ResourceDictionary*>
    ResolveResourceLayer(
        RuntimeResourceLayer layer) noexcept {
        switch (layer) {
        case RuntimeResourceLayer::Application:
            return &applicationResources;
        case RuntimeResourceLayer::Theme:
            return &themeResources;
        case RuntimeResourceLayer::System:
            return &systemResources;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "RuntimeHost resource layer is invalid");
    }

    Base::Result<void> RebuildDynamicResourceEnvironment() noexcept {
        Base::Result<void> rebuilt =
            dynamicResourceEnvironment.Clear();
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.TryAddMerged(
                    systemResources);
        }
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.TryAddMerged(
                    themeResources);
        }
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.TryAddMerged(
                    applicationResources);
        }
        return rebuilt;
    }

    void DetachRuntimePresentation() noexcept {
        presentationServices.Detach(
            RootVisual(),
            {loadedDocument.visualContent.nodes.Data(),
             loadedDocument.visualContent.nodes.Size()});
    }

    Presentation::Visual* RootVisual() noexcept {
        if (!root) return nullptr;
        if (!metadata->Descriptors().IsDerivedFrom(
                root->RuntimeType(),
                Presentation::Visual::StaticTypeId())) {
            return nullptr;
        }
        return static_cast<Presentation::Visual*>(root.Get());
    }

    Base::Result<Presentation::Visual*> ResolveVisual(
        Base::Object& object, Core::TypeId type) noexcept {
        if (object.RuntimeType() != type ||
            !metadata->Descriptors().IsDerivedFrom(
                type, Presentation::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RuntimeHost root is not a registered Visual");
        }
        return static_cast<Presentation::Visual*>(&object);
    }

    Base::Result<Presentation::UIElement*> ResolveUIElement(
        Base::Object& object, Core::TypeId type) noexcept {
        Base::Result<Presentation::Visual*> visual =
            ResolveVisual(object, type);
        if (!visual) return visual.GetStatus();
        Presentation::UIElement* element =
            visual.Value()->AsUIElement();
        if (element == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RuntimeHost root is not a UIElement");
        }
        return element;
    }

    Presentation::FrameworkElement* ResolveFrameworkElement(
        Base::Object& object, Core::TypeId type) noexcept {
        Base::Result<Presentation::Visual*> visual =
            ResolveVisual(object, type);
        return visual ? visual.Value()->AsFrameworkElement() : nullptr;
    }

    Base::Result<void> CreateTemplateServices() noexcept {
        Base::Result<void> status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            templates, *tree, *values,
            metadata->DependencyProperties(), layout, renderer);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            visualStates, *values, *templates);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            styles, *values,
            metadata->DependencyProperties());
        if (!status) return status.GetStatus();
        presentationServices.Configure(
            *allocator,
            *metadata,
            *values,
            *styles,
            *templates,
            *visualStates,
            ResourceEnvironment());
        return {};
    }

    void DestroyTemplateServices() noexcept {
        presentationServices.Reset();
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            styles);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            visualStates);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            templates);
    }

    Base::Result<void> VisitAndAttach(
        Presentation::Visual& rootVisual) noexcept {
        Base::Vector<Presentation::Visual*> stack(allocator);
        Base::Result<void> pushed =
            stack.TryPushBack(&rootVisual);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Presentation::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Core::TypeId type = node->RuntimeType();
            if (controlInteractions != nullptr &&
                metadata->Descriptors().IsDerivedFrom(
                    type, Controls::ButtonBase::StaticTypeId())) {
                Base::Result<void> attached =
                    controlInteractions->Attach(
                        *static_cast<Controls::ButtonBase*>(node));
                if (!attached) return attached.GetStatus();
            }
            if (metadata->Descriptors().IsDerivedFrom(
                    type, Controls::TextBox::StaticTypeId())) {
                auto& textBox =
                    *static_cast<Controls::TextBox*>(node);
                if (options.textInputMethodHost != nullptr) {
                    Base::Result<void> hosted =
                        textBox.SetInputMethodHost(
                            options.textInputMethodHost);
                    if (!hosted) return hosted.GetStatus();
                }
                if (textBoxInteractions != nullptr) {
                    Base::Result<void> attached =
                        textBoxInteractions->Attach(textBox);
                    if (!attached) return attached.GetStatus();
                }
            }
            const Base::Span<Presentation::Visual* const>
                children = node->VisualChildren();
            for (std::uint32_t index = 0U;
                 index < children.Size(); ++index) {
                pushed = stack.TryPushBack(children[index]);
                if (!pushed) return pushed.GetStatus();
            }
        }
        return {};
    }

    void ClearTextInputHosts(
        Presentation::Visual* node) noexcept {
        if (node == nullptr) return;
        if (metadata->Descriptors().IsDerivedFrom(
                node->RuntimeType(),
                Controls::TextBox::StaticTypeId())) {
            static_cast<void>(
                static_cast<Controls::TextBox*>(node)->
                    SetInputMethodHost(nullptr));
        }
        for (Presentation::Visual* child :
             node->VisualChildren()) {
            ClearTextInputHosts(child);
        }
    }

    void DestroyInteractions() noexcept {
        ClearTextInputHosts(RootVisual());
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            textBoxInteractions);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            controlInteractions);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            textInput);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            keyboard);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            pointer);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            focus);
    }

    Base::Result<void> CreateInteractions() noexcept {
        Presentation::Visual* rootVisual = RootVisual();
        if (rootVisual == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Runtime root is not a registered Visual");
        }
        Base::Result<void> status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            focus, *tree, *events);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            pointer, hitTests, *events, *rootVisual);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            keyboard, *focus, *events, *tree, commands);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            textInput, *focus, *events, *tree);
        if (!status) return status.GetStatus();

        if (options.attachControlInteractions) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                controlInteractions,
                *tree, *events, *pointer, *focus,
                *commands, visualStates);
            if (!status) return status.GetStatus();
            status = controlInteractions->Initialize();
            if (!status) return status.GetStatus();
        }
        if (options.attachTextEditing &&
            options.clipboard != nullptr) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                textBoxInteractions,
                *tree, *events, *pointer, *focus,
                *options.clipboard);
            if (!status) return status.GetStatus();
        }
        status = VisitAndAttach(*rootVisual);
        if (!status) {
            DestroyInteractions();
            return status.GetStatus();
        }
        return {};
    }

    void ShutdownServices() noexcept {
        DestroyInteractions();
        DetachRuntimePresentation();
        DestroyTemplateServices();
        if (visualMount != nullptr && visualMount->IsMounted()) {
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
        }
        mounted = false;
        root.Reset();
        ClearLoadedDocument();

        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation, visualMount);

        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            commands);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            events);
        if (bindings != nullptr) bindings->Shutdown();
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            bindings);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            renderer);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            layout);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            tree);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            values);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            objectServices);
        activation = nullptr;
        schema = nullptr;
        metadataRuntime = nullptr;
        metadata = nullptr;
        initialized = false;
    }

    Base::Result<void> InitializeServices(
        const RuntimeHostOptions& requested) noexcept {
        if (initialized) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "RuntimeHost is already initialized");
        }
        if (terminal) {
            return RuntimeInvalidState(
                "RuntimeHost cannot be restarted after shutdown or failed startup");
        }
        options = requested;

        Base::Result<void> status =
            EnsureDefaultXamlProviders();
        if (status) status = schemaBundle.Prepare(modules);
        if (!status) {
            terminal = true;
            return status.GetStatus();
        }
        metadata = &schemaBundle.Metadata();
        metadataRuntime = &schemaBundle.Runtime();

        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                objectServices, dispatcher,
                metadata->DependencyProperties(),
                *metadataRuntime);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                values, dispatcher,
                metadata->DependencyProperties());
        }
        if (status) status = values->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                tree, dispatcher, *values);
        }
        if (status) status = tree->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                layout, dispatcher);
        }
        if (status) status = layout->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                renderer, dispatcher, SelectedBackend());
        }
        if (status) status = renderer->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                bindings, dispatcher);
        }
        if (status) status = bindings->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                events, metadata->RoutedEvents());
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                commands, *tree);
        }
        if (status) status = CreateTemplateServices();
        if (status) {
            status = RebuildDynamicResourceEnvironment();
        }
        if (status) {
            status = schemaBundle.Finalize(
                modules,
                SchemaBundleServices{allocator});
        }
        if (status) {
            schema = &schemaBundle.XamlSchema();
            activation = &schemaBundle.ActivationFacets();
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                visualMount, *tree, *layout, renderer);
        }
        if (!status) {
            ShutdownServices();
            terminal = true;
            return status.GetStatus();
        }
        initialized = true;
        return {};
    }

    Base::Result<void> CommitResourceLayer(
        XamlLoadResult result,
        Presentation::ResourceDictionary& target,
        bool merge) noexcept {
        if (!result.root ||
            result.root->RuntimeType() !=
                Presentation::ResourceDictionary::StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RuntimeHost resource document root must be ResourceDictionary");
        }
        auto& dictionary =
            static_cast<Presentation::ResourceDictionary&>(
                *result.root);
        if (merge) {
            Base::Result<void> merged =
                target.TryAddMerged(dictionary);
            if (!merged) return merged.GetStatus();
            Base::Result<void> rebuilt =
                RebuildDynamicResourceEnvironment();
            if (rebuilt) return {};
            Base::Result<bool> removed =
                target.RemoveMerged(dictionary);
            Base::Result<void> restored =
                removed && removed.Value()
                ? RebuildDynamicResourceEnvironment()
                : Base::Result<void>(
                      removed
                      ? Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "RuntimeHost resource merge rollback lost its dictionary")
                      : removed.GetStatus());
            return restored
                ? Base::Result<void>(rebuilt.GetStatus())
                : restored;
        }

        Presentation::ResourceDictionary previous =
            std::move(target);
        target = std::move(dictionary);
        Base::Result<void> rebuilt =
            RebuildDynamicResourceEnvironment();
        if (rebuilt) return {};
        target = std::move(previous);
        Base::Result<void> restored =
            RebuildDynamicResourceEnvironment();
        return restored
            ? Base::Result<void>(rebuilt.GetStatus())
            : restored;
    }

    Base::Result<void> LoadResourceLayer(
        Base::StringView uri,
        Presentation::ResourceDictionary& target,
        Core::IDiagnosticSink* diagnostics,
        bool merge = false) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "RuntimeHost must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return RuntimeInvalidState(
                "RuntimeHost resource layers must be loaded before a document");
        }
        XamlActivationContext activationContext =
            ActivationContext();
        Base::Result<XamlLoadOptions> loadOptions =
            LoadOptions(activationContext);
        if (!loadOptions) {
            return loadOptions.GetStatus();
        }
        XamlLoader loader(
            *schema,
            xamlSources,
            diagnostics);
        Base::Result<XamlLoadResult> loaded =
            loader.Load(uri, loadOptions.Value());
        if (!loaded) {
            return loaded.GetStatus();
        }
        return CommitResourceLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

    Base::Result<void> LoadCompiledResourceLayer(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        Presentation::ResourceDictionary& target,
        bool merge = false) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "RuntimeHost must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return RuntimeInvalidState(
                "RuntimeHost resource layers must be loaded before a document");
        }
        XamlActivationContext activationContext =
            ActivationContext();
        Base::Result<XamlLoadOptions> loadOptions =
            LoadOptions(activationContext);
        if (!loadOptions) return loadOptions.GetStatus();
        XamlLoader loader(*schema, xamlSources);
        Base::Result<XamlLoadResult> loaded =
            loader.LoadCompiled(
                bytes, originUri, loadOptions.Value());
        if (!loaded) return loaded.GetStatus();

        return CommitResourceLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

    Base::Result<void> MountRoot(
        Base::Ref<Base::Object> requestedRoot,
        Presentation::Size availableSize) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "RuntimeHost must be initialized before mounting");
        }
        if (mounted || root) {
            return RuntimeInvalidState(
                "RuntimeHost already has a mounted root");
        }
        if (!requestedRoot) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RuntimeHost root must not be null");
        }
        if (loadedDocument.root &&
            loadedDocument.root.Get() != requestedRoot.Get()) {
            return RuntimeInvalidState(
                "Mounted root does not match the staged XAML document");
        }
        if (!metadata->Descriptors().IsDerivedFrom(
                requestedRoot->RuntimeType(),
                Presentation::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RuntimeHost root must derive from Visual");
        }
        Base::Result<Presentation::Visual*> rootVisual =
            ResolveVisual(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        Base::Result<Presentation::UIElement*> rootLayout =
            ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootLayout) return rootLayout.GetStatus();
        Base::Result<void> rootTracked =
            loadedDocument.visualContent.TryAddNode(*rootVisual.Value());
        if (!rootTracked) return rootTracked.GetStatus();
        if (visualMount == nullptr) {
            return RuntimeNotInitialized(
                "RuntimeHost visual mount service is unavailable");
        }
        Base::Result<void> mountedResult = visualMount->Mount(
            *rootVisual.Value(),
            *rootLayout.Value(),
            ResolveFrameworkElement(*requestedRoot, requestedRoot->RuntimeType()),
            {loadedDocument.visualContent.mountEdges.Data(),
             loadedDocument.visualContent.mountEdges.Size()},
            availableSize);
        if (!mountedResult) return mountedResult.GetStatus();
        root = std::move(requestedRoot);
        mounted = true;
        Base::Result<void> presentation =
            presentationServices.Apply(*rootVisual.Value());
        if (!presentation) {
            DetachRuntimePresentation();
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return presentation.GetStatus();
        }
        Base::Result<void> interactions =
            CreateInteractions();
        if (!interactions) {
            DestroyInteractions();
            DetachRuntimePresentation();
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return interactions.GetStatus();
        }
        return {};
    }

    Base::Result<void> UnmountRoot() noexcept {
        if (!initialized) return {};
        if (!mounted) {
            if (loadedDocument.root) {
                ClearLoadedDocument();
            }
            return {};
        }
        DestroyInteractions();
        DetachRuntimePresentation();
        Base::Result<void> unmounted =
            visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()});
        mounted = false;
        root.Reset();
        ClearLoadedDocument();
        bindings->Shutdown();
        Base::Result<void> bindingsReady =
            bindings->Initialize();
        if (!unmounted) return unmounted.GetStatus();
        return bindingsReady;
    }
};

RuntimeHost::RuntimeHost(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(*allocator_);
}

RuntimeHost::~RuntimeHost() noexcept {
    Shutdown();
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Markup);
        impl_ = nullptr;
    }
}

Base::Result<void> RuntimeHost::AddModule(
    const ModuleRegistration& registration) noexcept {
    if (impl_ == nullptr || impl_->initialized || impl_->terminal) {
        return RuntimeInvalidState(
            "RuntimeHost modules must be added before initialization");
    }
    return impl_->modules.TryAdd(registration);
}

Base::Result<void> RuntimeHost::Initialize() noexcept {
    return Initialize({});
}

Base::Result<void> RuntimeHost::Initialize(
    const RuntimeHostOptions& options) noexcept {
    return impl_->InitializeServices(options);
}

void RuntimeHost::Shutdown() noexcept {
    if (impl_ == nullptr || impl_->terminal) return;
    impl_->ShutdownServices();
    impl_->terminal = true;
}

bool RuntimeHost::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

bool RuntimeHost::IsMounted() const noexcept {
    return impl_ != nullptr && impl_->mounted;
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadXaml(
    Base::StringView uri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<void> ready = impl_->BeginDocumentLoad();
    if (!ready) return ready.GetStatus();
    XamlActivationContext activationContext =
        impl_->ActivationContext();
    Base::Result<XamlLoadOptions> options =
        impl_->LoadOptions(activationContext);
    if (!options) return options.GetStatus();
    XamlLoader loader(
        *impl_->schema,
        impl_->xamlSources,
        diagnostics);
    return impl_->CompleteDocumentLoad(
        loader.Load(uri, options.Value()));
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::ParseXaml(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<void> ready = impl_->BeginDocumentLoad();
    if (!ready) return ready.GetStatus();
    XamlActivationContext activationContext =
        impl_->ActivationContext();
    Base::Result<XamlLoadOptions> options =
        impl_->LoadOptions(activationContext);
    if (!options) return options.GetStatus();
    XamlLoader loader(
        *impl_->schema,
        impl_->xamlSources,
        diagnostics);
    return impl_->CompleteDocumentLoad(
        loader.Parse(
            source, baseUri, options.Value()));
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadCompiledXaml(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<void> ready = impl_->BeginDocumentLoad();
    if (!ready) return ready.GetStatus();
    XamlActivationContext activationContext =
        impl_->ActivationContext();
    Base::Result<XamlLoadOptions> options =
        impl_->LoadOptions(activationContext);
    if (!options) return options.GetStatus();
    XamlLoader loader(
        *impl_->schema,
        impl_->xamlSources);
    return impl_->CompleteDocumentLoad(
        loader.LoadCompiled(
            bytes, originUri, options.Value()));
}

Base::Result<void> RuntimeHost::RegisterXamlSourceProvider(
    IXamlSourceProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    if (impl_ == nullptr || impl_->terminal) {
        return RuntimeInvalidState(
            "RuntimeHost cannot register a XAML source provider");
    }
    return impl_->xamlSources.TryRegister(
        provider, scheme, assembly);
}

Base::Result<void> RuntimeHost::LoadResources(
    RuntimeResourceLayer layer,
    Base::StringView uri,
    RuntimeResourceLoadMode mode,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<Presentation::ResourceDictionary*> target =
        impl_->ResolveResourceLayer(layer);
    if (!target) return target.GetStatus();
    return impl_->LoadResourceLayer(
        uri,
        *target.Value(),
        diagnostics,
        mode == RuntimeResourceLoadMode::Merge);
}

Base::Result<void>
RuntimeHost::LoadCompiledResources(
    RuntimeResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    RuntimeResourceLoadMode mode) noexcept {
    Base::Result<Presentation::ResourceDictionary*> target =
        impl_->ResolveResourceLayer(layer);
    if (!target) return target.GetStatus();
    return impl_->LoadCompiledResourceLayer(
        bytes,
        originUri,
        *target.Value(),
        mode == RuntimeResourceLoadMode::Merge);
}

Base::Result<void> RuntimeHost::LoadBuiltInTheme(
    BuiltInTheme theme) noexcept {
    if (impl_ == nullptr) {
        return RuntimeInvalidState(
            "RuntimeHost has no implementation");
    }
    if (theme != BuiltInTheme::Light &&
        theme != BuiltInTheme::Dark) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Built-in theme value is invalid");
    }
    const std::uint8_t* paletteBytes =
        theme == BuiltInTheme::Light
        ? Detail::AeroThemeLightCompiled
        : Detail::AeroThemeDarkCompiled;
    const std::uint32_t paletteSize =
        theme == BuiltInTheme::Light
        ? Detail::AeroThemeLightCompiledSize
        : Detail::AeroThemeDarkCompiledSize;
    Base::Result<Base::ResourceUri> paletteUri =
        BuiltInThemeUri(
            theme == BuiltInTheme::Light
            ? Base::StringView("Light.xaml")
            : Base::StringView("Dark.xaml"));
    if (!paletteUri) return paletteUri.GetStatus();
    Base::Result<Base::ResourceUri> genericUri =
        BuiltInThemeUri(
            Base::StringView("Generic.xaml"));
    if (!genericUri) return genericUri.GetStatus();

    Presentation::ResourceDictionary previous =
        std::move(impl_->themeResources);
    Base::Result<void> loaded = paletteSize != 0U
        ? LoadCompiledResources(
              RuntimeResourceLayer::Theme,
              {paletteBytes, paletteSize},
              paletteUri.Value())
        : LoadResources(
              RuntimeResourceLayer::Theme,
              paletteUri.Value().Canonical());
    if (loaded) {
        loaded = Detail::AeroThemeGenericCompiledSize != 0U
            ? LoadCompiledResources(
                  RuntimeResourceLayer::Theme,
                  {Detail::AeroThemeGenericCompiled,
                   Detail::AeroThemeGenericCompiledSize},
                  genericUri.Value(),
                  RuntimeResourceLoadMode::Merge)
            : LoadResources(
                  RuntimeResourceLayer::Theme,
                  genericUri.Value().Canonical(),
                  RuntimeResourceLoadMode::Merge);
    }
    if (!loaded) {
        impl_->themeResources =
            std::move(previous);
        Base::Result<void> restored =
            impl_->RebuildDynamicResourceEnvironment();
        return restored
            ? Base::Result<void>(loaded.GetStatus())
            : Base::Result<void>(restored.GetStatus());
    }
    return {};
}

Base::Result<void> RuntimeHost::Mount(
    Presentation::Size availableSize) noexcept {
    if (!impl_->loadedDocument.root) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "RuntimeHost has no staged XAML root");
    }
    return impl_->MountRoot(
        impl_->loadedDocument.root, availableSize);
}

Base::Result<void> RuntimeHost::Mount(
    Base::Ref<Base::Object> root,
    Presentation::Size availableSize) noexcept {
    return impl_->MountRoot(
        std::move(root), availableSize);
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadAndMountXaml(
    Base::StringView uri,
    Presentation::Size availableSize,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadXaml(uri, diagnostics);
    if (!loaded) return loaded.GetStatus();
    Base::Result<void> mounted = Mount(availableSize);
    if (!mounted) return mounted.GetStatus();
    return impl_->root;
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::ParseAndMountXaml(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    Presentation::Size availableSize,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<Base::Ref<Base::Object>> loaded =
        ParseXaml(source, baseUri, diagnostics);
    if (!loaded) return loaded.GetStatus();
    Base::Result<void> mounted = Mount(availableSize);
    if (!mounted) return mounted.GetStatus();
    return impl_->root;
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadAndMountCompiledXaml(
    Base::Span<const std::uint8_t> bytes,
    Presentation::Size availableSize,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadCompiledXaml(bytes, originUri);
    if (!loaded) return loaded.GetStatus();
    Base::Result<void> mounted = Mount(availableSize);
    if (!mounted) return mounted.GetStatus();
    return impl_->root;
}

Base::Result<void> RuntimeHost::Resize(
    Presentation::Size availableSize) noexcept {
    if (!IsMounted() || impl_ == nullptr ||
        impl_->visualMount == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "RuntimeHost resize requires a mounted visual tree");
    }
    return impl_->visualMount->Resize(availableSize);
}

Base::Result<void> RuntimeHost::Unmount() noexcept {
    return impl_->UnmountRoot();
}

Base::Result<RuntimeFrameResult>
RuntimeHost::RunFrame() noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "RuntimeHost must be initialized before running frames");
    }
    const Core::DispatcherFramePhase phases[] = {
        Core::DispatcherFramePhase::BeginFrame,
        Core::DispatcherFramePhase::PropertyChanges,
        Core::DispatcherFramePhase::DataBind,
        Core::DispatcherFramePhase::Lifecycle,
        Core::DispatcherFramePhase::Layout,
        Core::DispatcherFramePhase::RenderCommit,
        Core::DispatcherFramePhase::EndFrame};
    RuntimeFrameResult result;
    for (Core::DispatcherFramePhase phase : phases) {
        Base::Result<std::uint32_t> ran =
            impl_->dispatcher.RunFramePhase(phase);
        if (!ran) return ran.GetStatus();
        result.callbackCount += ran.Value();
    }
    result.frameNumber = ++impl_->frameNumber;
    result.layout = impl_->layout->Diagnostics();
    result.render = impl_->renderer->Diagnostics();
    return result;
}

Base::Result<Presentation::PointerDispatchResult>
RuntimeHost::DispatchPointer(
    const Presentation::PointerInput& input) noexcept {
    if (!IsMounted() || impl_->pointer == nullptr) {
        return RuntimeNotInitialized(
            "Pointer input requires a mounted RuntimeHost");
    }
    return impl_->pointer->Dispatch(input);
}

Base::Result<Presentation::KeyboardDispatchResult>
RuntimeHost::DispatchKeyboard(
    const Presentation::KeyboardInput& input) noexcept {
    if (!IsMounted() || impl_->keyboard == nullptr) {
        return RuntimeNotInitialized(
            "Keyboard input requires a mounted RuntimeHost");
    }
    return impl_->keyboard->Dispatch(input);
}

Base::Result<Presentation::TextInputDispatchResult>
RuntimeHost::DispatchText(
    const Presentation::TextInput& input) noexcept {
    if (!IsMounted() || impl_->textInput == nullptr) {
        return RuntimeNotInitialized(
            "Text input requires a mounted RuntimeHost");
    }
    return impl_->textInput->Dispatch(input);
}

Base::Result<std::uint32_t>
RuntimeHost::AdvanceTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    if (!IsMounted() ||
        impl_->controlInteractions == nullptr) {
        return RuntimeNotInitialized(
            "Control timing requires mounted control interactions");
    }
    return impl_->controlInteractions->AdvanceTime(
        elapsedMilliseconds);
}

const Base::Ref<Base::Object>&
RuntimeHost::Root() const noexcept {
    static const Base::Ref<Base::Object> empty;
    return impl_ != nullptr ? impl_->root : empty;
}

Base::Object* RuntimeHost::FindNamedObject(
    Base::StringView name,
    Core::TypeId expectedType) noexcept {
    if (impl_ == nullptr || name.Empty()) {
        return nullptr;
    }
    Base::Object* object = impl_->loadedDocument.names.Find(name);
    if (object == nullptr || expectedType == Core::InvalidTypeId) {
        return object;
    }
    return impl_->metadata->Descriptors().IsAssignableFrom(
        expectedType, object->RuntimeType()) ? object : nullptr;
}

Core::MetadataDomain* RuntimeHost::Metadata() noexcept {
    return IsInitialized() ? impl_->metadata : nullptr;
}

Core::MetadataRuntime*
RuntimeHost::MetadataRuntime() noexcept {
    return impl_ != nullptr ? impl_->metadataRuntime : nullptr;
}

Core::EffectiveValueEngine*
RuntimeHost::EffectiveValues() noexcept {
    return impl_ != nullptr ? impl_->values : nullptr;
}

Presentation::ObjectTree* RuntimeHost::Tree() noexcept {
    return impl_ != nullptr ? impl_->tree : nullptr;
}

Presentation::LayoutManager* RuntimeHost::Layout() noexcept {
    return impl_ != nullptr ? impl_->layout : nullptr;
}

Presentation::RenderManager* RuntimeHost::Renderer() noexcept {
    return impl_ != nullptr ? impl_->renderer : nullptr;
}

Presentation::BindingManager* RuntimeHost::Bindings() noexcept {
    return impl_ != nullptr ? impl_->bindings : nullptr;
}

Presentation::CommandManager* RuntimeHost::Commands() noexcept {
    return impl_ != nullptr ? impl_->commands : nullptr;
}

Presentation::RoutedEventManager*
RuntimeHost::RoutedEvents() noexcept {
    return impl_ != nullptr ? impl_->events : nullptr;
}

Presentation::FocusManager* RuntimeHost::Focus() noexcept {
    return impl_ != nullptr ? impl_->focus : nullptr;
}

Controls::TemplateManager* RuntimeHost::Templates() noexcept {
    return impl_ != nullptr ? impl_->templates : nullptr;
}

Controls::VisualStateManager*
RuntimeHost::VisualStates() noexcept {
    return impl_ != nullptr ? impl_->visualStates : nullptr;
}

XamlSchemaContext* RuntimeHost::Schema() noexcept {
    return impl_ != nullptr ? impl_->schema : nullptr;
}

Core::ActivationProviderRegistry*
RuntimeHost::ActivationFacets() noexcept {
    return impl_ != nullptr
        ? impl_->activation
        : nullptr;
}

XamlSourceProviderRegistry*
RuntimeHost::XamlSources() noexcept {
    return impl_ != nullptr
        ? &impl_->xamlSources
        : nullptr;
}

EmbeddedXamlSourceProvider*
RuntimeHost::EmbeddedXamlSources() noexcept {
    return impl_ != nullptr
        ? &impl_->embeddedXaml
        : nullptr;
}

Presentation::ResourceDictionary*
RuntimeHost::ApplicationResources() noexcept {
    return impl_ != nullptr
        ? &impl_->applicationResources
        : nullptr;
}

Presentation::ResourceDictionary*
RuntimeHost::ThemeResources() noexcept {
    return impl_ != nullptr
        ? &impl_->themeResources
        : nullptr;
}

Presentation::ResourceDictionary*
RuntimeHost::SystemResources() noexcept {
    return impl_ != nullptr
        ? &impl_->systemResources
        : nullptr;
}

Presentation::StyleManager*
RuntimeHost::Styles() noexcept {
    return impl_ != nullptr ? impl_->styles : nullptr;
}

std::uint32_t RuntimeHost::NamedObjectCount() const noexcept {
    return impl_ != nullptr ? impl_->loadedDocument.names.Size() : 0U;
}

} // namespace Aero
