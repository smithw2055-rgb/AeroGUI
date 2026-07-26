#include <Aero/RuntimeHost.hpp>

#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlCompiledDocument.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XamlVisualTree.hpp>
#include <Aero/Platform/Clipboard.hpp>
#include <Aero/Platform/Ime.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/Commands.hpp>
#include <Aero/Presentation/ObjectTree.hpp>

#include <mutex>
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

struct QueuedRenderBackend::Impl final {
    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value), queue(&value) {}

    Base::IAllocator* allocator = nullptr;
    Presentation::IRenderBackend* downstream = nullptr;
    Base::Vector<Presentation::RenderPlan> queue;
    std::uint32_t capacity = 0U;
    FrameQueueFullPolicy policy =
        FrameQueueFullPolicy::DropOldest;
    FrameQueueStatistics statistics;
    mutable std::mutex mutex;
    bool initialized = false;
};

QueuedRenderBackend::QueuedRenderBackend(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Render});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Render);
    }
    impl_ = new (memory) Impl(*allocator_);
}

QueuedRenderBackend::~QueuedRenderBackend() noexcept {
    Shutdown();
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Render);
        impl_ = nullptr;
    }
}

Base::Result<void> QueuedRenderBackend::Initialize(
    Presentation::IRenderBackend& downstream,
    std::uint32_t capacity,
    FrameQueueFullPolicy policy) noexcept {
    if (impl_ == nullptr) {
        return RuntimeNotInitialized(
            "Render queue storage is unavailable");
    }
    if (&downstream == this || capacity == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render queue requires a downstream backend and nonzero capacity");
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Render queue is already initialized");
    }
    Base::Result<void> reserved =
        impl_->queue.TryReserve(capacity);
    if (!reserved) return reserved.GetStatus();
    impl_->downstream = &downstream;
    impl_->capacity = capacity;
    impl_->policy = policy;
    impl_->statistics = {};
    impl_->initialized = true;
    return {};
}

void QueuedRenderBackend::Shutdown() noexcept {
    if (impl_ == nullptr) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->queue.Clear();
    impl_->downstream = nullptr;
    impl_->capacity = 0U;
    impl_->statistics.pending = 0U;
    impl_->initialized = false;
}

Base::Result<void> QueuedRenderBackend::Submit(
    const Presentation::RenderPlan& plan) noexcept {
    if (impl_ == nullptr) {
        return RuntimeNotInitialized(
            "Render queue storage is unavailable");
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized || impl_->downstream == nullptr) {
        return RuntimeNotInitialized(
            "Render queue is not initialized");
    }
    if (impl_->queue.Size() >= impl_->capacity) {
        if (impl_->policy == FrameQueueFullPolicy::Reject) {
            ++impl_->statistics.rejected;
            return RuntimeInvalidState(
                "Render queue capacity is exhausted");
        }
        for (std::uint32_t index = 1U;
             index < impl_->queue.Size(); ++index) {
            impl_->queue[index - 1U] =
                std::move(impl_->queue[index]);
        }
        impl_->queue.PopBack();
        ++impl_->statistics.dropped;
    }
    Base::Result<void> appended =
        impl_->queue.TryPushBack(plan);
    if (!appended) return appended.GetStatus();
    ++impl_->statistics.accepted;
    impl_->statistics.pending = impl_->queue.Size();
    if (impl_->statistics.pending >
        impl_->statistics.highWatermark) {
        impl_->statistics.highWatermark =
            impl_->statistics.pending;
    }
    return {};
}

Base::Result<bool> QueuedRenderBackend::ConsumeOne() noexcept {
    if (impl_ == nullptr) {
        return RuntimeNotInitialized(
            "Render queue storage is unavailable");
    }
    Presentation::RenderPlan plan;
    Presentation::IRenderBackend* downstream = nullptr;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->initialized || impl_->downstream == nullptr) {
            return RuntimeNotInitialized(
                "Render queue is not initialized");
        }
        if (impl_->queue.Empty()) return false;
        plan = std::move(impl_->queue[0]);
        for (std::uint32_t index = 1U;
             index < impl_->queue.Size(); ++index) {
            impl_->queue[index - 1U] =
                std::move(impl_->queue[index]);
        }
        impl_->queue.PopBack();
        impl_->statistics.pending = impl_->queue.Size();
        downstream = impl_->downstream;
    }

    Base::Result<void> submitted =
        downstream->Submit(plan);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (submitted) {
            ++impl_->statistics.consumed;
        } else {
            ++impl_->statistics.failed;
        }
    }
    if (!submitted) return submitted.GetStatus();
    return true;
}

