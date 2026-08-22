#include <Aero/Base/Assert.hpp>

#include "gui/meta/MetadataState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"

#include <new>
#include <utility>

namespace Aero {
namespace {

template<class T, class... TArgs>
Base::Result<T*> Create(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    TArgs&&... arguments) noexcept {
    void* memory = allocator.Allocate({sizeof(T), alignof(T), tag});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Schema bundle service allocation failed");
    }
    return new (memory) T(std::forward<TArgs>(arguments)...);
}

template<class T>
void Destroy(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& value) noexcept {
    if (value == nullptr) return;
    value->~T();
    allocator.Deallocate(value, sizeof(T), alignof(T), tag);
    value = nullptr;
}

Base::Status InvalidBundleState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

} // namespace

struct GuiSchemaState {
    explicit GuiSchemaState(Base::IAllocator& value) noexcept
        : allocator(&value) {}

    Base::IAllocator* allocator = nullptr;
    ::Aero::Meta::Registry metadata;
    const ModuleSet* modules = nullptr;
    Markup::Schema* schema = nullptr;
    Markup::DynamicResourceExtension* dynamicResource = nullptr;
    Markup::BindingExtension* binding = nullptr;
    Markup::UiObjectModel* uiObjectModel = nullptr;
    Markup::ResourceExtension resourceExtension;
    Markup::StaticExtension staticExtension;
    Markup::TypeExtension typeExtension;
    Markup::LocExtension locExtension;
    Markup::TemplateBindingExtension templateBindingExtension;
    bool prepared = false;
    bool frozen = false;
    bool terminal = false;

    ~GuiSchemaState() noexcept {
        Destroy(*allocator, Base::MemoryTag::Markup, uiObjectModel);
        Destroy(*allocator, Base::MemoryTag::Markup, binding);
        Destroy(*allocator, Base::MemoryTag::Markup, dynamicResource);
        Destroy(*allocator, Base::MemoryTag::Markup, schema);
    }
};

static_assert(
    sizeof(GuiSchemaState) <= 65536,
    "GuiSchema inline state storage is too small");
static_assert(
    alignof(GuiSchemaState) <= alignof(std::max_align_t),
    "GuiSchema inline state alignment is insufficient");

GuiSchema::GuiSchema(Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_) GuiSchemaState(*allocator_);
}

GuiSchema::~GuiSchema() noexcept {
    if (state_ == nullptr) return;
    state_->~GuiSchemaState();
    state_ = nullptr;
}

Base::Result<void> GuiSchema::Prepare(
    const ModuleSet& modules) noexcept {
    if (state_ == nullptr || state_->terminal) {
        return InvalidBundleState("Schema bundle is terminal");
    }
    if (state_->prepared) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Schema bundle metadata is already prepared");
    }
    Base::Result<void> status = modules.RegisterMetadata(state_->metadata);
    if (status) status = state_->metadata.Seal();
    if (!status) {
        state_->terminal = true;
        return status.GetStatus();
    }
    state_->prepared = true;
    state_->modules = &modules;
    return {};
}

