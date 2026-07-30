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

template<class T, class ExactLookup>
const T* FindByPolicy(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors,
    ExactLookup&& lookup) noexcept {
    if constexpr (
        T::InheritancePolicy ==
        XamlFacetInheritancePolicy::ExactOnly) {
        return lookup(type);
    } else {
        return FindInherited<T>(
            type, descriptors,
            std::forward<ExactLookup>(lookup));
    }
}

template<class T>
const T* FindExact(
    const Base::Vector<T>& values,
    const Base::HashMap<Core::TypeId, std::uint32_t>& index,
    bool frozen,
    Core::TypeId type) noexcept {
    if (frozen) {
        const std::uint32_t* position = index.Find(type);
        return position != nullptr && *position < values.Size()
            ? &values[*position]
            : nullptr;
    }
    for (const T& value : values) {
        if (value.type == type) return &value;
    }
    return nullptr;
}

template<class T>
const T* FindExactLinear(
    const Base::Vector<T>& values,
    Core::TypeId type) noexcept {
    for (const T& value : values) {
        if (value.type == type) return &value;
    }
    return nullptr;
}

template<class T>
Base::Result<void> BuildIndex(
    const Base::Vector<T>& values,
    Base::HashMap<Core::TypeId, std::uint32_t>& index) noexcept {
    index.Clear();
    Base::Result<void> reserved = index.TryReserve(values.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t position = 0U;
         position < values.Size();
         ++position) {
        Base::Result<
            Base::HashMap<Core::TypeId, std::uint32_t>::InsertResult>
            inserted = index.TryInsert(values[position].type, position);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML facet index contains a duplicate type");
        }
    }
    return {};
}

template<class T>
void RollbackTo(
    Base::Vector<T>& values,
    std::uint32_t size) noexcept {
    while (values.Size() > size) {
        values.PopBack();
    }
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
    Base::Result<void> valid =
        ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();

    const std::uint32_t lifecycleSize = lifecycles_.Size();
    const std::uint32_t nameScopeSize = nameScopes_.Size();
    const std::uint32_t resourceScopeSize = resourceScopes_.Size();
    const std::uint32_t deferredSize = deferredContents_.Size();
    const std::uint32_t implicitKeySize = implicitResourceKeys_.Size();
    const std::uint32_t propertyTargetSize = propertyTargets_.Size();

    const auto rollback = [this,
        lifecycleSize,
        nameScopeSize,
        resourceScopeSize,
        deferredSize,
        implicitKeySize,
        propertyTargetSize]() noexcept {
        RollbackTo(lifecycles_, lifecycleSize);
        RollbackTo(nameScopes_, nameScopeSize);
        RollbackTo(resourceScopes_, resourceScopeSize);
        RollbackTo(deferredContents_, deferredSize);
        RollbackTo(implicitResourceKeys_, implicitKeySize);
        RollbackTo(propertyTargets_, propertyTargetSize);
    };

    bool projected = false;
    Base::Result<void> added;
    if (facet.beginInit != nullptr || facet.endInit != nullptr ||
        facet.abortInit != nullptr ||
        facet.endInitWithServices != nullptr) {
        added = TryAdd(XamlLifecycleFacet{
            facet.type,
            facet.beginInit,
            facet.endInit,
            facet.abortInit,
            facet.endInitWithServices,
            facet.context}, descriptors);
        if (!added) {
            rollback();
            return added.GetStatus();
        }
        projected = true;
    }
    if (facet.createsNameScope || facet.registerName != nullptr) {
        added = TryAdd(XamlNameScopeFacet{
            facet.type,
            facet.createsNameScope,
            facet.registerName,
            facet.context}, descriptors);
        if (!added) {
            rollback();
            return added.GetStatus();
        }
        projected = true;
    }
    if (facet.createsResourceScope || facet.addResource != nullptr ||
        facet.resolveResourceScope != nullptr) {
        added = TryAdd(XamlResourceScopeFacet{
            facet.type,
            facet.createsResourceScope,
            facet.addResource,
            facet.resolveResourceScope,
            facet.context}, descriptors);
        if (!added) {
            rollback();
            return added.GetStatus();
        }
        projected = true;
    }
    if (facet.defersVisualContent) {
        added = TryAdd(
            XamlDeferredContentFacet{facet.type, true},
            descriptors);
        if (!added) {
            rollback();
            return added.GetStatus();
        }
        projected = true;
    }
    if (facet.resolveImplicitResourceKey != nullptr) {
        added = TryAdd(XamlImplicitResourceKeyFacet{
            facet.type,
            facet.resolveImplicitResourceKey,
            facet.context}, descriptors);
        if (!added) {
            rollback();
            return added.GetStatus();
        }
        projected = true;
    }
    if (facet.resolvePropertyTarget != nullptr) {
        added = TryAdd(XamlPropertyTargetFacet{
            facet.type,
            facet.resolvePropertyTarget,
            facet.context}, descriptors);
        if (!added) {
            rollback();
            return added.GetStatus();
        }
        projected = true;
    }

    if (!projected) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML aggregate facet contains no capabilities");
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
    if (FindExactLinear(storage, facet.type) != nullptr) { \
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

    Base::Result<void> indexed =
        BuildIndex(lifecycles_, lifecycleIndex_);
    if (!indexed) return indexed.GetStatus();
    indexed = BuildIndex(nameScopes_, nameScopeIndex_);
    if (!indexed) return indexed.GetStatus();
    indexed = BuildIndex(resourceScopes_, resourceScopeIndex_);
    if (!indexed) return indexed.GetStatus();
    indexed = BuildIndex(deferredContents_, deferredContentIndex_);
    if (!indexed) return indexed.GetStatus();
    indexed = BuildIndex(implicitResourceKeys_, implicitResourceKeyIndex_);
    if (!indexed) return indexed.GetStatus();
    indexed = BuildIndex(propertyTargets_, propertyTargetIndex_);
    if (!indexed) return indexed.GetStatus();
    indexed = BuildIndex(markupExtensions_, markupExtensionIndex_);
    if (!indexed) return indexed.GetStatus();

    frozen_ = true;
    return {};
}

Base::Result<void> XamlFacetStore::CollectLifecycle(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors,
    Base::Vector<const XamlLifecycleFacet*>& output) const noexcept {
    output.Clear();
    if (type == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Lifecycle facet collection requires a valid type");
    }

    Core::TypeId current = type;
    std::uint32_t depth = 0U;
    while (current != Core::InvalidTypeId &&
           depth <= descriptors.TypeCount()) {
        const XamlLifecycleFacet* facet =
            FindLifecycleExact(current);
        if (facet != nullptr) {
            Base::Result<void> added = output.TryPushBack(facet);
            if (!added) {
                output.Clear();
                return added.GetStatus();
            }
        }
        const Core::TypeInfo* descriptor =
            descriptors.FindType(current);
        if (descriptor == nullptr) break;
        current = descriptor->BaseType();
        ++depth;
    }
    if (current != Core::InvalidTypeId) {
        output.Clear();
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "Lifecycle facet inheritance chain contains a cycle");
    }

    for (std::uint32_t left = 0U,
         right = output.Size(); left < right / 2U; ++left) {
        const std::uint32_t opposite = right - left - 1U;
        const XamlLifecycleFacet* temporary = output[left];
        output[left] = output[opposite];
        output[opposite] = temporary;
    }
    return {};
}

