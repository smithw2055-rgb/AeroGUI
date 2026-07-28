#include "runtime/ViewRuntime.hpp"
#include "runtime/TextRuntime.hpp"
#include "SchemaBundle.hpp"

#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include "core/metadata/MetadataDomainAccess.hpp"
#include "markup/Loader.hpp"
#include "markup/LoadOptionsAccess.hpp"
#include "markup/LoaderResult.hpp"
#include "UiDocumentAccess.hpp"
#include <Aero/Markup/Schema.hpp>
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
#include "controls/TextServicesAccess.hpp"
#include "integration/RenderEndpointInternal.hpp"
#include "presentation/RenderingInternal.hpp"

#include <new>
#include <utility>

namespace Aero {
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

class EndpointRenderBackend final
    : public Presentation::IRenderBackend {
public:
    void SetEndpoint(
        Base::Ref<Integration::RenderEndpoint> endpoint) noexcept {
        endpoint_ = std::move(endpoint);
    }

    void Reset() noexcept {
        endpoint_.Reset();
    }

    Base::Result<void> Submit(
        const Presentation::RenderPlan& plan) noexcept override {
        if (!endpoint_) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View has no render endpoint");
        }
        return Integration::Detail::RenderEndpointAccess::Submit(
            *endpoint_, plan);
    }

private:
    void* QueryInternalService(
        std::uint64_t service) noexcept override {
        return endpoint_
            ? Integration::Detail::RenderEndpointAccess::
                  QueryInternalService(*endpoint_, service)
            : nullptr;
    }

    Base::Ref<Integration::RenderEndpoint> endpoint_;
};

} // namespace

struct ViewRuntime::Impl final {
    Impl(
        Base::IAllocator& value,
        SchemaBundle* sharedSchema = nullptr,
        Markup::DocumentCache* sharedDocumentCache = nullptr) noexcept
        : allocator(&value),
          ownedSchemaBundle(&value),
          schemaBundle(sharedSchema != nullptr
              ? sharedSchema
              : &ownedSchemaBundle),
          usesSharedSchema(sharedSchema != nullptr),
          ownedDocumentCache(&value),
          documentCache(sharedDocumentCache != nullptr
              ? sharedDocumentCache
              : &ownedDocumentCache) {}

    Base::IAllocator* allocator = nullptr;
    Core::Dispatcher dispatcher;
    SchemaBundle ownedSchemaBundle;
    SchemaBundle* schemaBundle = nullptr;
    bool usesSharedSchema = false;
    Markup::DocumentCache ownedDocumentCache;
    Markup::DocumentCache* documentCache = nullptr;
    Core::MetadataDomain* metadata = nullptr;
    ModuleCatalog modules;
    ViewRuntimeOptions options;
    Base::Ref<Integration::RenderEndpoint> endpoint;
    EndpointRenderBackend endpointBackend;
    bool endpointBound = false;
    std::uint64_t endpointGeneration = 0U;

    Core::MetadataRuntime* metadataRuntime = nullptr;
    Core::ObjectServicesScope* objectServices = nullptr;
    Core::EffectiveValueEngine* values = nullptr;
    Presentation::ObjectTree* tree = nullptr;
    Presentation::LayoutManager* layout = nullptr;
    Presentation::RenderManager* renderer = nullptr;
    Detail::TextRuntime* textRuntime = nullptr;
    Presentation::BindingManager* bindings = nullptr;
    Presentation::RoutedEventManager* events = nullptr;
    Presentation::CommandManager* commands = nullptr;
    Controls::TemplateManager* templates = nullptr;
    Controls::VisualStateManager* visualStates = nullptr;
    Presentation::StyleManager* styles = nullptr;
    Detail::RuntimePresentationServices presentationServices;

    Markup::Schema* schema = nullptr;
    Presentation::VisualTreeMount* visualMount = nullptr;
    Markup::SourceProviderRegistry xamlSources;
    Markup::EmbeddedSourceProvider embeddedXaml;
    Markup::FileSourceProvider fileXaml;
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

    Markup::LoaderResult loadedDocument;
    Markup::LoadContext loadContext;
    Base::Ref<Markup::EffectLifetime> effectLifetime;
    Base::Ref<Base::Object> root;
    std::uint64_t frameNumber = 0U;
    bool initialized = false;
    bool mounted = false;
    bool terminal = false;

