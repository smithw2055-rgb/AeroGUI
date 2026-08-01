#include "XamlFacets.hpp"

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
    const Core::TypeInfo* descriptor = descriptors.FindType(type);
    if (descriptor == nullptr ||
        HasTypeFlag(descriptor->Flags(), Core::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML type capability requires a registered object type");
    }
    return {};
}

Base::Status InvalidFacet(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status DuplicateFacet(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::AlreadyExists, message);
}

template<class T, class ExactLookup>
const T* FindInherited(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors,
    ExactLookup&& lookup) noexcept {
    Core::TypeId current = type;
    std::uint32_t depth = 0U;
    while (current != Core::InvalidTypeId &&
           depth <= descriptors.TypeCount()) {
        const T* facet = lookup(current);
        if (facet != nullptr) return facet;
        const Core::TypeInfo* descriptor = descriptors.FindType(current);
        if (descriptor == nullptr) return nullptr;
        current = descriptor->BaseType();
        ++depth;
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

} // namespace

Base::Result<XamlFacets::TypeFacets*>
XamlFacets::EnsureType(Core::TypeId type) noexcept {
    TypeFacets* existing = FindTypeMutable(type);
    if (existing != nullptr) return existing;

    TypeFacets entry;
    entry.type = type;
    Base::Result<void> added = types_.TryPushBack(std::move(entry));
    if (!added) return added.GetStatus();
    return &types_[types_.Size() - 1U];
}

XamlFacets::TypeFacets* XamlFacets::FindTypeMutable(
    Core::TypeId type) noexcept {
    if (frozen_) {
        const std::uint32_t* position = index_.Find(type);
        return position != nullptr && *position < types_.Size()
            ? &types_[*position]
            : nullptr;
    }
    for (TypeFacets& entry : types_) {
        if (entry.type == type) return &entry;
    }
    return nullptr;
}

const XamlFacets::TypeFacets* XamlFacets::FindType(
    Core::TypeId type) const noexcept {
    if (frozen_) {
        const std::uint32_t* position = index_.Find(type);
        return position != nullptr && *position < types_.Size()
            ? &types_[*position]
            : nullptr;
    }
    for (const TypeFacets& entry : types_) {
        if (entry.type == type) return &entry;
    }
    return nullptr;
}

Base::Result<void> XamlFacets::TryAdd(
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

    const bool addLifecycle =
        facet.beginInit != nullptr || facet.endInit != nullptr ||
        facet.abortInit != nullptr ||
        facet.endInitWithServices != nullptr;
    const bool addNameScope =
        facet.createsNameScope || facet.registerName != nullptr;
    const bool addResourceScope =
        facet.createsResourceScope || facet.addResource != nullptr ||
        facet.resolveResourceScope != nullptr;
    const bool addDeferredContent = facet.defersVisualContent;
    const bool addImplicitResourceKey =
        facet.resolveImplicitResourceKey != nullptr;
    const bool addPropertyTarget = facet.resolvePropertyTarget != nullptr;

    if (!addLifecycle && !addNameScope && !addResourceScope &&
        !addDeferredContent && !addImplicitResourceKey &&
        !addPropertyTarget) {
        return InvalidFacet(
            "XAML aggregate facet contains no capabilities");
    }

    TypeFacets* existing = FindTypeMutable(facet.type);
    if ((addLifecycle && existing != nullptr && existing->hasLifecycle) ||
        (addNameScope && existing != nullptr && existing->hasNameScope) ||
        (addResourceScope && existing != nullptr &&
            existing->hasResourceScope) ||
        (addDeferredContent && existing != nullptr &&
            existing->hasDeferredContent) ||
        (addImplicitResourceKey && existing != nullptr &&
            existing->hasImplicitResourceKey) ||
        (addPropertyTarget && existing != nullptr &&
            existing->hasPropertyTarget)) {
        return DuplicateFacet(
            "XAML aggregate facet overlaps an existing capability");
    }

    Base::Result<TypeFacets*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    TypeFacets& entry = *ensured.Value();

    if (addLifecycle) {
        entry.lifecycle = {
            facet.type,
            facet.beginInit,
            facet.endInit,
            facet.abortInit,
            facet.endInitWithServices,
            facet.context};
        entry.hasLifecycle = true;
    }
    if (addNameScope) {
        entry.nameScope = {
            facet.type,
            facet.createsNameScope,
            facet.registerName,
            facet.context};
        entry.hasNameScope = true;
    }
    if (addResourceScope) {
        entry.resourceScope = {
            facet.type,
            facet.createsResourceScope,
            facet.addResource,
            facet.resolveResourceScope,
            facet.context};
        entry.hasResourceScope = true;
    }
    if (addDeferredContent) {
        entry.deferredContent = {facet.type, true};
        entry.hasDeferredContent = true;
    }
    if (addImplicitResourceKey) {
        entry.implicitResourceKey = {
            facet.type,
            facet.resolveImplicitResourceKey,
            facet.context};
        entry.hasImplicitResourceKey = true;
    }
    if (addPropertyTarget) {
        entry.propertyTarget = {
            facet.type,
            facet.resolvePropertyTarget,
            facet.context};
        entry.hasPropertyTarget = true;
    }
    return {};
}

Base::Result<void> XamlFacets::TryAdd(
    const XamlLifecycleFacet& facet,
    const Core::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.abiVersion != XamlFacetAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML capability facet ABI version is incompatible");
    }
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();
    if (facet.beginInit == nullptr && facet.endInit == nullptr &&
        facet.abortInit == nullptr &&
        facet.endInitWithServices == nullptr) {
        return InvalidFacet("XAML lifecycle facet is invalid");
    }
    Base::Result<TypeFacets*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    if (ensured.Value()->hasLifecycle) {
        return DuplicateFacet("XAML lifecycle facet is already registered");
    }
    ensured.Value()->lifecycle = facet;
    ensured.Value()->hasLifecycle = true;
    return {};
}

