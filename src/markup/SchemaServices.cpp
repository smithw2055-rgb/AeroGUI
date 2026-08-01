#include "SchemaInternal.hpp"

#include <Aero/Meta/ValueCodec.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Markup {
using namespace Detail;

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

Base::Result<Core::TypeReference> ResolveTypeReference(
    Base::StringView name,
    const ExtensionContext& services) noexcept {
    if (services.schema == nullptr || name.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Type reference requires a qualified type name");
    }
    std::uint32_t colon = name.SizeBytes();
    for (std::uint32_t index = 0U;
         index < name.SizeBytes();
         ++index) {
        if (name[index] != ':') continue;
        if (colon != name.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Type reference contains multiple namespace prefixes");
        }
        colon = index;
    }
    Base::StringView prefix;
    Base::StringView localName = name;
    if (colon != name.SizeBytes()) {
        if (colon == 0U ||
            colon + 1U >= name.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Type reference namespace prefix is malformed");
        }
        prefix = name.Substr(0U, colon);
        localName = name.Substr(
            colon + 1U,
            name.SizeBytes() - colon - 1U);
    }
    Base::Result<Base::StringView> uri =
        services.namespaces.Lookup(prefix);
    if (!uri) return uri.GetStatus();
    Base::Result<const Core::TypeInfo*> resolved =
        services.schema->ResolveType(
            uri.Value(), localName);
    if (!resolved) return resolved.GetStatus();
    const Core::TypeInfo* type = resolved.Value();
    if (type == nullptr ||
        HasTypeFlag(
            type->Flags(),
            Core::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Type reference target was not found or is not an object type");
    }
    return Core::TypeReference{type->Id()};
}

} // namespace

Base::Result<void> Schema::Freeze() noexcept {
    if (frozen_) return {};
    if (domain_ == nullptr || runtime_ == nullptr ||
        !domain_->IsSealed() || !runtime_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetadataDomain and MetadataRuntime must be sealed before XAML schema freeze");
    }
    Base::Result<void> facetsFrozen = impl_->facets.Freeze();
    if (!facetsFrozen) return facetsFrozen.GetStatus();
    frozen_ = true;
    return {};
}

Base::Result<Core::Value> Schema::ConvertText(
    Core::TypeId type,
    Base::StringView text,
    const ExtensionContext* services) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    if (type == Core::TypeOf<Core::TypeReference>()) {
        if (services == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Type-reference conversion requires markup services");
        }
        Base::Result<Core::TypeReference> reference =
            ResolveTypeReference(text, *services);
        return reference
            ? Core::ValueCodec<Core::TypeReference>::Encode(
                  reference.Value())
            : Base::Result<Core::Value>(
                  reference.GetStatus());
    }
    // Members flagged AnyValue still need a concrete runtime value. Preserve
    // literal XAML text as a String so style and template finalizers can
    // convert it after resolving the actual target dependency property.
    if (type == Core::TypeOf<Core::Value>()) {
        return Core::Value::TryFromString(
            Core::TypeOf<Base::String>(), text);
    }
    if (type == Core::TypeOf<Base::ResourceUri>() &&
        services != nullptr &&
        services->baseUri != nullptr &&
        !services->baseUri->Empty()) {
        Base::Result<Base::ResourceUri> uri =
            Base::ResourceUri::Resolve(
                *services->baseUri,
                text);
        if (!uri) return uri.GetStatus();
        return runtime_->TryCreateValue(
            type,
            &uri.Value());
    }
    return runtime_->TryConvertText(type, text);
}

Base::Result<ProvidedValue> Schema::ProvideMarkupExtensionValue(
    Core::TypeId type,
    Base::StringView arguments,
    const ExtensionContext& services) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    const Core::TypeInfo* info =
        runtime_->Types().FindType(type);
    const XamlMarkupExtensionFacet* registration =
        impl_->facets.FindMarkupExtension(type);
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

Base::Result<void> Schema::BeginInit(
    Core::TypeId type,
    Base::Object& object) const noexcept {
    Base::Vector<const XamlLifecycleFacet*> lifecycle;
    Base::Result<void> collected =
        impl_->facets.CollectLifecycle(
            type, runtime_->Types(), lifecycle);
    if (!collected) return collected.GetStatus();

    for (const XamlLifecycleFacet* facet : lifecycle) {
        if (facet == nullptr || facet->beginInit == nullptr) continue;
        Base::Result<void> initialized =
            facet->beginInit(object, facet->context);
        if (!initialized) return initialized.GetStatus();
    }
    return {};
}

