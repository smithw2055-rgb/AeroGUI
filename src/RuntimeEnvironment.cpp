#include <Aero/Integration/View.hpp>

#include <Aero/Integration/SourceProvider.hpp>
#include <Aero/Integration/ViewHost.hpp>
#include "SchemaBundle.hpp"

#include "markup/Loader.hpp"
#include "markup/LoadOptionsAccess.hpp"
#include "runtime/ViewRuntime.hpp"

#include <new>
#include <utility>
#include "ui/RuntimeManagers.hpp"
#include "controls/RuntimeManagers.hpp"

namespace Aero {

struct RuntimeEnvironment::Impl final : public Base::Object {
    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value), schema(&value), documents(&value) {}

    Base::IAllocator* allocator = nullptr;
    ModuleCatalog modules;
    SchemaBundle schema;
    Markup::DocumentCache documents;
    bool initialized = false;
};

struct View::Impl final {
    Impl(
        Base::Ref<Base::Object> environmentState,
        SchemaBundle& schema,
        Markup::DocumentCache& documents,
        Base::IAllocator* allocator) noexcept
        : environment(std::move(environmentState)),
          runtime(schema, documents, allocator) {}

    Base::Ref<Base::Object> environment;
    ViewRuntime runtime;
};

RuntimeEnvironment::RuntimeEnvironment(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    Base::Result<Base::Ref<Impl>> made =
        Base::MakeRefWithAllocator<Impl>(
            *allocator_, *allocator_);
    if (!made) {
        Base::ReportOutOfMemory(
            sizeof(Impl),
            alignof(Impl),
            Base::MemoryTag::Object);
    }
    impl_ = Base::Ref<Base::Object>(std::move(made).Value());
}

RuntimeEnvironment::~RuntimeEnvironment() noexcept = default;

Base::Result<void> RuntimeEnvironment::AddModule(
    const ModuleRegistration& registration) noexcept {
    Impl& state = static_cast<Impl&>(*impl_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Runtime environment modules are frozen");
    }
    return state.modules.Add(registration);
}

Base::Result<void> RuntimeEnvironment::Initialize() noexcept {
    Impl& state = static_cast<Impl&>(*impl_);
    if (state.initialized) return {};
    Base::Result<void> prepared = state.schema.Prepare(state.modules);
    if (!prepared) return prepared.GetStatus();
    Base::Result<void> finalized = state.schema.Finalize(
        SchemaBundleServices{state.allocator});
    if (!finalized) return finalized.GetStatus();
    Base::Result<void> frozen = state.modules.Freeze();
    if (!frozen) return frozen.GetStatus();
    state.initialized = true;
    return {};
}

Base::Result<Base::Ref<View>> RuntimeEnvironment::CreateView(
    Base::IAllocator* allocator) noexcept {
    return CreateIntegratedView({}, allocator);
}

Base::Result<Base::Ref<View>>
RuntimeEnvironment::CreateIntegratedView(
    const Integration::ViewHostOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Runtime environment must be initialized before creating a view");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : *allocator_;
    Base::Result<Base::Ref<View>> made =
        Base::MakeRefWithAllocator<View>(
            selected,
            View::ConstructionToken{},
            *this,
            &selected);
    if (!made) return made.GetStatus();
    Base::Result<void> initialized =
        made.Value()->Initialize(options);
    if (!initialized) return initialized.GetStatus();
    return std::move(made).Value();
}

bool RuntimeEnvironment::IsInitialized() const noexcept {
    const Impl& state = static_cast<const Impl&>(*impl_);
    return state.initialized && state.schema.IsFrozen();
}

View::View(
    ConstructionToken,
    RuntimeEnvironment& environment,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    RuntimeEnvironment::Impl& environmentState =
        static_cast<RuntimeEnvironment::Impl&>(
            *environment.impl_);
    void* memory = allocator_->Allocate({
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Object});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl),
            alignof(Impl),
            Base::MemoryTag::Object);
    }
    impl_ = new (memory) Impl(
        environment.impl_,
        environmentState.schema,
        environmentState.documents,
        allocator_);
}

View::~View() noexcept {
    if (impl_ == nullptr) return;
    impl_->runtime.Shutdown();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Object);
    impl_ = nullptr;
}

Base::Result<void> View::Initialize(
    const Integration::ViewHostOptions& options) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "View has no runtime implementation");
    }
    RuntimeEnvironment::Impl& environmentState =
        static_cast<RuntimeEnvironment::Impl&>(
            *impl_->environment);
    if (!environmentState.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Runtime environment must be initialized before creating a view");
    }
    return impl_->runtime.Initialize(options);
}

Base::Result<UiDocument> View::Load(
    Base::StringView uri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    return impl_->runtime.Load(uri, diagnostics);
}

Base::Result<UiDocument> View::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    return impl_->runtime.Parse(source, baseUri, diagnostics);
}

Base::Result<void> View::SetContent(
    UiDocument&& document,
    Aero::Size availableSize) noexcept {
    return impl_->runtime.IsMounted()
        ? impl_->runtime.ReplaceMountedDocument(
              std::move(document), availableSize)
        : impl_->runtime.Mount(
              std::move(document), availableSize);
}

Base::Result<void> View::MountContent(
    Controls::ContentControl& host,
    UiDocument&& document) noexcept {
    return impl_ != nullptr
        ? impl_->runtime.MountContent(host, std::move(document))
        : Base::Result<void>(
              Base::Status::Failure(
                  Base::ErrorCode::NotInitialized,
                  "View is not initialized"));
}

Base::Result<void> View::UnmountContent(
    Controls::ContentControl& host) noexcept {
    return impl_ != nullptr
        ? impl_->runtime.UnmountContent(host)
        : Base::Result<void>(
              Base::Status::Failure(
                  Base::ErrorCode::NotInitialized,
                  "View is not initialized"));
}