const XamlLifecycleFacet* XamlFacetStore::FindLifecycle(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlLifecycleFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindLifecycleExact(current);
        });
}

const XamlNameScopeFacet* XamlFacetStore::FindNameScope(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlNameScopeFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindNameScopeExact(current);
        });
}

const XamlResourceScopeFacet* XamlFacetStore::FindResourceScope(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlResourceScopeFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindResourceScopeExact(current);
        });
}

const XamlDeferredContentFacet* XamlFacetStore::FindDeferredContent(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlDeferredContentFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindDeferredContentExact(current);
        });
}

const XamlImplicitResourceKeyFacet*
XamlFacetStore::FindImplicitResourceKey(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlImplicitResourceKeyFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindImplicitResourceKeyExact(current);
        });
}

const XamlPropertyTargetFacet* XamlFacetStore::FindPropertyTarget(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlPropertyTargetFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindPropertyTargetExact(current);
        });
}

const XamlMarkupExtensionFacet* XamlFacetStore::FindMarkupExtension(
    Core::TypeId type) const noexcept {
    return FindExact(
        markupExtensions_, markupExtensionIndex_, frozen_, type);
}

const XamlLifecycleFacet* XamlFacetStore::FindLifecycleExact(
    Core::TypeId type) const noexcept {
    return FindExact(lifecycles_, lifecycleIndex_, frozen_, type);
}

const XamlNameScopeFacet* XamlFacetStore::FindNameScopeExact(
    Core::TypeId type) const noexcept {
    return FindExact(nameScopes_, nameScopeIndex_, frozen_, type);
}

const XamlResourceScopeFacet* XamlFacetStore::FindResourceScopeExact(
    Core::TypeId type) const noexcept {
    return FindExact(resourceScopes_, resourceScopeIndex_, frozen_, type);
}

const XamlDeferredContentFacet*
XamlFacetStore::FindDeferredContentExact(
    Core::TypeId type) const noexcept {
    return FindExact(
        deferredContents_, deferredContentIndex_, frozen_, type);
}

const XamlImplicitResourceKeyFacet*
XamlFacetStore::FindImplicitResourceKeyExact(
    Core::TypeId type) const noexcept {
    return FindExact(
        implicitResourceKeys_, implicitResourceKeyIndex_, frozen_, type);
}

const XamlPropertyTargetFacet*
XamlFacetStore::FindPropertyTargetExact(
    Core::TypeId type) const noexcept {
    return FindExact(
        propertyTargets_, propertyTargetIndex_, frozen_, type);
}

} // namespace Aero::Markup::Detail
