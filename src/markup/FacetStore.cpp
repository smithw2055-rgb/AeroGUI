#include "XamlFacetStore.hpp"

#include <cstdint>
#include <utility>

namespace Aero::Markup::Detail {
namespace {

bool HasTypeFlag(
    Core::TypeFlags value,
    Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status FrozenStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML facet store is frozen");
}

Base::Result<void> ValidateObjectType(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) noexcept {
    const Core::TypeInfo* descriptor =
        descriptors.FindType(type);
    if (descriptor == nullptr ||
        HasTypeFlag(descriptor->Flags(), Core::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML type capability requires a registered object type");
    }
    return {};
}

template<class T, class ExactLookup>
const T* FindInherited(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors,
    ExactLookup&& lookup) noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        const T* facet = lookup(current);
        if (facet != nullptr) return facet;
        const Core::TypeInfo* descriptor =
            descriptors.FindType(current);
        if (descriptor == nullptr) break;
        current = descriptor->BaseType();
    }
    return nullptr;
}

template<class T>
const T* FindExact(
    const Base::Vector<T>& values,
    Core::TypeId type) noexcept {
    for (const T& value : values) {
        if (value.type == type) return &value;
    }
    return nullptr;
}

} // namespace

Base::Result<void> XamlFacetStore::TryAdd(
    const XamlTypeFacet& facet,
    const Core::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.abiVersion != XamlFacetAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML type facet ABI version is incompatible");
    }
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();
    if (FindTypeExact(facet.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML compatibility type facet is already registered");
    }
    Base::Result<void> added = types_.TryPushBack(facet);
    if (!added) return added.GetStatus();

    if (facet.beginInit != nullptr || facet.endInit != nullptr ||
        facet.abortInit != nullptr || facet.endInitWithServices != nullptr) {
        added = TryAdd(XamlLifecycleFacet{
            facet.type,
            facet.beginInit,
            facet.endInit,
            facet.abortInit,
            facet.endInitWithServices,
            facet.context}, descriptors);
        if (!added) return added.GetStatus();
    }
    if (facet.createsNameScope || facet.registerName != nullptr) {
        added = TryAdd(XamlNameScopeFacet{
            facet.type,
            facet.createsNameScope,
            facet.registerName,
            facet.context}, descriptors);
        if (!added) return added.GetStatus();
    }
    if (facet.createsResourceScope || facet.addResource != nullptr ||
        facet.resolveResourceScope != nullptr) {
        added = TryAdd(XamlResourceScopeFacet{
            facet.type,
            facet.createsResourceScope,
            facet.addResource,
            facet.resolveResourceScope,
            facet.context}, descriptors);
        if (!added) return added.GetStatus();
    }
    if (facet.defersVisualContent) {
        added = TryAdd(XamlDeferredContentFacet{facet.type, true}, descriptors);
        if (!added) return added.GetStatus();
    }
    if (facet.resolveImplicitResourceKey != nullptr) {
        added = TryAdd(XamlImplicitResourceKeyFacet{
            facet.type,
            facet.resolveImplicitResourceKey,
            facet.context}, descriptors);
        if (!added) return added.GetStatus();
    }
    if (facet.resolvePropertyTarget != nullptr) {
        added = TryAdd(XamlPropertyTargetFacet{
            facet.type,
            facet.resolvePropertyTarget,
            facet.context}, descriptors);
        if (!added) return added.GetStatus();
    }
    return {};
}

#define AERO_DEFINE_TYPE_FACET_ADD(TypeName, storage, validExpression, message) \
Base::Result<void> XamlFacetStore::TryAdd( \
    const TypeName& facet, \
    const Core::TypeRegistry& descriptors) noexcept { \
    if (frozen_) return FrozenStatus(); \
    if (facet.abiVersion != XamlFacetAbiVersion) { \
        return Base::Status::Failure( \
            Base::ErrorCode::Unsupported, \
            "XAML capability facet ABI version is incompatible"); \
    } \
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors); \
    if (!valid) return valid.GetStatus(); \
    if (!(validExpression)) { \
        return Base::Status::Failure( \
            Base::ErrorCode::InvalidArgument, message); \
    } \
    if (FindExact(storage, facet.type) != nullptr) { \
        return Base::Status::Failure( \
            Base::ErrorCode::AlreadyExists, message); \
    } \
    return storage.TryPushBack(facet); \
}

AERO_DEFINE_TYPE_FACET_ADD(
    XamlLifecycleFacet,
    lifecycles_,
    facet.beginInit != nullptr || facet.endInit != nullptr ||
        facet.abortInit != nullptr || facet.endInitWithServices != nullptr,
    "XAML lifecycle facet is invalid or already registered")
AERO_DEFINE_TYPE_FACET_ADD(
    XamlNameScopeFacet,
    nameScopes_,
    facet.createsNameScope || facet.registerName != nullptr,
    "XAML name-scope facet is invalid or already registered")