    Presentation::IRenderBackend& SelectedBackend() noexcept {
        return endpointBackend;
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
                "ViewRuntime must be initialized before XAML loading");
        }
        if (mounted || root || loadedDocument.root) {
            return RuntimeInvalidState(
                "ViewRuntime already owns a loaded document");
        }
        return {};
    }

    Base::Result<Markup::LoadOptions> LoadOptions(
        bool deferredEffects = false) noexcept {
        Markup::LoadOptions result;
        loadContext.resources = &dynamicResourceEnvironment;
        loadContext.effectiveValues = values;
        loadContext.bindings = bindings;
        loadContext.fallbackResources =
            &dynamicResourceEnvironment;
        loadContext.documentCache = documentCache;
        loadContext.dispatcher = &dispatcher;
        loadContext.dependencyProperties =
            &Core::Detail::MetadataDomainAccess::
                DependencyProperties(*metadata);
        loadContext.effectLifetime = effectLifetime;
        loadContext.effectCommitMode = deferredEffects
            ? Markup::EffectCommitMode::Deferred
            : Markup::EffectCommitMode::Immediate;
        Markup::Detail::LoadOptionsAccess::SetContext(
            result, &loadContext);
        return result;
    }

    void AttachTextService(
        Presentation::Visual& node,
        Controls::Detail::TextLayoutService* service,
        bool invalidate = false) noexcept {
        if (metadata == nullptr) return;
        const Core::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBlock::StaticTypeId())) {
            Controls::Detail::TextServicesAccess::Attach(
                *static_cast<Controls::TextBlock*>(&node),
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBox::StaticTypeId())) {
            Controls::Detail::TextServicesAccess::Attach(
                *static_cast<Controls::TextBox*>(&node),
                service,
                invalidate);
        }
    }

    void VisitTextServices(
        Presentation::Visual* rootVisual,
        Controls::Detail::TextLayoutService* service,
        bool invalidate = false) noexcept {
        if (rootVisual == nullptr) return;
        AttachTextService(
            *rootVisual, service, invalidate);
        for (Presentation::Visual* child :
             rootVisual->VisualChildren()) {
            VisitTextServices(
                child, service, invalidate);
        }
    }

    static void TextLifecycleHook(
        const Presentation::ObjectTreeLifecycleEvent& event,
        void* context) noexcept {
        auto* runtime = static_cast<Impl*>(context);
        if (runtime == nullptr || event.node == nullptr) {
            return;
        }
        runtime->AttachTextService(
            *event.node,
            event.loaded && runtime->textRuntime != nullptr
                ? runtime->textRuntime->Service()
                : nullptr);
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
            "ViewRuntime resource layer is invalid");
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
        if (!metadata->Types().IsDerivedFrom(
                root->RuntimeType(),
                Presentation::Visual::StaticTypeId())) {
            return nullptr;
        }
        return static_cast<Presentation::Visual*>(root.Get());
    }

    Base::Result<Presentation::Visual*> ResolveVisual(
        Base::Object& object, Core::TypeId type) noexcept {
        if (object.RuntimeType() != type ||
            !metadata->Types().IsDerivedFrom(
                type, Presentation::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ViewRuntime root is not a registered Visual");
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
                "ViewRuntime root is not a UIElement");
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
            Core::Detail::MetadataDomainAccess::
                DependencyProperties(*metadata),
            layout, renderer);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            visualStates, *values, *templates);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            styles, *values,
            Core::Detail::MetadataDomainAccess::
                DependencyProperties(*metadata));
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
            AttachTextService(
                *node,
                textRuntime != nullptr
                    ? textRuntime->Service()
                    : nullptr);
            if (controlInteractions != nullptr &&
                metadata->Types().IsDerivedFrom(
                    type, Controls::ButtonBase::StaticTypeId())) {
                Base::Result<void> attached =
                    controlInteractions->Attach(
                        *static_cast<Controls::ButtonBase*>(node));
                if (!attached) return attached.GetStatus();
            }
            if (metadata->Types().IsDerivedFrom(
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
        if (metadata->Types().IsDerivedFrom(
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
        if (tree != nullptr) {
            tree->SetLifecycleHandler(nullptr);
        }
        VisitTextServices(RootVisual(), nullptr);
        if (visualMount != nullptr && visualMount->IsMounted()) {
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
        }
        mounted = false;
        root.Reset();
        ClearLoadedDocument();
        if (effectLifetime) effectLifetime->Invalidate();

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
            *allocator, Base::MemoryTag::Render,
            textRuntime);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            values);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            objectServices);
        schema = nullptr;
        metadataRuntime = nullptr;
        metadata = nullptr;
        endpointBackend.Reset();
        if (endpointBound && endpoint) {
            Integration::Detail::RenderEndpointAccess::Unbind(
                *endpoint, this);
        }
        endpointBound = false;
        endpoint.Reset();
        initialized = false;
    }

    Base::Result<void> InitializeServices(
        const ViewRuntimeOptions& requested) noexcept {
        if (initialized) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "ViewRuntime is already initialized");
        }
        if (terminal) {
            return RuntimeInvalidState(
                "ViewRuntime cannot be restarted after shutdown or failed startup");
        }
        options = requested;

        endpoint = options.renderEndpoint;
        if (!endpoint) {
            Base::Result<Base::Ref<Integration::RenderEndpoint>>
                headless =
                    Integration::Detail::RenderEndpointAccess::
                        CreateHeadless(
                            Integration::RenderSubmissionMode::Immediate,
                            allocator);
            if (!headless) {
                terminal = true;
                return headless.GetStatus();
            }
            endpoint = std::move(headless).Value();
        }
        Base::Result<void> endpointBinding =
            Integration::Detail::RenderEndpointAccess::Bind(
                *endpoint, this);
        if (!endpointBinding) {
            endpoint.Reset();
            terminal = true;
            return endpointBinding.GetStatus();
        }
        endpointBound = true;
        endpointGeneration = endpoint->Generation();
        endpointBackend.SetEndpoint(endpoint);

        Base::Result<Base::Ref<Markup::EffectLifetime>> lifetime =
            Base::MakeRefWithAllocator<Markup::EffectLifetime>(
                *allocator);
        Base::Result<void> status = lifetime
            ? Base::Result<void>()
            : Base::Result<void>(lifetime.GetStatus());
        if (status) effectLifetime = std::move(lifetime).Value();
        if (status) status = EnsureDefaultXamlProviders();
        if (status && !schemaBundle->IsPrepared()) {
            status = schemaBundle->Prepare(modules);
        }
        if (!status || !schemaBundle->IsPrepared()) {
            terminal = true;
            return status
                ? RuntimeInvalidState(
                      "ViewRuntime schema bundle was not prepared")
                : status.GetStatus();
        }
        metadata = &schemaBundle->Metadata();
        metadataRuntime = &schemaBundle->Runtime();

        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                objectServices, dispatcher,
                Core::Detail::MetadataDomainAccess::
                    DependencyProperties(*metadata),
                *metadataRuntime);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                values, dispatcher,
                Core::Detail::MetadataDomainAccess::
                    DependencyProperties(*metadata));
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
                *allocator, Base::MemoryTag::Render,
                textRuntime, allocator);
        }
        if (status) {
            status = textRuntime->Initialize(
                SelectedBackend(), options.text);
        }
        if (status) {
            tree->SetLifecycleHandler(
                &TextLifecycleHook, this);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                bindings, dispatcher);
        }
        if (status) status = bindings->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                events,
                Core::Detail::MetadataDomainAccess::
                    RoutedEventState(*metadata));
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
        if (status && !schemaBundle->IsFrozen()) {
            status = schemaBundle->Finalize(
                SchemaBundleServices{allocator});
        }
        if (status && schemaBundle->IsFrozen()) {
            schema = &schemaBundle->Schema();
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
        UiDocument document,
        Presentation::ResourceDictionary& target,
        bool merge) noexcept {
        const Base::Ref<Base::Object>& rootObject =
            document.Root();
        if (!rootObject ||
            rootObject->RuntimeType() !=
                Presentation::ResourceDictionary::StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ViewRuntime resource document root must be ResourceDictionary");
        }
        auto& dictionary =
            static_cast<Presentation::ResourceDictionary&>(
                *rootObject);
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
                            "ViewRuntime resource merge rollback lost its dictionary")
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
                "ViewRuntime must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return RuntimeInvalidState(
                "ViewRuntime resource layers must be loaded before a document");
        }
        Base::Result<Markup::LoadOptions> loadOptions =
            LoadOptions();
        if (!loadOptions) {
            return loadOptions.GetStatus();
        }
        Markup::Loader loader(
            *schema,
            xamlSources,
            diagnostics,
            allocator);
        Base::Result<UiDocument> loaded =
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
                "ViewRuntime must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return RuntimeInvalidState(
                "ViewRuntime resource layers must be loaded before a document");
        }
        Base::Result<Markup::LoadOptions> loadOptions =
            LoadOptions();
        if (!loadOptions) return loadOptions.GetStatus();
        Markup::Loader loader(
            *schema, xamlSources, nullptr, allocator);
        Base::Result<UiDocument> loaded =
            loader.LoadCompiled(
                bytes, originUri, loadOptions.Value());
        if (!loaded) return loaded.GetStatus();

        return CommitResourceLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

    Base::Result<void> ValidateDocumentRoot(
        const Base::Ref<Base::Object>& requestedRoot) noexcept {
        if (!requestedRoot) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ViewRuntime root must not be null");
        }
        if (!metadata->Types().IsDerivedFrom(
                requestedRoot->RuntimeType(),
                Presentation::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ViewRuntime root must derive from Visual");
        }
        Base::Result<Presentation::UIElement*> rootLayout =
            ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        return rootLayout
            ? Base::Result<void>()
            : Base::Result<void>(rootLayout.GetStatus());
    }

    Base::Result<void> MountRoot(
        Base::Ref<Base::Object> requestedRoot,
        Presentation::Size availableSize) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "ViewRuntime must be initialized before mounting");
        }
        if (mounted || root) {
            return RuntimeInvalidState(
                "ViewRuntime already has a mounted root");
        }
        Base::Result<void> validRoot = ValidateDocumentRoot(requestedRoot);
        if (!validRoot) return validRoot.GetStatus();
        if (loadedDocument.root &&
            loadedDocument.root.Get() != requestedRoot.Get()) {
            return RuntimeInvalidState(
                "Mounted root does not match the staged XAML document");
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
                "ViewRuntime visual mount service is unavailable");
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
        Base::Result<void> effects = loadedDocument.effects.Commit();
        if (!effects) {
            DestroyInteractions();
            DetachRuntimePresentation();
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return effects.GetStatus();
        }
        return {};
    }

    Base::Result<void> DetachMountedRoot(
        bool clearDocument) noexcept {
        if (!initialized) return {};
        if (!mounted) {
            if (clearDocument && loadedDocument.root) {
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
        if (clearDocument) ClearLoadedDocument();
        return unmounted;
    }

    Base::Result<void> UnmountRoot() noexcept {
        return DetachMountedRoot(true);
    }
};

ViewRuntime::ViewRuntime(
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

ViewRuntime::ViewRuntime(
    SchemaBundle& schemaBundle,
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
    impl_ = new (memory) Impl(*allocator_, &schemaBundle);
}

ViewRuntime::ViewRuntime(
    SchemaBundle& schemaBundle,
    Markup::DocumentCache& documentCache,
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
    impl_ = new (memory) Impl(
        *allocator_, &schemaBundle, &documentCache);
}

ViewRuntime::~ViewRuntime() noexcept {
    Shutdown();
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Markup);
        impl_ = nullptr;
    }
}

