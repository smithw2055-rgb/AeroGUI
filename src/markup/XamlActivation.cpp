#include <Aero/Markup/XamlActivation.hpp>

#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {
namespace {

struct ActiveActivation final {
    XamlActivationProviderRegistry* providers = nullptr;
    const XamlActivationContext* context = nullptr;
};

thread_local ActiveActivation gActiveActivation;

bool HasTypeFlag(Core::TypeFlags value, Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

class ActiveActivationScope final {
public:
    ActiveActivationScope(
        XamlActivationProviderRegistry& providers,
        const XamlActivationContext& context) noexcept
        : previous_(gActiveActivation) {
        gActiveActivation.providers = &providers;
        gActiveActivation.context = &context;
    }

    ~ActiveActivationScope() {
        gActiveActivation = previous_;
    }

    ActiveActivationScope(const ActiveActivationScope&) = delete;
    ActiveActivationScope& operator=(const ActiveActivationScope&) = delete;

private:
    ActiveActivation previous_;
};

} // namespace

XamlActivationProviderRegistry::XamlActivationProviderRegistry(
    XamlSchemaContext& schema,
    Base::IAllocator* allocator) noexcept
    : schema_(&schema),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      providers_(allocator_) {}

Base::Result<void> XamlActivationProviderRegistry::TryRegister(
    const XamlActivationProviderRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML activation provider registry is frozen");
    }
    const Core::TypeInfo* type = schema_->Types().FindType(registration.type);
    if (type == nullptr ||
        HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType) ||
        registration.activate == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML activation provider registration is invalid");
    }
    if (FindExact(registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML activation provider is already registered");
    }
    return providers_.TryPushBack(registration);
}

Base::Result<void> XamlActivationProviderRegistry::Freeze() noexcept {
    if (frozen_) {
        return {};
    }
    if (!schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML schema context must be frozen before activation providers");
    }
    frozen_ = true;
    return {};
}

Base::Result<Base::Ref<Base::Object>>
XamlActivationProviderRegistry::CreateObject(
    Core::TypeId requestedType,
    const XamlActivationContext& activation) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML activation provider registry is not frozen");
    }
    if (!activation.IsCompatible()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML activation context is incompatible");
    }

    const XamlActivationProviderRegistration* provider = Find(requestedType);
    if (provider == nullptr) {
        return schema_->CreateObject(requestedType);
    }

    Base::Result<Base::Ref<Base::Object>> created = provider->activate(
        requestedType,
        activation,
        *allocator_,
        provider->context);
    if (!created) {
        return created.GetStatus();
    }
    if (!created.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML activation provider returned a null object");
    }
    return created;
}

const XamlActivationProviderRegistration*
XamlActivationProviderRegistry::FindExact(Core::TypeId type) const noexcept {
    for (const XamlActivationProviderRegistration& provider : providers_) {
        if (provider.type == type) {
            return &provider;
        }
    }
    return nullptr;
}

const XamlActivationProviderRegistration*
XamlActivationProviderRegistry::Find(Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        const XamlActivationProviderRegistration* provider = FindExact(current);
        if (provider != nullptr) {
            return provider;
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) {
            break;
        }
        current = info->BaseType();
    }
    return nullptr;
}

Base::Result<Base::Ref<Base::Object>> XamlSchemaContext::CreateObjectActivated(
    Core::TypeId type) const noexcept {
    if (gActiveActivation.providers == nullptr) {
        return CreateObject(type);
    }
    if (gActiveActivation.context == nullptr ||
        &gActiveActivation.providers->Schema() != this) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Active XAML activation provider belongs to another schema");
    }
    return gActiveActivation.providers->CreateObject(
        type,
        *gActiveActivation.context);
}

Base::Result<Base::Ref<Base::Object>> LoadXamlWithActivation(
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept {
    if (!activation.IsCompatible()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML activation context is incompatible");
    }
    if (!providers.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML activation provider registry is not frozen");
    }
    ActiveActivationScope scope(providers, activation);
    return writer.Load(reader);
}

} // namespace Aero::Markup