AERO_DEFINE_TYPE_FACET_ADD(
    XamlResourceScopeFacet,
    resourceScopes_,
    facet.createsResourceScope || facet.addResource != nullptr ||
        facet.resolveResourceScope != nullptr,
    "XAML resource-scope facet is invalid or already registered")
AERO_DEFINE_TYPE_FACET_ADD(
    XamlDeferredContentFacet,
    deferredContents_,
    facet.defersVisualContent,
    "XAML deferred-content facet is invalid or already registered")
AERO_DEFINE_TYPE_FACET_ADD(
    XamlImplicitResourceKeyFacet,
    implicitResourceKeys_,
    facet.resolve != nullptr,
    "XAML implicit-resource-key facet is invalid or already registered")
AERO_DEFINE_TYPE_FACET_ADD(
    XamlPropertyTargetFacet,
    propertyTargets_,
    facet.resolve != nullptr,
    "XAML property-target facet is invalid or already registered")

#undef AERO_DEFINE_TYPE_FACET_ADD

Base::Result<void> XamlFacetStore::TryAdd(
    const XamlMarkupExtensionFacet& facet,
    const Core::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    const Core::TypeInfo* type =
        descriptors.FindType(facet.type);
    if (facet.abiVersion != XamlFacetAbiVersion ||
        type == nullptr || facet.provideValue == nullptr ||
        !HasTypeFlag(type->Flags(), Core::TypeFlags::MarkupExtension)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML markup-extension facet requires a flagged type and provider");
    }
    if (FindMarkupExtension(facet.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML markup-extension facet is already registered");
    }
    return markupExtensions_.TryPushBack(facet);
}

Base::Result<void> XamlFacetStore::Freeze() noexcept {
    if (frozen_) return {};

    Base::Result<void> reserved =
        markupExtensionIndex_.TryReserve(markupExtensions_.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U; index < markupExtensions_.Size(); ++index) {
        Base::Result<Base::HashMap<Core::TypeId, std::uint32_t>::InsertResult>
            inserted = markupExtensionIndex_.TryInsert(
                markupExtensions_[index].type, index);
        if (!inserted) return inserted.GetStatus();
    }

    frozen_ = true;
    return {};
}

const XamlTypeFacet* XamlFacetStore::FindType(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindInherited<XamlTypeFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindTypeExact(current);
        });
}

const XamlLifecycleFacet* XamlFacetStore::FindLifecycle(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindInherited<XamlLifecycleFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindLifecycleExact(current);
        });
}

const XamlNameScopeFacet* XamlFacetStore::FindNameScope(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindInherited<XamlNameScopeFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindNameScopeExact(current);
        });
}

const XamlResourceScopeFacet* XamlFacetStore::FindResourceScope(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindInherited<XamlResourceScopeFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindResourceScopeExact(current);
        });
}

const XamlDeferredContentFacet* XamlFacetStore::FindDeferredContent(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindInherited<XamlDeferredContentFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindDeferredContentExact(current);
        });
}

const XamlImplicitResourceKeyFacet* XamlFacetStore::FindImplicitResourceKey(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindInherited<XamlImplicitResourceKeyFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindImplicitResourceKeyExact(current);
        });
}

const XamlPropertyTargetFacet* XamlFacetStore::FindPropertyTarget(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindInherited<XamlPropertyTargetFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindPropertyTargetExact(current);
        });
}

const XamlMarkupExtensionFacet* XamlFacetStore::FindMarkupExtension(
    Core::TypeId type) const noexcept {
    if (frozen_) {
        const std::uint32_t* index = markupExtensionIndex_.Find(type);
        return index != nullptr && *index < markupExtensions_.Size()
            ? &markupExtensions_[*index]
            : nullptr;
    }
    for (const XamlMarkupExtensionFacet& facet : markupExtensions_) {
        if (facet.type == type) return &facet;
    }
    return nullptr;
}

const XamlTypeFacet* XamlFacetStore::FindTypeExact(
    Core::TypeId type) const noexcept {
    return FindExact(types_, type);
}

const XamlLifecycleFacet* XamlFacetStore::FindLifecycleExact(
    Core::TypeId type) const noexcept {
    return FindExact(lifecycles_, type);
}

const XamlNameScopeFacet* XamlFacetStore::FindNameScopeExact(
    Core::TypeId type) const noexcept {
    return FindExact(nameScopes_, type);
}

const XamlResourceScopeFacet* XamlFacetStore::FindResourceScopeExact(
    Core::TypeId type) const noexcept {
    return FindExact(resourceScopes_, type);
}

const XamlDeferredContentFacet* XamlFacetStore::FindDeferredContentExact(
    Core::TypeId type) const noexcept {
    return FindExact(deferredContents_, type);
}

const XamlImplicitResourceKeyFacet*
XamlFacetStore::FindImplicitResourceKeyExact(
    Core::TypeId type) const noexcept {
    return FindExact(implicitResourceKeys_, type);
}

const XamlPropertyTargetFacet* XamlFacetStore::FindPropertyTargetExact(
    Core::TypeId type) const noexcept {
    return FindExact(propertyTargets_, type);
}

} // namespace Aero::Markup::Detail
