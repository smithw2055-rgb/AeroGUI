#include <Aero/Markup/XamlSchemaContext.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Markup {
namespace {

constexpr const char* MessageSchemaNotFrozen =
    "XAML schema context must be frozen before use";
constexpr const char* MessageSchemaAlreadyFrozen =
    "XAML schema context is frozen";
constexpr const char* MessageInvalidMarkupExtension =
    "XAML markup-extension registration requires a flagged type and provider";
constexpr const char* MessageMissingMarkupExtension =
    "XAML markup-extension type has no registered value provider";

bool HasTypeFlag(Core::TypeFlags value, Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

} // namespace

Base::Result<void> XamlSchemaContext::TryRegisterMemberAdapter(
    const XamlMemberAdapterRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    if (registration.member == Core::InvalidMemberId ||
        (registration.set == nullptr && registration.setWithServices == nullptr) ||
        runtime_->Descriptors().FindProperty(registration.member) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member adapter registration is invalid");
    }
    if (FindMemberAdapter(registration.member) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML member adapter is already registered");
    }
    return memberAdapters_.TryPushBack(registration);
}

Base::Result<void> XamlSchemaContext::TryRegisterMemberProvider(
    const XamlMemberProviderRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    if (registration.handles == nullptr || registration.set == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member provider registration is invalid");
    }
    for (const XamlMemberProviderRegistration& current : memberProviders_) {
        if (current.handles == registration.handles &&
            current.set == registration.set &&
            current.context == registration.context) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML member provider is already registered");
        }
    }
    return memberProviders_.TryPushBack(registration);
}

Base::Result<void> XamlSchemaContext::TryRegisterTypeAdapter(
    const XamlTypeAdapterRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    const Core::MetadataTypeDescriptor* type =
        runtime_->Descriptors().FindType(registration.type);
    if (type == nullptr ||
        HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML type adapter registration is invalid");
    }
    if (FindTypeAdapterExact(registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML type adapter is already registered");
    }
    return typeAdapters_.TryPushBack(registration);
}

Base::Result<void> XamlSchemaContext::TryRegisterMarkupExtension(
    const XamlMarkupExtensionRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    const Core::MetadataTypeDescriptor* type =
        runtime_->Descriptors().FindType(registration.type);
    if (type == nullptr || registration.provideValue == nullptr ||
        !HasTypeFlag(type->Flags(), Core::TypeFlags::MarkupExtension)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidMarkupExtension);
    }
    if (FindMarkupExtension(registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML markup-extension provider is already registered");
    }
    return markupExtensions_.TryPushBack(registration);
}

Base::Result<void> XamlSchemaContext::Freeze() noexcept {
    if (frozen_) return {};
    if (domain_ == nullptr || runtime_ == nullptr ||
        !domain_->IsSealed() || !runtime_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetadataDomain and MetadataRuntime must be sealed before XAML schema freeze");
    }
    frozen_ = true;
    return {};
}

Base::Result<XamlValue> XamlSchemaContext::ConvertText(
    Core::TypeId type,
    Base::StringView text) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    return runtime_->TryConvertText(type, text);
}

Base::Result<XamlValue> XamlSchemaContext::ProvideMarkupExtensionValue(
    Core::TypeId type,
    Base::StringView arguments,
    const XamlServiceProvider& services) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    const Core::MetadataTypeDescriptor* info =
        runtime_->Descriptors().FindType(type);
    const XamlMarkupExtensionRegistration* registration =
        FindMarkupExtension(type);
    if (info == nullptr ||
        !HasTypeFlag(info->Flags(), Core::TypeFlags::MarkupExtension) ||
        registration == nullptr || registration->provideValue == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageMissingMarkupExtension);
    }
    return registration->provideValue(
        arguments,
        services,
        registration->context);
}

Base::Result<void> XamlSchemaContext::BeginInit(
    Core::TypeId type,
    Base::Object& object) const noexcept {
    const XamlTypeAdapterRegistration* adapter = FindTypeAdapter(type);
    if (adapter == nullptr || adapter->beginInit == nullptr) return {};
    return adapter->beginInit(object, adapter->context);
}

Base::Result<void> XamlSchemaContext::EndInit(
    Core::TypeId type,
    Base::Object& object) const noexcept {
    const XamlTypeAdapterRegistration* adapter = FindTypeAdapter(type);
    if (adapter == nullptr || adapter->endInit == nullptr) return {};
    return adapter->endInit(object, adapter->context);
}

void XamlSchemaContext::AbortInit(
    Core::TypeId type,
    Base::Object& object) const noexcept {
    const XamlTypeAdapterRegistration* adapter = FindTypeAdapter(type);
    if (adapter != nullptr && adapter->abortInit != nullptr) {
        adapter->abortInit(object, adapter->context);
    }
}

bool XamlSchemaContext::CreatesNameScope(Core::TypeId type) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapterExact(type);
    return adapter != nullptr && adapter->createsNameScope;
}

bool XamlSchemaContext::CreatesResourceScope(
    Core::TypeId type) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapterExact(type);
    return adapter != nullptr && adapter->createsResourceScope;
}

Base::Result<void> XamlSchemaContext::RegisterName(
    Core::TypeId scopeType,
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapter(scopeType);
    if (adapter == nullptr || adapter->registerName == nullptr) return {};
    return adapter->registerName(
        scopeOwner,
        name,
        object,
        adapter->context);
}

Base::Result<void> XamlSchemaContext::AddResource(
    Core::TypeId scopeType,
    Base::Object& scopeOwner,
    Base::StringView key,
    Core::TypeId valueType,
    const Base::Ref<Base::Object>& value) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapter(scopeType);
    if (adapter == nullptr || adapter->addResource == nullptr) return {};
    return adapter->addResource(
        scopeOwner,
        key,
        valueType,
        value,
        adapter->context);
}

const XamlMemberAdapterRegistration* XamlSchemaContext::FindMemberAdapter(
    Core::MemberId member) const noexcept {
    for (const XamlMemberAdapterRegistration& adapter : memberAdapters_) {
        if (adapter.member == member) return &adapter;
    }
    return nullptr;
}

const XamlTypeAdapterRegistration* XamlSchemaContext::FindTypeAdapter(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        const XamlTypeAdapterRegistration* adapter =
            FindTypeAdapterExact(current);
        if (adapter != nullptr) return adapter;
        const Core::MetadataTypeDescriptor* info =
            runtime_->Descriptors().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

const XamlMarkupExtensionRegistration*
XamlSchemaContext::FindMarkupExtension(Core::TypeId type) const noexcept {
    for (const XamlMarkupExtensionRegistration& registration :
         markupExtensions_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const XamlMemberProviderRegistration* XamlSchemaContext::FindMemberProvider(
    const XamlResolvedMember& member) const noexcept {
    if (!member.IsValid()) return nullptr;
    for (const XamlMemberProviderRegistration& provider : memberProviders_) {
        if (provider.handles != nullptr &&
            provider.handles(member, provider.context)) {
            return &provider;
        }
    }
    return nullptr;
}

const XamlTypeAdapterRegistration*
XamlSchemaContext::FindTypeAdapterExact(Core::TypeId type) const noexcept {
    for (const XamlTypeAdapterRegistration& adapter : typeAdapters_) {
        if (adapter.type == type) return &adapter;
    }
    return nullptr;
}


} // namespace Aero::Markup
