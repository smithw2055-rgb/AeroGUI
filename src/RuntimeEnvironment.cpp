#include <Aero/RuntimeEnvironment.hpp>

#include <Aero/Markup/Loader.hpp>

#include <new>
#include <utility>

namespace Aero {
namespace {

class RuntimeEnvironmentState final : public Base::Object {
public:
    explicit RuntimeEnvironmentState(Base::IAllocator& value) noexcept
        : allocator(&value), schema(&value), documents(&value) {}
    ~RuntimeEnvironmentState() noexcept override = default;

    Base::IAllocator* allocator = nullptr;
    ModuleCatalog modules;
    SchemaBundle schema;
    Markup::DocumentCache documents;
    bool initialized = false;
};

RuntimeEnvironmentState& State(Base::Object& value) noexcept {
    return static_cast<RuntimeEnvironmentState&>(value);
}

} // namespace

RuntimeEnvironment::RuntimeEnvironment(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    Base::Result<Base::Ref<RuntimeEnvironmentState>> made =
        Base::MakeRefWithAllocator<RuntimeEnvironmentState>(
            *allocator_, *allocator_);
    if (!made) {
        Base::ReportOutOfMemory(
            sizeof(RuntimeEnvironmentState),
            alignof(RuntimeEnvironmentState),
            Base::MemoryTag::Object);
    }
    state_ = Base::Ref<Base::Object>(std::move(made).Value());
}

Base::Result<void> RuntimeEnvironment::AddModule(
    const ModuleRegistration& registration) noexcept {
    RuntimeEnvironmentState& state =
        *static_cast<RuntimeEnvironmentState*>(state_.Get());
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Runtime environment modules are frozen");
    }
    return state.modules.Add(registration);
}

Base::Result<void> RuntimeEnvironment::Initialize() noexcept {
    RuntimeEnvironmentState& state =
        *static_cast<RuntimeEnvironmentState*>(state_.Get());
    if (state.initialized) return {};
    Base::Result<void> prepared = state.schema.Prepare(state.modules);
    if (!prepared) return prepared.GetStatus();
    Base::Result<void> finalized = state.schema.Finalize(
        state.modules, SchemaBundleServices{state.allocator});
    if (!finalized) return finalized.GetStatus();
    Base::Result<void> frozen = state.modules.Freeze();
    if (!frozen) return frozen.GetStatus();
    state.initialized = true;
    return {};
}

Base::Result<Base::Ref<View>> RuntimeEnvironment::CreateView(
    const RuntimeHostOptions& options,
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
            selected, *this, &selected);
    if (!made) return made.GetStatus();
    Base::Result<void> initialized = made.Value()->Initialize(options);
    if (!initialized) return initialized.GetStatus();
    return std::move(made).Value();
}

bool RuntimeEnvironment::IsInitialized() const noexcept {
    const RuntimeEnvironmentState& state =
        *static_cast<RuntimeEnvironmentState*>(state_.Get());
    return state.initialized && state.schema.IsFrozen();
}

SchemaBundle& RuntimeEnvironment::Schema() noexcept {
    return static_cast<RuntimeEnvironmentState&>(*state_).schema;
}

const SchemaBundle& RuntimeEnvironment::Schema() const noexcept {
    return static_cast<const RuntimeEnvironmentState&>(*state_).schema;
}

Markup::DocumentCache& RuntimeEnvironment::Documents() noexcept {
    return static_cast<RuntimeEnvironmentState&>(*state_).documents;
}

const Markup::DocumentCache&
RuntimeEnvironment::Documents() const noexcept {
    return static_cast<const RuntimeEnvironmentState&>(*state_).documents;
}

View::View(
    RuntimeEnvironment& environment,
    Base::IAllocator* allocator) noexcept
    : environmentState_(environment.state_),
      host_(
          State(*environmentState_).schema,
          State(*environmentState_).documents,
          allocator) {}

Base::Result<void> View::Initialize(
    const RuntimeHostOptions& options) noexcept {
    if (!environmentState_ ||
        !State(*environmentState_).initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Runtime environment must be initialized before creating a view");
    }
    return host_.Initialize(options);
}

Base::Result<UiDocument> View::Load(
    Base::StringView uri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    return host_.Load(uri, diagnostics);
}

Base::Result<UiDocument> View::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    return host_.Parse(
        source, baseUri, diagnostics);
}

Base::Result<void> View::SetContent(
    UiDocument&& document,
    Presentation::Size availableSize) noexcept {
    return host_.IsMounted()
        ? host_.ReplaceMountedDocument(
              std::move(document), availableSize)
        : host_.Mount(
              std::move(document), availableSize);
}

Base::Result<void> View::LoadContent(
    Base::StringView uri,
    Presentation::Size availableSize,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<UiDocument> loaded =
        Load(uri, diagnostics);
    if (!loaded) return loaded.GetStatus();
    return SetContent(
        std::move(loaded).Value(), availableSize);
}

} // namespace Aero