Base::Result<void> XamlFacets::TryAdd(
    const XamlNameScopeFacet& facet,
    const Core::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.abiVersion != XamlFacetAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML capability facet ABI version is incompatible");
    }
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();
    if (!facet.createsNameScope && facet.registerName == nullptr) {
        return InvalidFacet("XAML name-scope facet is invalid");
    }
    Base::Result<TypeFacets*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    if (ensured.Value()->hasNameScope) {
        return DuplicateFacet("XAML name-scope facet is already registered");
    }
    ensured.Value()->nameScope = facet;
    ensured.Value()->hasNameScope = true;
    return {};
}

Base::Result<void> XamlFacets::TryAdd(
    const XamlResourceScopeFacet& facet,
    const Core::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.abiVersion != XamlFacetAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML capability facet ABI version is incompatible");
    }
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();
    if (!facet.createsResourceScope && facet.addResource == nullptr &&
        facet.resolveResourceScope == nullptr) {
        return InvalidFacet("XAML resource-scope facet is invalid");
    }
    Base::Result<TypeFacets*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    if (ensured.Value()->hasResourceScope) {
        return DuplicateFacet(
            "XAML resource-scope facet is already registered");
    }
    ensured.Value()->resourceScope = facet;
    ensured.Value()->hasResourceScope = true;
    return {};
}

Base::Result<void> XamlFacets::TryAdd(
    const XamlDeferredContentFacet& facet,
    const Core::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.abiVersion != XamlFacetAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML capability facet ABI version is incompatible");
    }
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();
    if (!facet.defersVisualContent) {
        return InvalidFacet("XAML deferred-content facet is invalid");
    }
    Base::Result<TypeFacets*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    if (ensured.Value()->hasDeferredContent) {
        return DuplicateFacet(
            "XAML deferred-content facet is already registered");
    }
    ensured.Value()->deferredContent = facet;
    ensured.Value()->hasDeferredContent = true;
    return {};
}

Base::Result<void> XamlFacets::TryAdd(
    const XamlImplicitResourceKeyFacet& facet,
    const Core::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.abiVersion != XamlFacetAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML capability facet ABI version is incompatible");
    }
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();
    if (facet.resolve == nullptr) {
        return InvalidFacet("XAML implicit-resource-key facet is invalid");
    }
    Base::Result<TypeFacets*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    if (ensured.Value()->hasImplicitResourceKey) {
        return DuplicateFacet(
            "XAML implicit-resource-key facet is already registered");
    }
    ensured.Value()->implicitResourceKey = facet;
    ensured.Value()->hasImplicitResourceKey = true;
    return {};
}

Base::Result<void> XamlFacets::TryAdd(
    const XamlPropertyTargetFacet& facet,
    const Core::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.abiVersion != XamlFacetAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML capability facet ABI version is incompatible");
    }
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();
    if (facet.resolve == nullptr) {
        return InvalidFacet("XAML property-target facet is invalid");
    }
    Base::Result<TypeFacets*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    if (ensured.Value()->hasPropertyTarget) {
        return DuplicateFacet(
            "XAML property-target facet is already registered");
    }
    ensured.Value()->propertyTarget = facet;
    ensured.Value()->hasPropertyTarget = true;
    return {};
}

Base::Result<void> XamlFacets::TryAdd(
    const XamlMarkupExtensionFacet& facet,
    const Core::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    const Core::TypeInfo* type = descriptors.FindType(facet.type);
    if (facet.abiVersion != XamlFacetAbiVersion ||
        type == nullptr || facet.provideValue == nullptr ||
        !HasTypeFlag(type->Flags(), Core::TypeFlags::MarkupExtension)) {
        return InvalidFacet(
            "XAML markup-extension facet requires a flagged type and provider");
    }
    Base::Result<TypeFacets*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    if (ensured.Value()->hasMarkupExtension) {
        return DuplicateFacet(
            "XAML markup-extension facet is already registered");
    }
    ensured.Value()->markupExtension = facet;
    ensured.Value()->hasMarkupExtension = true;
    return {};
}