Base::Result<void> View::LoadContent(
    Base::StringView uri,
    Aero::Size availableSize,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<UiDocument> loaded =
        Load(uri, diagnostics);
    if (!loaded) return loaded.GetStatus();
    return SetContent(
        std::move(loaded).Value(), availableSize);
}

Base::Result<UiDocument> View::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    return impl_->runtime.LoadCompiled(bytes, originUri);
}

Base::Result<void> View::LoadResources(
    RuntimeResourceLayer layer,
    Base::StringView uri,
    RuntimeResourceLoadMode mode,
    Core::IDiagnosticSink* diagnostics) noexcept {
    return impl_->runtime.LoadResources(
        layer, uri, mode, diagnostics);
}

Base::Result<void> View::LoadCompiledResources(
    RuntimeResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    RuntimeResourceLoadMode mode) noexcept {
    return impl_->runtime.LoadCompiledResources(
        layer, bytes, originUri, mode);
}

Base::Result<void> View::SetResourceDictionary(
    RuntimeResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    RuntimeResourceLoadMode mode) noexcept {
    return impl_ != nullptr
        ? impl_->runtime.SetResourceDictionary(
              layer, dictionary, mode)
        : Base::Result<void>(
              Base::Status::Failure(
                  Base::ErrorCode::NotInitialized,
                  "View is not initialized"));
}

Base::Result<void> View::LoadBuiltInTheme(
    BuiltInTheme theme) noexcept {
    return impl_->runtime.LoadBuiltInTheme(theme);
}

Base::Result<void> View::Resize(
    Aero::Size availableSize) noexcept {
    return impl_->runtime.Resize(availableSize);
}

Base::Result<void> View::Unmount() noexcept {
    return impl_->runtime.Unmount();
}

Base::Result<ViewFrameResult> View::RunFrame() noexcept {
    return impl_->runtime.RunFrame();
}

Base::Result<Input::PointerDispatchResult>
View::DispatchPointer(
    const Input::PointerInput& input) noexcept {
    return impl_->runtime.DispatchPointer(input);
}

Base::Result<Input::KeyboardDispatchResult>
View::DispatchKeyboard(
    const Input::KeyboardInput& input) noexcept {
    return impl_->runtime.DispatchKeyboard(input);
}

Base::Result<Input::TextInputDispatchResult>
View::DispatchText(
    const Input::TextInput& input) noexcept {
    return impl_->runtime.DispatchText(input);
}

Base::Result<std::uint32_t> View::AdvanceTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    return impl_->runtime.AdvanceTime(elapsedMilliseconds);
}

Base::Result<std::uint32_t> View::AdvanceAnimationTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    return impl_->runtime.AdvanceAnimationTime(
        elapsedMilliseconds);
}

Base::Result<void>
View::SetRenderBatchingEnabledForTesting(
    bool enabled) noexcept {
    return impl_ != nullptr
        ? impl_->runtime.
              SetRenderBatchingEnabledForTesting(
                  enabled)
        : Base::Result<void>(
              Base::Status::Failure(
                  Base::ErrorCode::NotInitialized,
                  "View is not initialized"));
}

Base::Result<void> View::SetRenderEndpoint(
    Base::Ref<Integration::RenderEndpoint> endpoint,
    bool automaticAnimationClock) noexcept {
    return impl_ != nullptr
        ? impl_->runtime.SetRenderEndpoint(
              std::move(endpoint),
              automaticAnimationClock)
        : Base::Result<void>(
              Base::Status::Failure(
                  Base::ErrorCode::NotInitialized,
                  "View is not initialized"));
}

const Base::Ref<Base::Object>& View::Root() const noexcept {
    return impl_->runtime.Root();
}

Base::Object* View::FindNamedObject(
    Base::StringView name,
    Core::TypeId expectedType) noexcept {
    return impl_->runtime.FindNamedObject(name, expectedType);
}

std::uint32_t View::NamedObjectCount() const noexcept {
    return impl_->runtime.NamedObjectCount();
}

void* View::IntegrationRuntime() noexcept {
    return impl_ != nullptr
        ? static_cast<void*>(&impl_->runtime)
        : nullptr;
}

Base::Result<void> View::RegisterSourceProvider(
    Integration::ISourceProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    return impl_->runtime.RegisterSourceProvider(
        provider,
        scheme,
        assembly);
}

Base::Result<Base::Ref<View>>
Integration::ViewHost::CreateView(
    RuntimeEnvironment& environment,
    const ViewHostOptions& options,
    Base::IAllocator* allocator) noexcept {
    return environment.CreateIntegratedView(options, allocator);
}

Base::Result<void>
Integration::ViewHost::RegisterSourceProvider(
    Integration::ISourceProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    if (view_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ViewHost is not bound to a View");
    }
    return view_->RegisterSourceProvider(
        provider, scheme, assembly);
}

Base::Result<Integration::Source>
Integration::SourceProviderAdapter::Load(
    const Base::ResourceUri& uri) const noexcept {
    if (load_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Source provider has no load callback");
    }
    return load_(uri, context_);
}

Base::Result<std::uint64_t>
Integration::SourceProviderAdapter::Revision(
    const Base::ResourceUri& uri) const noexcept {
    return revision_ != nullptr
        ? revision_(uri, context_)
        : ISourceProvider::Revision(uri);
}

std::uint64_t
Integration::SourceProviderAdapter::CacheIdentity() const noexcept {
    return cacheIdentity_ != 0U
        ? cacheIdentity_
        : ISourceProvider::CacheIdentity();
}

} // namespace Aero
