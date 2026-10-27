#include <Aero/Base/Assert.hpp>

#include "gui/markup/XamlObjectWriterState.hpp"

#include <cstddef>
#include <new>
#include <utility>

namespace Aero {
namespace {

Base::Status InvalidBundleState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

} // namespace

struct GuiSchema::State {
    explicit State(Base::IAllocator& value) noexcept
        : allocator(&value) {}

    Base::IAllocator* allocator = nullptr;
    ::Aero::Meta::Registry metadata;
    const ModuleSet* modules = nullptr;

    Markup::Schema* schema = nullptr;
    std::aligned_storage_t<sizeof(Markup::DynamicResourceExtension), alignof(Markup::DynamicResourceExtension)> dynamicResourceStorage;
    std::aligned_storage_t<sizeof(Markup::BindingExtension), alignof(Markup::BindingExtension)> bindingStorage;
    std::aligned_storage_t<sizeof(Markup::UiObjectModel), alignof(Markup::UiObjectModel)> uiObjectModelStorage;
    Markup::DynamicResourceExtension* dynamicResource = nullptr;
    Markup::BindingExtension* binding = nullptr;
    Markup::UiObjectModel* uiObjectModel = nullptr;
    bool prepared = false;
    bool frozen = false;
    bool terminal = false;

    ~State() noexcept {
        if (uiObjectModel != nullptr) {
            uiObjectModel->~UiObjectModel();
            uiObjectModel = nullptr;
        }
        if (binding != nullptr) {
            binding->~BindingExtension();
            binding = nullptr;
        }
        if (dynamicResource != nullptr) {
            dynamicResource->~DynamicResourceExtension();
            dynamicResource = nullptr;
        }
        if (schema != nullptr) {
            Base::FreeObject(*allocator, Base::MemoryTag::General, schema);
        }
    }
};

GuiSchema::GuiSchema(Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::General, state_, *allocator_));
}

GuiSchema::~GuiSchema() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::General, state_);
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

    Base::Result<void> status = Base::AllocateObject(
        *state_->allocator,
        Base::MemoryTag::General,
        state_->schema,
        state_->metadata,
        state_->allocator);
    if (!status) {
        state_->terminal = true;
        return status;
    }

    state_->dynamicResource = new (&state_->dynamicResourceStorage)
        Markup::DynamicResourceExtension(Markup::DynamicResourceExtensionOptions{});

    state_->binding = new (&state_->bindingStorage)
        Markup::BindingExtension(Markup::BindingExtensionOptions{
            nullptr,
            Aero::FrameworkElement::DataContextProperty.Handle()});

    state_->uiObjectModel = new (&state_->uiObjectModelStorage)
        Markup::UiObjectModel(Markup::UiObjectModelOptions{
            &state_->metadata,
            &(state_->metadata).DependencyProperties(),
            &programAllocator});

    status = state_->dynamicResource->Register(
        *state_->schema,
        Meta::MakeTypeId(
            Base::StringView("urn:aero"),
            Base::StringView("DynamicResource")));
    if (status) {
        status = state_->binding->Register(
            *state_->schema,
            Meta::MakeTypeId(
                Base::StringView("urn:aero"),
                Base::StringView("Binding")));
    }
    if (status) {
        status = Markup::RegisterBuiltinMarkupExtensions(*state_->schema);
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