Base::Result<void> XamlFacets::Freeze() noexcept {
    if (frozen_) return {};

    index_.Clear();
    Base::Result<void> reserved = index_.TryReserve(types_.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t position = 0U;
         position < types_.Size();
         ++position) {
        Base::Result<FacetIndex::InsertResult> inserted =
            index_.TryInsert(types_[position].type, position);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) {
            index_.Clear();
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML facet index contains a duplicate type");
        }
    }
    frozen_ = true;
    return {};
}

Base::Result<void> XamlFacets::CollectLifecycle(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors,
    Base::Vector<const XamlLifecycleFacet*>& output) const noexcept {
    output.Clear();
    if (type == Core::InvalidTypeId) {
        return InvalidFacet(
            "Lifecycle facet collection requires a valid type");
    }

    Core::TypeId current = type;
    std::uint32_t depth = 0U;
    while (current != Core::InvalidTypeId &&
           depth <= descriptors.TypeCount()) {
        const XamlLifecycleFacet* facet = FindLifecycleExact(current);
        if (facet != nullptr) {
            Base::Result<void> added = output.TryPushBack(facet);
            if (!added) {
                output.Clear();
                return added.GetStatus();
            }
        }
        const Core::TypeInfo* descriptor = descriptors.FindType(current);
        if (descriptor == nullptr) {
            current = Core::InvalidTypeId;
            break;
        }
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

const XamlLifecycleFacet* XamlFacets::FindLifecycle(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlLifecycleFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindLifecycleExact(current);
        });
}

const XamlNameScopeFacet* XamlFacets::FindNameScope(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlNameScopeFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindNameScopeExact(current);
        });
}

const XamlResourceScopeFacet* XamlFacets::FindResourceScope(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlResourceScopeFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindResourceScopeExact(current);
        });
}

const XamlDeferredContentFacet* XamlFacets::FindDeferredContent(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlDeferredContentFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindDeferredContentExact(current);
        });
}

const XamlImplicitResourceKeyFacet*
XamlFacets::FindImplicitResourceKey(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlImplicitResourceKeyFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindImplicitResourceKeyExact(current);
        });
}

const XamlPropertyTargetFacet* XamlFacets::FindPropertyTarget(
    Core::TypeId type,
    const Core::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlPropertyTargetFacet>(
        type, descriptors,
        [this](Core::TypeId current) noexcept {
            return FindPropertyTargetExact(current);
        });
}

const XamlMarkupExtensionFacet* XamlFacets::FindMarkupExtension(
    Core::TypeId type) const noexcept {
    const TypeFacets* entry = FindType(type);
    return entry != nullptr && entry->hasMarkupExtension
        ? &entry->markupExtension
        : nullptr;
}

const XamlLifecycleFacet* XamlFacets::FindLifecycleExact(
    Core::TypeId type) const noexcept {
    const TypeFacets* entry = FindType(type);
    return entry != nullptr && entry->hasLifecycle
        ? &entry->lifecycle
        : nullptr;
}

const XamlNameScopeFacet* XamlFacets::FindNameScopeExact(
    Core::TypeId type) const noexcept {
    const TypeFacets* entry = FindType(type);
    return entry != nullptr && entry->hasNameScope
        ? &entry->nameScope
        : nullptr;
}

const XamlResourceScopeFacet* XamlFacets::FindResourceScopeExact(
    Core::TypeId type) const noexcept {
    const TypeFacets* entry = FindType(type);
    return entry != nullptr && entry->hasResourceScope
        ? &entry->resourceScope
        : nullptr;
}

const XamlDeferredContentFacet*
XamlFacets::FindDeferredContentExact(
    Core::TypeId type) const noexcept {
    const TypeFacets* entry = FindType(type);
    return entry != nullptr && entry->hasDeferredContent
        ? &entry->deferredContent
        : nullptr;
}

const XamlImplicitResourceKeyFacet*
XamlFacets::FindImplicitResourceKeyExact(
    Core::TypeId type) const noexcept {
    const TypeFacets* entry = FindType(type);
    return entry != nullptr && entry->hasImplicitResourceKey
        ? &entry->implicitResourceKey
        : nullptr;
}

const XamlPropertyTargetFacet*
XamlFacets::FindPropertyTargetExact(
    Core::TypeId type) const noexcept {
    const TypeFacets* entry = FindType(type);
    return entry != nullptr && entry->hasPropertyTarget
        ? &entry->propertyTarget
        : nullptr;
}

} // namespace Aero::Markup::Detail