Base::Result<void> ViewRuntime::AddModule(
    const ModuleRegistration& registration) noexcept {
    if (impl_ == nullptr || impl_->initialized || impl_->terminal ||
        impl_->usesSharedSchema) {
        return RuntimeInvalidState(
            "ViewRuntime modules require an owned, uninitialized schema bundle");
    }
    return impl_->modules.Add(registration);
}

Base::Result<void> ViewRuntime::Initialize() noexcept {
    return Initialize({});
}

Base::Result<void> ViewRuntime::Initialize(
    const ViewRuntimeOptions& options) noexcept {
    return impl_->InitializeServices(options);
}

void ViewRuntime::Shutdown() noexcept {
    if (impl_ == nullptr || impl_->terminal) return;
    impl_->ShutdownServices();
    impl_->terminal = true;
}

bool ViewRuntime::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

bool ViewRuntime::IsMounted() const noexcept {
    return impl_ != nullptr && impl_->mounted;
}

Base::Result<UiDocument> ViewRuntime::Load(
    Base::StringView uri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "ViewRuntime must be initialized before XAML loading");
    }
    Base::Result<Markup::LoadOptions> options =
        impl_->LoadOptions(true);
    if (!options) return options.GetStatus();
    Markup::Loader loader(
        *impl_->schema,
        impl_->xamlSources,
        diagnostics,
        allocator_);
    return loader.Load(uri, options.Value());
}

