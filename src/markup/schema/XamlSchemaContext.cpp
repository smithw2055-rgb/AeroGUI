#include <Aero/Markup/Schema/XamlSchemaContext.hpp>

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
    return xamlFacets_.TryAdd(registration, runtime_->Descriptors());
}

Base::Result<void> XamlSchemaContext::TryRegisterMemberProvider(
    const XamlMemberProviderRegistration& registration) noexcept {
    return xamlFacets_.TryAdd(registration);
}

Base::Result<void> XamlSchemaContext::TryRegisterTypeAdapter(
    const XamlTypeAdapterRegistration& registration) noexcept {
    return xamlFacets_.TryAdd(registration, runtime_->Descriptors());
}

Base::Result<void> XamlSchemaContext::TryRegisterMarkupExtension(
    const XamlMarkupExtensionRegistration& registration) noexcept {
    return xamlFacets_.TryAdd(registration, runtime_->Descriptors());
}

Base::Result<void> XamlSchemaContext::Freeze() noexcept {
    if (frozen_) return {};
    if (domain_ == nullptr || runtime_ == nullptr ||
        !domain_->IsSealed() || !runtime_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetadataDomain and MetadataRuntime must be sealed before XAML schema freeze");
    }
    Base::Result<void> facetsFrozen = xamlFacets_.Freeze();
    if (!facetsFrozen) return facetsFrozen.GetStatus();
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

Base::Result<XamlProvidedValue> XamlSchemaContext::ProvideMarkupExtensionValue(
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
    Base::Object& object,
    const XamlServiceProvider& services) const noexcept {
    const XamlTypeAdapterRegistration* adapter = FindTypeAdapter(type);
    if (adapter == nullptr) return {};
    if (adapter->endInitWithServices != nullptr) {
        return adapter->endInitWithServices(
            object, services, adapter->context);
    }
    return adapter->endInit != nullptr
        ? adapter->endInit(object, adapter->context)
        : Base::Result<void>();
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
        FindTypeAdapter(type);
    return adapter != nullptr && adapter->createsNameScope;
}

bool XamlSchemaContext::CreatesResourceScope(
    Core::TypeId type) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapter(type);
    return adapter != nullptr && adapter->createsResourceScope;
}

bool XamlSchemaContext::DefersVisualContent(
    Core::TypeId type) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapter(type);
    return adapter != nullptr &&
        adapter->defersVisualContent;
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
    const XamlValue& value) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapter(scopeType);
    if (adapter == nullptr || adapter->addResource == nullptr) return {};
    return adapter->addResource(
        scopeOwner,
        key,
        value,
        adapter->context);
}

ResourceDictionary*
XamlSchemaContext::ResolveResourceScope(
    Core::TypeId scopeType,
    Base::Object& scopeOwner) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapter(scopeType);
    return adapter != nullptr &&
            adapter->resolveResourceScope != nullptr
        ? adapter->resolveResourceScope(
              scopeOwner,
              adapter->context)
        : nullptr;
}

Base::Result<ResourceKey> XamlSchemaContext::ResolveImplicitResourceKey(
    Core::TypeId type,
    const Base::Object& object) const noexcept {
    const XamlTypeAdapterRegistration* adapter = FindTypeAdapter(type);
    if (adapter == nullptr || adapter->resolveImplicitResourceKey == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML type has no implicit resource-key facet");
    }
    return adapter->resolveImplicitResourceKey(
        object, adapter->context);
}

const XamlMemberAdapterRegistration* XamlSchemaContext::FindMemberAdapter(
    Core::MemberId member) const noexcept {
    return xamlFacets_.FindMember(member);
}

const XamlTypeAdapterRegistration* XamlSchemaContext::FindTypeAdapter(
    Core::TypeId type) const noexcept {
    return xamlFacets_.FindType(type, runtime_->Descriptors());
}

const XamlMarkupExtensionRegistration*
XamlSchemaContext::FindMarkupExtension(Core::TypeId type) const noexcept {
    return xamlFacets_.FindMarkupExtension(type);
}

const XamlMemberProviderRegistration* XamlSchemaContext::FindMemberProvider(
    const XamlResolvedMember& member) const noexcept {
    return xamlFacets_.FindMemberProvider(member);
}


} // namespace Aero::Markup