Base::Result<void> Schema::EndInit(
    Core::TypeId type,
    Base::Object& object,
    const ExtensionContext& services) const noexcept {
    Base::Vector<const XamlLifecycleFacet*> lifecycle;
    Base::Result<void> collected =
        impl_->facets.CollectLifecycle(
            type, runtime_->Types(), lifecycle);
    if (!collected) return collected.GetStatus();

    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            lifecycle[index - 1U];
        if (facet == nullptr) continue;
        Base::Result<void> initialized;
        if (facet->endInitWithServices != nullptr) {
            initialized = facet->endInitWithServices(
                object, services, facet->context);
        } else if (facet->endInit != nullptr) {
            initialized = facet->endInit(
                object, facet->context);
        }
        if (!initialized) return initialized.GetStatus();
    }
    return {};
}

void Schema::AbortInit(
    Core::TypeId type,
    Base::Object& object) const noexcept {
    Base::Vector<const XamlLifecycleFacet*> lifecycle;
    Base::Result<void> collected =
        impl_->facets.CollectLifecycle(
            type, runtime_->Types(), lifecycle);
    if (!collected) return;

    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            lifecycle[index - 1U];
        if (facet != nullptr && facet->abortInit != nullptr) {
            facet->abortInit(object, facet->context);
        }
    }
}

bool Schema::CreatesNameScope(Core::TypeId type) const noexcept {
    const XamlNameScopeFacet* facet = impl_->facets.FindNameScope(
        type, runtime_->Types());
    return facet != nullptr && facet->createsNameScope;
}

bool Schema::CreatesResourceScope(
    Core::TypeId type) const noexcept {
    const XamlResourceScopeFacet* facet = impl_->facets.FindResourceScope(
        type, runtime_->Types());
    return facet != nullptr && facet->createsResourceScope;
}

bool Schema::DefersVisualContent(
    Core::TypeId type) const noexcept {
    const XamlDeferredContentFacet* facet =
        impl_->facets.FindDeferredContent(
        type, runtime_->Types());
    return facet != nullptr && facet->defersVisualContent;
}

Base::Result<void> Schema::RegisterName(
    Core::TypeId scopeType,
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object) const noexcept {
    const XamlNameScopeFacet* facet = impl_->facets.FindNameScope(
        scopeType, runtime_->Types());
    if (facet == nullptr || facet->registerName == nullptr) return {};
    return facet->registerName(
        scopeOwner, name, object, facet->context);
}

Base::Result<void> Schema::AddResource(
    Core::TypeId scopeType,
    Base::Object& scopeOwner,
    const Aero::ResourceKey& key,
    const Core::Value& value) const noexcept {
    const XamlResourceScopeFacet* facet = impl_->facets.FindResourceScope(
        scopeType, runtime_->Types());
    if (facet == nullptr) return {};
    if (facet->addResource != nullptr) {
        return facet->addResource(
            scopeOwner, key, value, facet->context);
    }
    Aero::ResourceDictionary* resources =
        facet->resolveResourceScope != nullptr
        ? facet->resolveResourceScope(
              scopeOwner,
              facet->context)
        : nullptr;
    return resources != nullptr
        ? resources->TryAdd(key, value)
        : Base::Result<void>();
}

Aero::ResourceDictionary* Schema::ResolveResourceScope(
    Core::TypeId scopeType,
    Base::Object& scopeOwner) const noexcept {
    const XamlResourceScopeFacet* facet = impl_->facets.FindResourceScope(
        scopeType, runtime_->Types());
    return facet != nullptr && facet->resolveResourceScope != nullptr
        ? facet->resolveResourceScope(scopeOwner, facet->context)
        : nullptr;
}

Base::Result<Aero::ResourceKey>
Schema::ResolveImplicitResourceKey(
    Core::TypeId type,
    const Base::Object& object) const noexcept {
    const XamlImplicitResourceKeyFacet* facet =
        impl_->facets.FindImplicitResourceKey(
            type, runtime_->Types());
    if (facet == nullptr || facet->resolve == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML type has no implicit resource-key facet");
    }
    return facet->resolve(object, facet->context);
}

} // namespace Aero::Markup