Base::Result<UiDocument> ViewRuntime::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "ViewRuntime must be initialized before XAML parsing");
    }
    Base::Result<Markup::LoadOptions> options =
        impl_->LoadOptions(true);
    if (!options) return options.GetStatus();
    Markup::Loader loader(
        *impl_->schema,
        impl_->xamlSources,
        diagnostics,
        allocator_);
    return loader.Parse(source, baseUri, options.Value());
}

Base::Result<UiDocument> ViewRuntime::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "ViewRuntime must be initialized before compiled XAML loading");
    }
    Base::Result<Markup::LoadOptions> options =
        impl_->LoadOptions(true);
    if (!options) return options.GetStatus();
    Markup::Loader loader(
        *impl_->schema,
        impl_->xamlSources,
        nullptr,
        allocator_);
    return loader.LoadCompiled(
        bytes, originUri, options.Value());
}

Base::Result<void> ViewRuntime::RegisterSourceProvider(
    Integration::ISourceProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    if (impl_ == nullptr || impl_->terminal) {
        return RuntimeInvalidState(
            "ViewRuntime cannot register a XAML source provider");
    }
    return impl_->xamlSources.TryRegister(
        provider, scheme, assembly);
}

Base::Result<void> ViewRuntime::LoadResources(
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
ViewRuntime::LoadCompiledResources(
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

Base::Result<void> ViewRuntime::LoadBuiltInTheme(
    BuiltInTheme theme) noexcept {
    if (impl_ == nullptr) {
        return RuntimeInvalidState(
            "ViewRuntime has no implementation");
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

Base::Result<void> ViewRuntime::Mount(
    Presentation::Size availableSize) noexcept {
    if (!impl_->loadedDocument.root) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "ViewRuntime has no staged XAML root");
    }
    return impl_->MountRoot(
        impl_->loadedDocument.root, availableSize);
}

Base::Result<void> ViewRuntime::Mount(
    Base::Ref<Base::Object> root,
    Presentation::Size availableSize) noexcept {
    return impl_->MountRoot(
        std::move(root), availableSize);
}

Base::Result<void> ViewRuntime::Mount(
    UiDocument&& document,
    Presentation::Size availableSize) noexcept {
    Base::Result<void> ready = impl_->BeginDocumentLoad();
    if (!ready) return ready.GetStatus();
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ViewRuntime cannot mount an empty UI document");
    }
    if (Detail::UiDocumentAccess::RuntimeLifetime(document) !=
        impl_->effectLifetime.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "UI document belongs to another View");
    }
    Base::Result<void> valid = impl_->ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();
    impl_->loadedDocument =
        Detail::UiDocumentAccess::Take(document);
    return impl_->MountRoot(
        impl_->loadedDocument.root, availableSize);
}