Base::Result<std::uint32_t>
QueuedRenderBackend::Drain() noexcept {
    std::uint32_t count = 0U;
    while (true) {
        Base::Result<bool> consumed = ConsumeOne();
        if (!consumed) return consumed.GetStatus();
        if (!consumed.Value()) return count;
        ++count;
    }
}

bool QueuedRenderBackend::IsInitialized() const noexcept {
    if (impl_ == nullptr) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->initialized;
}

FrameQueueStatistics
QueuedRenderBackend::Statistics() const noexcept {
    if (impl_ == nullptr) return {};
    std::lock_guard<std::mutex> lock(impl_->mutex);
    FrameQueueStatistics result = impl_->statistics;
    result.pending = impl_->queue.Size();
    return result;
}

struct RuntimeHost::Impl final {
    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value) {}

    Base::IAllocator* allocator = nullptr;
    Core::Dispatcher dispatcher;
    Core::MetadataDomain metadata;
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

    XamlSchemaContext* schema = nullptr;
    XamlActivationProviderRegistry* activation = nullptr;
    XamlVisualTreeHost* visualTree = nullptr;
    XamlObjectWriter* writer = nullptr;

    Presentation::HitTestManager hitTests;
    Presentation::FocusManager* focus = nullptr;
    Presentation::PointerInputManager* pointer = nullptr;
    Presentation::KeyboardInputManager* keyboard = nullptr;
    Presentation::TextInputManager* textInput = nullptr;
    Controls::ControlInteractionManager* controlInteractions = nullptr;
    Controls::TextBoxInteractionManager* textBoxInteractions = nullptr;

    Base::Ref<Base::Object> pendingRoot;
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
            &metadata.DependencyProperties();
        context.applicationServices =
            options.applicationServices;
        context.hostContext = options.hostContext;
        return context;
    }

    Presentation::Visual* RootVisual() noexcept {
        if (!root) return nullptr;
        if (!metadata.Descriptors().IsDerivedFrom(
                root->RuntimeType(),
                Presentation::Visual::StaticTypeId())) {
            return nullptr;
        }
        return static_cast<Presentation::Visual*>(root.Get());
    }

    Base::Result<void> CreateTemplateServices() noexcept {
        Base::Result<void> status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            templates, *tree, *values,
            metadata.DependencyProperties(), layout, renderer);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            visualStates, *values, *templates);
        if (!status) return status.GetStatus();
        return {};
    }

    void DestroyTemplateServices() noexcept {
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
                metadata.Descriptors().IsDerivedFrom(
                    type, Controls::ButtonBase::StaticTypeId())) {
                Base::Result<void> attached =
                    controlInteractions->Attach(
                        *static_cast<Controls::ButtonBase*>(node));
                if (!attached) return attached.GetStatus();
            }
            if (metadata.Descriptors().IsDerivedFrom(
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
        if (metadata.Descriptors().IsDerivedFrom(
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
        DestroyTemplateServices();
        if (visualTree != nullptr &&
            visualTree->IsMounted()) {
            static_cast<void>(visualTree->Unmount());
        }
        mounted = false;
        root.Reset();
        pendingRoot.Reset();
        if (writer != nullptr) writer->Reset();

        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Markup, writer);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Markup, visualTree);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Markup, activation);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Markup, schema);

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
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            metadataRuntime);
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
            modules.RegisterMetadata(metadata);
        if (status) status = metadata.Seal();
        if (!status) {
            terminal = true;
            return status.GetStatus();
        }

        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            metadataRuntime, metadata);
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                objectServices, dispatcher,
                metadata.DependencyProperties(),
                *metadataRuntime);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                values, dispatcher,
                metadata.DependencyProperties());
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
                events, metadata.RoutedEvents());
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                commands, *tree);
        }
        if (status) status = CreateTemplateServices();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Markup,
                schema, metadata, *metadataRuntime);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Markup,
                activation, *schema);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Markup,
                visualTree, *tree, *layout,
                *values, renderer);
        }
        if (status) status = visualTree->Register(*schema);
        if (status) status = metadataRuntime->Freeze();
        if (status) status = schema->Freeze();
        if (status) status = activation->Freeze();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Markup,
                writer, *schema);
        }
        if (!status) {
            ShutdownServices();
            terminal = true;
            return status.GetStatus();
        }
        initialized = true;
        return {};
    }

    Base::Result<Base::Ref<Base::Object>> LoadReader(
        XamlNodeReader& reader) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "RuntimeHost must be initialized before XAML loading");
        }
        if (mounted || root || pendingRoot) {
            return RuntimeInvalidState(
                "RuntimeHost already owns a loaded document");
        }
        writer->Reset();
        static_cast<void>(visualTree->DiscardStaged());
        Base::Result<Base::Ref<Base::Object>> loaded =
            LoadXamlVisualTreeWithActivation(
                *visualTree, *writer, reader,
                *activation, ActivationContext());
        if (!loaded) {
            static_cast<void>(visualTree->DiscardStaged());
            writer->Reset();
            return loaded.GetStatus();
        }
        pendingRoot = loaded.Value();
        return pendingRoot;
    }

    Base::Result<Base::Ref<Base::Object>> LoadCompiled(
        const XamlCompiledDocument& document) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "RuntimeHost must be initialized before XAML loading");
        }
        if (mounted || root || pendingRoot) {
            return RuntimeInvalidState(
                "RuntimeHost already owns a loaded document");
        }
        writer->Reset();
        static_cast<void>(visualTree->DiscardStaged());
        Base::Result<Base::Ref<Base::Object>> loaded =
            LoadXamlVisualTreeWithActivation(
                *visualTree, *writer, document,
                *activation, ActivationContext());
        if (!loaded) {
            static_cast<void>(visualTree->DiscardStaged());
            writer->Reset();
            return loaded.GetStatus();
        }
        pendingRoot = loaded.Value();
        return pendingRoot;
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
        if (pendingRoot &&
            pendingRoot.Get() != requestedRoot.Get()) {
            return RuntimeInvalidState(
                "Mounted root does not match the staged XAML document");
        }
        if (!metadata.Descriptors().IsDerivedFrom(
                requestedRoot->RuntimeType(),
                Presentation::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RuntimeHost root must derive from Visual");
        }
        Base::Result<void> mountedResult =
            visualTree->Mount(
                *requestedRoot,
                requestedRoot->RuntimeType(),
                availableSize);
        if (!mountedResult) return mountedResult.GetStatus();
        root = std::move(requestedRoot);
        pendingRoot.Reset();
        mounted = true;
        Base::Result<void> interactions =
            CreateInteractions();
        if (!interactions) {
            DestroyInteractions();
            static_cast<void>(visualTree->Unmount());
            mounted = false;
            root.Reset();
            writer->Reset();
            return interactions.GetStatus();
        }
        return {};
    }

    Base::Result<void> UnmountRoot() noexcept {
        if (!initialized) return {};
        if (!mounted) {
            if (pendingRoot) {
                static_cast<void>(visualTree->DiscardStaged());
                pendingRoot.Reset();
                writer->Reset();
            }
            return {};
        }
        DestroyInteractions();
        DestroyTemplateServices();
        Base::Result<void> unmounted =
            visualTree->Unmount();
        mounted = false;
        root.Reset();
        pendingRoot.Reset();
        writer->Reset();
        bindings->Shutdown();
        Base::Result<void> bindingsReady =
            bindings->Initialize();
        Base::Result<void> templatesReady =
            CreateTemplateServices();
        if (!unmounted) return unmounted.GetStatus();
        if (!bindingsReady) return bindingsReady.GetStatus();
        return templatesReady;
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
RuntimeHost::Load(XamlNodeReader& reader) noexcept {
    return impl_->LoadReader(reader);
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::Load(
    const XamlCompiledDocument& document) noexcept {
    return impl_->LoadCompiled(document);
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadXaml(
    Base::StringView source,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Utf8XmlTokenizer tokenizer;
    Base::Result<void> reset = tokenizer.Reset(source, diagnostics);
    if (!reset) return reset.GetStatus();
    XamlNodeReader reader(tokenizer, diagnostics);
    return Load(reader);
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadCompiledXaml(
    Base::Span<const std::uint8_t> bytes) noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "RuntimeHost must be initialized before XAML loading");
    }
    Base::Result<XamlCompiledDocument> document =
        XamlCompiledDocument::Deserialize(
            bytes, impl_->metadata, {});
    if (!document) return document.GetStatus();
    return Load(document.Value());
}

Base::Result<void> RuntimeHost::Mount(
    Presentation::Size availableSize) noexcept {
    if (!impl_->pendingRoot) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "RuntimeHost has no staged XAML root");
    }
    return impl_->MountRoot(
        impl_->pendingRoot, availableSize);
}