Base::Result<void> GuiSchema::Finalize(
    const GuiSchemaOptions& requested) noexcept {
    if (state_ == nullptr || state_->terminal || !state_->prepared) {
        return InvalidBundleState(
            "Schema bundle metadata must be prepared before XAML finalization");
    }
    if (state_->frozen) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Schema bundle is already frozen");
    }
    Base::IAllocator& programAllocator = requested.allocator != nullptr
        ? *requested.allocator
        : *state_->allocator;

    Base::Result<Markup::Schema*> schema =
        Create<Markup::Schema>(
            *state_->allocator,
            Base::MemoryTag::Markup,
            state_->metadata,
            state_->allocator);
    if (!schema) {
        state_->terminal = true;
        return schema.GetStatus();
    }
    state_->schema = schema.Value();

    Base::Result<Markup::DynamicResourceExtension*> dynamicResource =
        Create<Markup::DynamicResourceExtension>(
            *state_->allocator,
            Base::MemoryTag::Markup,
            Markup::DynamicResourceExtensionOptions{});
    if (!dynamicResource) {
        state_->terminal = true;
        return dynamicResource.GetStatus();
    }
    state_->dynamicResource = dynamicResource.Value();

    Base::Result<Markup::BindingExtension*> binding =
        Create<Markup::BindingExtension>(
            *state_->allocator,
            Base::MemoryTag::Markup,
            Markup::BindingExtensionOptions{
                nullptr,
                Aero::FrameworkElement::DataContextProperty.Handle()});
    if (!binding) {
        state_->terminal = true;
        return binding.GetStatus();
    }
    state_->binding = binding.Value();

    Base::Result<Markup::UiObjectModel*> uiObjectModel =
        Create<Markup::UiObjectModel>(
            *state_->allocator,
            Base::MemoryTag::Markup,
            Markup::UiObjectModelOptions{
                &state_->metadata,
                &::Aero::MetadataPrivate::
                    DependencyProperties(state_->metadata),
                &programAllocator});
    if (!uiObjectModel) {
        state_->terminal = true;
        return uiObjectModel.GetStatus();
    }
    state_->uiObjectModel = uiObjectModel.Value();

    Base::Result<void> status =
        state_->resourceExtension.Register(*state_->schema);
    if (status) {
        status = state_->dynamicResource->Register(
            *state_->schema,
            Meta::MakeTypeId(
                Base::StringView("urn:aero"),
                Base::StringView("DynamicResource")));
    }
    if (status) {
        status = state_->binding->Register(
            *state_->schema,
            Meta::MakeTypeId(
                Base::StringView("urn:aero"),
                Base::StringView("Binding")));
    }
    if (status) {
        status = state_->staticExtension.Register(
            *state_->schema,
            Meta::MakeTypeId(
                Markup::LanguageNamespaceUri(),
                Base::StringView("Static")));
    }
    if (status) {
        status = state_->typeExtension.Register(
            *state_->schema,
            Meta::MakeTypeId(
                Markup::LanguageNamespaceUri(),
                Base::StringView("Type")));
    }
    if (status) {
        status = state_->locExtension.Register(
            *state_->schema,
            Meta::MakeTypeId(
                Meta::AeroNamespaceUri(),
                Base::StringView("Loc")));
    }
    if (status) {
        status = state_->templateBindingExtension.Register(
            *state_->schema,
            Meta::MakeTypeId(
                Meta::AeroNamespaceUri(),
                Base::StringView("TemplateBinding")));
    }
    if (status) {
        status = state_->uiObjectModel->Register(*state_->schema);
    }
    if (status && state_->modules != nullptr) {
        status = state_->modules->RegisterResourceScopes(*state_->schema);
    }
    if (status) status = state_->metadata.Complete();
    if (status) status = state_->schema->Freeze();
    if (!status) {
        state_->terminal = true;
        return status.GetStatus();
    }
    state_->frozen = true;
    return {};
}

bool GuiSchema::IsPrepared() const noexcept {
    return state_ != nullptr && state_->prepared;
}

bool GuiSchema::IsFrozen() const noexcept {
    return state_ != nullptr && state_->frozen;
}

::Aero::Meta::Registry& GuiSchema::Metadata() noexcept {
    AERO_ASSERT(state_ != nullptr && state_->prepared);
    return state_->metadata;
}

const ::Aero::Meta::Registry& GuiSchema::Metadata() const noexcept {
    AERO_ASSERT(state_ != nullptr && state_->prepared);
    return state_->metadata;
}

Markup::Schema& GuiSchema::Schema() noexcept {
    AERO_ASSERT(state_ != nullptr && state_->schema != nullptr);
    return *state_->schema;
}

const Markup::Schema& GuiSchema::Schema() const noexcept {
    AERO_ASSERT(state_ != nullptr && state_->schema != nullptr);
    return *state_->schema;
}

} // namespace Aero