Base::Result<void> ViewRuntime::ReplaceMountedDocument(
    UiDocument&& document,
    Presentation::Size availableSize) noexcept {
    if (impl_ == nullptr || !impl_->initialized || !impl_->mounted) {
        return RuntimeInvalidState(
            "ViewRuntime document replacement requires a mounted view");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ViewRuntime cannot replace a document with an empty document");
    }
    if (Detail::UiDocumentAccess::RuntimeLifetime(document) !=
        impl_->effectLifetime.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Replacement UI document belongs to another View");
    }
    Base::Result<void> valid = impl_->ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();

    Markup::LoaderResult next =
        Detail::UiDocumentAccess::Take(document);
    if (!next.root ||
        !impl_->metadata->Types().IsDerivedFrom(
            next.root->RuntimeType(),
            Presentation::Visual::StaticTypeId())) {
        next.Clear();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Replacement UI document root must derive from Visual");
    }

    Base::Result<void> detached =
        impl_->DetachMountedRoot(false);
    if (!detached) {
        Base::Result<void> restored = impl_->MountRoot(
            impl_->loadedDocument.root, availableSize);
        next.Clear();
        return restored ? detached : restored;
    }

    Markup::LoaderResult previous =
        std::move(impl_->loadedDocument);
    impl_->loadedDocument = std::move(next);
    Base::Result<void> mounted = impl_->MountRoot(
        impl_->loadedDocument.root, availableSize);
    if (mounted) {
        previous.Clear();
        return {};
    }

    impl_->loadedDocument = std::move(previous);
    Base::Result<void> restored = impl_->MountRoot(
        impl_->loadedDocument.root, availableSize);
    return restored ? mounted : restored;
}