Base::Result<void> RuntimeHost::Mount(
    Base::Ref<Base::Object> root,
    Presentation::Size availableSize) noexcept {
    return impl_->MountRoot(
        std::move(root), availableSize);
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadAndMount(
    XamlNodeReader& reader,
    Presentation::Size availableSize) noexcept {
    Base::Result<Base::Ref<Base::Object>> loaded =
        Load(reader);
    if (!loaded) return loaded.GetStatus();
    Base::Result<void> mounted = Mount(availableSize);
    if (!mounted) return mounted.GetStatus();
    return impl_->root;
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadAndMount(
    const XamlCompiledDocument& document,
    Presentation::Size availableSize) noexcept {
    Base::Result<Base::Ref<Base::Object>> loaded =
        Load(document);
    if (!loaded) return loaded.GetStatus();
    Base::Result<void> mounted = Mount(availableSize);
    if (!mounted) return mounted.GetStatus();
    return impl_->root;
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadAndMountXaml(
    Base::StringView source,
    Presentation::Size availableSize,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadXaml(source, diagnostics);
    if (!loaded) return loaded.GetStatus();
    Base::Result<void> mounted = Mount(availableSize);
    if (!mounted) return mounted.GetStatus();
    return impl_->root;
}

Base::Result<Base::Ref<Base::Object>>
RuntimeHost::LoadAndMountCompiledXaml(
    Base::Span<const std::uint8_t> bytes,
    Presentation::Size availableSize) noexcept {
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadCompiledXaml(bytes);
    if (!loaded) return loaded.GetStatus();
    Base::Result<void> mounted = Mount(availableSize);
    if (!mounted) return mounted.GetStatus();
    return impl_->root;
}

Base::Result<void> RuntimeHost::Resize(
    Presentation::Size availableSize) noexcept {
    if (!IsMounted() || impl_ == nullptr ||
        impl_->visualTree == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "RuntimeHost resize requires a mounted visual tree");
    }
    return impl_->visualTree->Resize(availableSize);
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
    if (impl_ == nullptr || impl_->writer == nullptr || name.Empty()) {
        return nullptr;
    }
    Base::Object* object = impl_->writer->DocumentNameScope().Find(name);
    if (object == nullptr || expectedType == Core::InvalidTypeId) {
        return object;
    }
    return impl_->metadata.Descriptors().IsAssignableFrom(
        expectedType, object->RuntimeType()) ? object : nullptr;
}

Core::MetadataDomain* RuntimeHost::Metadata() noexcept {
    return IsInitialized() ? &impl_->metadata : nullptr;
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

XamlActivationProviderRegistry*
RuntimeHost::Activation() noexcept {
    return impl_ != nullptr ? impl_->activation : nullptr;
}

XamlVisualTreeHost* RuntimeHost::VisualTree() noexcept {
    return impl_ != nullptr ? impl_->visualTree : nullptr;
}

XamlObjectWriter* RuntimeHost::Writer() noexcept {
    return impl_ != nullptr ? impl_->writer : nullptr;
}

} // namespace Aero