Base::Result<void> ViewRuntime::Resize(
    Presentation::Size availableSize) noexcept {
    if (!IsMounted() || impl_ == nullptr ||
        impl_->visualMount == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "ViewRuntime resize requires a mounted visual tree");
    }
    return impl_->visualMount->Resize(availableSize);
}

Base::Result<void> ViewRuntime::Unmount() noexcept {
    return impl_->UnmountRoot();
}

Base::Result<RuntimeFrameResult>
ViewRuntime::RunFrame() noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "ViewRuntime must be initialized before running frames");
    }
    if (impl_->endpoint) {
        const Base::Status endpointStatus =
            Integration::Detail::RenderEndpointAccess::
                FrameStatus(*impl_->endpoint);
        if (!endpointStatus.IsOk()) {
            return endpointStatus;
        }
        const std::uint64_t generation =
            impl_->endpoint->Generation();
        if (generation !=
            impl_->endpointGeneration) {
            Presentation::Visual* rootVisual =
                impl_->RootVisual();
            Presentation::FrameworkElement* root =
                rootVisual != nullptr
                ? rootVisual->AsFrameworkElement()
                : nullptr;
            if (root != nullptr) {
                Base::Result<void> invalidated =
                    impl_->renderer->Invalidate(*root);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
            impl_->endpointGeneration = generation;
        }
    }
    if (impl_->textRuntime != nullptr) {
        Base::Result<bool> synchronized =
            impl_->textRuntime->SynchronizeBackend();
        if (!synchronized) {
            return synchronized.GetStatus();
        }
        if (synchronized.Value()) {
            impl_->VisitTextServices(
                impl_->RootVisual(),
                impl_->textRuntime->Service(),
                true);
        }
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
        if (phase ==
            Core::DispatcherFramePhase::RenderCommit) {
            const Base::Status committed =
                impl_->renderer->LastCommitStatus();
            if (!committed.IsOk()) return committed;
        }
    }
    if (impl_->textRuntime != nullptr) {
        Base::Result<std::uint32_t> collected =
            impl_->textRuntime->CollectGarbage();
        if (!collected) return collected.GetStatus();
    }
    result.frameNumber = ++impl_->frameNumber;
    const Presentation::LayoutDiagnostics layout =
        impl_->layout->Diagnostics();
    result.layout.passVersion = layout.passVersion;
    result.layout.measuredCount = layout.measuredCount;
    result.layout.arrangedCount = layout.arrangedCount;
    result.layout.pendingMeasureCount =
        layout.pendingMeasureCount;
    result.layout.pendingArrangeCount =
        layout.pendingArrangeCount;
    const Presentation::RenderDiagnostics render =
        impl_->renderer->Diagnostics();
    result.render.snapshotVersion = render.commitVersion;
    result.render.nodeCount = render.nodeCount;
    result.render.commandCount = render.commandCount;
    result.render.dirtyCount = render.dirtyCount;
    result.render.snapshotHash = render.planHash;
    return result;
}

Base::Result<Presentation::PointerDispatchResult>
ViewRuntime::DispatchPointer(
    const Presentation::PointerInput& input) noexcept {
    if (!IsMounted() || impl_->pointer == nullptr) {
        return RuntimeNotInitialized(
            "Pointer input requires a mounted ViewRuntime");
    }
    return impl_->pointer->Dispatch(input);
}

Base::Result<Presentation::KeyboardDispatchResult>
ViewRuntime::DispatchKeyboard(
    const Presentation::KeyboardInput& input) noexcept {
    if (!IsMounted() || impl_->keyboard == nullptr) {
        return RuntimeNotInitialized(
            "Keyboard input requires a mounted ViewRuntime");
    }
    return impl_->keyboard->Dispatch(input);
}

Base::Result<Presentation::TextInputDispatchResult>
ViewRuntime::DispatchText(
    const Presentation::TextInput& input) noexcept {
    if (!IsMounted() || impl_->textInput == nullptr) {
        return RuntimeNotInitialized(
            "Text input requires a mounted ViewRuntime");
    }
    return impl_->textInput->Dispatch(input);
}

Base::Result<std::uint32_t>
ViewRuntime::AdvanceTime(
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
ViewRuntime::Root() const noexcept {
    static const Base::Ref<Base::Object> empty;
    return impl_ != nullptr ? impl_->root : empty;
}

Base::Object* ViewRuntime::FindNamedObject(
    Base::StringView name,
    Core::TypeId expectedType) noexcept {
    if (impl_ == nullptr || name.Empty()) {
        return nullptr;
    }
    Base::Object* object = impl_->loadedDocument.names.Find(name);
    if (object == nullptr || expectedType == Core::InvalidTypeId) {
        return object;
    }
    return impl_->metadata->Types().IsAssignableFrom(
        expectedType, object->RuntimeType()) ? object : nullptr;
}

Core::MetadataDomain* ViewRuntime::Metadata() noexcept {
    return IsInitialized() ? impl_->metadata : nullptr;
}

Core::MetadataRuntime*
ViewRuntime::MetadataRuntime() noexcept {
    return impl_ != nullptr ? impl_->metadataRuntime : nullptr;
}

Core::EffectiveValueEngine*
ViewRuntime::EffectiveValues() noexcept {
    return impl_ != nullptr ? impl_->values : nullptr;
}

Presentation::ObjectTree* ViewRuntime::Tree() noexcept {
    return impl_ != nullptr ? impl_->tree : nullptr;
}

Presentation::LayoutManager* ViewRuntime::Layout() noexcept {
    return impl_ != nullptr ? impl_->layout : nullptr;
}

Presentation::RenderManager* ViewRuntime::Renderer() noexcept {
    return impl_ != nullptr ? impl_->renderer : nullptr;
}

Presentation::BindingManager* ViewRuntime::Bindings() noexcept {
    return impl_ != nullptr ? impl_->bindings : nullptr;
}

Presentation::CommandManager* ViewRuntime::Commands() noexcept {
    return impl_ != nullptr ? impl_->commands : nullptr;
}

Presentation::RoutedEventManager*
ViewRuntime::RoutedEvents() noexcept {
    return impl_ != nullptr ? impl_->events : nullptr;
}

Presentation::FocusManager* ViewRuntime::Focus() noexcept {
    return impl_ != nullptr ? impl_->focus : nullptr;
}

Controls::TemplateManager* ViewRuntime::Templates() noexcept {
    return impl_ != nullptr ? impl_->templates : nullptr;
}

Controls::VisualStateManager*
ViewRuntime::VisualStates() noexcept {
    return impl_ != nullptr ? impl_->visualStates : nullptr;
}

Markup::Schema* ViewRuntime::Schema() noexcept {
    return impl_ != nullptr ? impl_->schema : nullptr;
}

Markup::SourceProviderRegistry*
ViewRuntime::Sources() noexcept {
    return impl_ != nullptr
        ? &impl_->xamlSources
        : nullptr;
}

Markup::EmbeddedSourceProvider*
ViewRuntime::EmbeddedSources() noexcept {
    return impl_ != nullptr
        ? &impl_->embeddedXaml
        : nullptr;
}

Markup::DocumentCache* ViewRuntime::DocumentCache() noexcept {
    return impl_ != nullptr ? impl_->documentCache : nullptr;
}

const Base::ResourceUri& ViewRuntime::CurrentDocumentUri() const noexcept {
    static const Base::ResourceUri empty;
    return impl_ != nullptr
        ? impl_->loadedDocument.canonicalUri
        : empty;
}

Base::Span<const Base::ResourceUri>
ViewRuntime::CurrentDocumentDependencies() const noexcept {
    return impl_ != nullptr
        ? Base::Span<const Base::ResourceUri>{
              impl_->loadedDocument.dependencies.Data(),
              impl_->loadedDocument.dependencies.Size()}
        : Base::Span<const Base::ResourceUri>{};
}

Presentation::ResourceDictionary*
ViewRuntime::ApplicationResources() noexcept {
    return impl_ != nullptr
        ? &impl_->applicationResources
        : nullptr;
}

Presentation::ResourceDictionary*
ViewRuntime::ThemeResources() noexcept {
    return impl_ != nullptr
        ? &impl_->themeResources
        : nullptr;
}

Presentation::ResourceDictionary*
ViewRuntime::SystemResources() noexcept {
    return impl_ != nullptr
        ? &impl_->systemResources
        : nullptr;
}

Presentation::StyleManager*
ViewRuntime::Styles() noexcept {
    return impl_ != nullptr ? impl_->styles : nullptr;
}

std::uint32_t ViewRuntime::NamedObjectCount() const noexcept {
    return impl_ != nullptr ? impl_->loadedDocument.names.Size() : 0U;
}

} // namespace Aero
