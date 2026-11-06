#include "gui/markup/XamlSchema.hpp"
#include "gui/markup/XamlBytecode.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EnumRegistration.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/templates/TemplateInstance.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Documents/Inlines.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>
#include <Aero/VisualStateManager.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

// ============================================================================
// SECTION: XamlSchemaMetadata
// Source: src/gui/markup/XamlSchemaMetadata.cpp
// ============================================================================




// XamlFacets runtime + markup extension token registration (PopulateMarkupMetadata).

namespace Aero::Markup {
namespace {

bool MetadataHasTypeFlag(
    Meta::TypeFlags value,
    Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status FrozenStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML facet store is frozen");
}

Base::Result<void> ValidateObjectType(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) noexcept {
    const Meta::TypeInfo* descriptor = descriptors.FindType(type);
    if (descriptor == nullptr ||
        MetadataHasTypeFlag(descriptor->Flags(), Meta::TypeFlags::ValueType)) {
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
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors,
    ExactLookup&& lookup) noexcept {
    Meta::TypeId current = type;
    std::uint32_t depth = 0U;
    while (current != Meta::InvalidTypeId &&
           depth <= descriptors.TypeCount()) {
        const T* facet = lookup(current);
        if (facet != nullptr) return facet;
        const Meta::TypeInfo* descriptor = descriptors.FindType(current);
        if (descriptor == nullptr) return nullptr;
        current = descriptor->BaseType();
        ++depth;
    }
    return nullptr;
}

template<class T, class ExactLookup>
const T* FindByPolicy(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors,
    ExactLookup&& lookup) noexcept {
    if (const T* exact = lookup(type)) {
        return exact;
    }
    if constexpr (
        T::InheritancePolicy ==
        XamlFacetInheritancePolicy::ExactOnly) {
        return nullptr;
    } else {
        const Meta::TypeInfo* descriptor = descriptors.FindType(type);
        return descriptor != nullptr
            ? FindInherited<T>(
                  descriptor->BaseType(),
                  descriptors,
                  std::forward<ExactLookup>(lookup))
            : nullptr;
    }
}

template<class Facet>
const Facet* FacetColumnAt(
    const Base::Vector<Facet>& column,
    std::uint32_t index) noexcept {
    return index < column.Size() ? &column[index] : nullptr;
}

} // namespace

std::uint16_t XamlFacets::FacetCountBefore(
    FacetMask mask,
    FacetKind kind) noexcept {
    return ::Aero::CompactFacetIndex::CountBefore(mask, kind);
}

XamlFacets::DraftType* XamlFacets::FindDraft(
    Meta::TypeId type) noexcept {
    for (DraftType& draft : drafts_) {
        if (draft.type == type) return &draft;
    }
    return nullptr;
}

const XamlFacets::DraftType* XamlFacets::FindDraft(
    Meta::TypeId type) const noexcept {
    for (const DraftType& draft : drafts_) {
        if (draft.type == type) return &draft;
    }
    return nullptr;
}

Base::Result<XamlFacets::DraftType*>
XamlFacets::EnsureType(Meta::TypeId type) noexcept {
    DraftType* existing = FindDraft(type);
    if (existing != nullptr) return existing;
    DraftType* added = drafts_.EmplaceBack();
    added->type = type;
    return added;
}

const XamlFacets::XamlTypePlan* XamlFacets::FindPlan(
    Meta::TypeId type) const noexcept {
    if (!frozen_) return nullptr;
    const std::uint32_t* position = index_.Find(type);
    return position != nullptr && *position < plans_.Size()
        ? &plans_[*position]
        : nullptr;
}

std::uint32_t XamlFacets::FindFacetIndex(
    Meta::TypeId type,
    FacetKind kind) const noexcept {
    const XamlTypePlan* plan = FindPlan(type);
    const FacetMask bit = FacetBit(kind);
    if (plan == nullptr || (plan->facetMask & bit) == 0U) {
        return InvalidFacetIndex;
    }
    const std::uint32_t reference = plan->firstFacetRef +
        FacetCountBefore(plan->facetMask, kind);
    return reference < facetRefs_.Size()
        ? facetRefs_[reference]
        : InvalidFacetIndex;
}

Base::Result<void> XamlFacets::Add(
    const XamlTypeFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
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

    const DraftType* existing = FindDraft(facet.type);
    const auto occupied = [existing](FacetKind kind) noexcept {
        return existing != nullptr &&
            existing->facets[static_cast<std::uint8_t>(kind)] !=
                InvalidFacetIndex;
    };
    if ((addLifecycle && occupied(FacetKind::Lifecycle)) ||
        (addNameScope && occupied(FacetKind::NameScope)) ||
        (addResourceScope && occupied(FacetKind::ResourceScope)) ||
        (addDeferredContent && occupied(FacetKind::DeferredContent)) ||
        (addImplicitResourceKey &&
            occupied(FacetKind::ImplicitResourceKey)) ||
        (addPropertyTarget && occupied(FacetKind::PropertyTarget))) {
        return DuplicateFacet(
            "XAML aggregate facet overlaps an existing capability");
    }

    drafts_.Reserve(
        drafts_.Size() + (existing == nullptr ? 1U : 0U));
    if (addLifecycle) {
        lifecycles_.Reserve(lifecycles_.Size() + 1U);
    }
    if (addNameScope) {
        nameScopes_.Reserve(nameScopes_.Size() + 1U);
    }
    if (addResourceScope) {
        resourceScopes_.Reserve(resourceScopes_.Size() + 1U);
    }
    if (addDeferredContent) {
        deferredContents_.Reserve(deferredContents_.Size() + 1U);
    }
    if (addImplicitResourceKey) {
        implicitResourceKeys_.Reserve(
            implicitResourceKeys_.Size() + 1U);
    }
    if (addPropertyTarget) {
        propertyTargets_.Reserve(propertyTargets_.Size() + 1U);
    }

    Base::Result<DraftType*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    DraftType& draft = *ensured.Value();
    if (addLifecycle) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::Lifecycle)] =
            lifecycles_.Size();
        lifecycles_.PushBack({
            facet.type, facet.beginInit, facet.endInit, facet.abortInit,
            facet.endInitWithServices, facet.context});
    }
    if (addNameScope) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::NameScope)] =
            nameScopes_.Size();
        nameScopes_.PushBack({
            facet.type, facet.createsNameScope,
            facet.registerName, facet.context});
    }
    if (addResourceScope) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::ResourceScope)] =
            resourceScopes_.Size();
        resourceScopes_.PushBack({
            facet.type, facet.createsResourceScope, facet.addResource,
            facet.resolveResourceScope, facet.context});
    }
    if (addDeferredContent) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::DeferredContent)] =
            deferredContents_.Size();
        deferredContents_.PushBack({facet.type, true});
    }
    if (addImplicitResourceKey) {
        draft.facets[
            static_cast<std::uint8_t>(FacetKind::ImplicitResourceKey)] =
                implicitResourceKeys_.Size();
        implicitResourceKeys_.PushBack({
            facet.type, facet.resolveImplicitResourceKey, facet.context});
    }
    if (addPropertyTarget) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::PropertyTarget)] =
            propertyTargets_.Size();
        propertyTargets_.PushBack({
            facet.type, facet.resolvePropertyTarget, facet.context});
    }
    return {};
}

template<class Facet, class ColumnType>
Base::Result<void> XamlFacets::AddFacetCore(
    ColumnType& column,
    FacetKind kind,
    const Facet& facet,
    const Meta::TypeRegistry& descriptors,
    bool isInvalid,
    const char* invalidMessage,
    const char* duplicateMessage) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.abiVersion != XamlFacetAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML capability facet ABI version is incompatible");
    }
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();
    if (isInvalid) return InvalidFacet(invalidMessage);
    DraftType* existing = FindDraft(facet.type);
    if (existing != nullptr &&
        existing->facets[static_cast<std::uint8_t>(kind)] != InvalidFacetIndex) {
        return DuplicateFacet(duplicateMessage);
    }
    drafts_.Reserve(drafts_.Size() + (existing == nullptr ? 1U : 0U));
    column.Reserve(column.Size() + 1U);
    Base::Result<DraftType*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    ensured->facets[static_cast<std::uint8_t>(kind)] = column.Size();
    column.PushBack(facet);
    return {};
}

Base::Result<void> XamlFacets::Add(
    const XamlLifecycleFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    return AddFacetCore(
        lifecycles_, FacetKind::Lifecycle, facet, descriptors,
        facet.beginInit == nullptr && facet.endInit == nullptr &&
            facet.abortInit == nullptr &&
            facet.endInitWithServices == nullptr,
        "XAML lifecycle facet is invalid",
        "XAML lifecycle facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlNameScopeFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    return AddFacetCore(
        nameScopes_, FacetKind::NameScope, facet, descriptors,
        !facet.createsNameScope && facet.registerName == nullptr,
        "XAML name-scope facet is invalid",
        "XAML name-scope facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlResourceScopeFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    return AddFacetCore(
        resourceScopes_, FacetKind::ResourceScope, facet, descriptors,
        !facet.createsResourceScope && facet.addResource == nullptr &&
            facet.resolveResourceScope == nullptr,
        "XAML resource-scope facet is invalid",
        "XAML resource-scope facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlDeferredContentFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    return AddFacetCore(
        deferredContents_, FacetKind::DeferredContent, facet, descriptors,
        !facet.defersVisualContent,
        "XAML deferred-content facet is invalid",
        "XAML deferred-content facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlImplicitResourceKeyFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    return AddFacetCore(
        implicitResourceKeys_, FacetKind::ImplicitResourceKey, facet, descriptors,
        facet.resolve == nullptr,
        "XAML implicit-resource-key facet is invalid",
        "XAML implicit-resource-key facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlPropertyTargetFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    return AddFacetCore(
        propertyTargets_, FacetKind::PropertyTarget, facet, descriptors,
        facet.resolve == nullptr,
        "XAML property-target facet is invalid",
        "XAML property-target facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlMarkupExtensionFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    const Meta::TypeInfo* type = descriptors.FindType(facet.type);
    if (facet.abiVersion != XamlFacetAbiVersion ||
        type == nullptr || facet.provideValue == nullptr ||
        !MetadataHasTypeFlag(type->Flags(), Meta::TypeFlags::MarkupExtension)) {
        return InvalidFacet(
            "XAML markup-extension facet requires a flagged type and provider");
    }
    DraftType* existing = FindDraft(facet.type);
    if (existing != nullptr &&
        existing->facets[static_cast<std::uint8_t>(
            FacetKind::MarkupExtension)] != InvalidFacetIndex) {
        return DuplicateFacet(
            "XAML markup-extension facet is already registered");
    }
    drafts_.Reserve(
        drafts_.Size() + (existing == nullptr ? 1U : 0U));
    markupExtensions_.Reserve(markupExtensions_.Size() + 1U);
    Base::Result<DraftType*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    ensured->facets[static_cast<std::uint8_t>(
        FacetKind::MarkupExtension)] = markupExtensions_.Size();
    markupExtensions_.PushBack(facet);
    return {};
}

Base::Result<void> XamlFacets::BuildLifecyclePlans(
    const Meta::TypeRegistry& descriptors) noexcept {
    Base::Vector<Meta::TypeId> ancestry;
    ancestry.Reserve(descriptors.TypeCount());

    for (XamlTypePlan& plan : plans_) {
        plan.firstLifecycleRef = lifecycleRefs_.Size();
        ancestry.Clear();
        Meta::TypeId current = plan.type;
        std::uint32_t depth = 0U;
        while (current != Meta::InvalidTypeId &&
               depth <= descriptors.TypeCount()) {
            ancestry.PushBack(current);
            const Meta::TypeInfo* descriptor = descriptors.FindType(current);
            if (descriptor == nullptr) break;
            current = descriptor->BaseType();
            ++depth;
        }
        if (current != Meta::InvalidTypeId &&
            depth > descriptors.TypeCount()) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Lifecycle facet inheritance chain contains a cycle");
        }

        std::uint32_t count = 0U;
        for (std::uint32_t index = ancestry.Size(); index > 0U; --index) {
            const DraftType* draft = FindDraft(ancestry[index - 1U]);
            if (draft == nullptr) continue;
            const std::uint32_t facet = draft->facets[
                static_cast<std::uint8_t>(FacetKind::Lifecycle)];
            if (facet == InvalidFacetIndex) continue;
            if (count == UINT16_MAX) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "XAML lifecycle plan exceeds the compact record limit");
            }
            lifecycleRefs_.PushBack(facet);
            ++count;
        }
        plan.lifecycleCount = static_cast<std::uint16_t>(count);
    }
    return {};
}

Base::Result<void> XamlFacets::Freeze(
    const Meta::TypeRegistry& descriptors) noexcept {
    if (frozen_) return {};

    plans_.Clear();
    facetRefs_.Clear();
    lifecycleRefs_.Clear();
    index_.Clear();

    plans_.Reserve(descriptors.TypeCount());
    index_.Reserve(descriptors.TypeCount());
    facetRefs_.Reserve(
        lifecycles_.Size() + nameScopes_.Size() + resourceScopes_.Size() +
        deferredContents_.Size() + implicitResourceKeys_.Size() +
        propertyTargets_.Size() + markupExtensions_.Size());

    for (const Meta::TypeInfo& descriptor : descriptors.Types()) {
        XamlTypePlan plan;
        plan.type = descriptor.Id();
        plan.firstFacetRef = facetRefs_.Size();
        const DraftType* draft = FindDraft(plan.type);
        for (std::uint8_t kind = 0U;
             kind < static_cast<std::uint8_t>(FacetKind::Count);
             ++kind) {
            std::uint32_t facet = draft != nullptr
                ? draft->facets[kind] : InvalidFacetIndex;
            if (facet == InvalidFacetIndex &&
                kind != static_cast<std::uint8_t>(FacetKind::Lifecycle) &&
                kind != static_cast<std::uint8_t>(FacetKind::MarkupExtension)) {
                Meta::TypeId current = descriptor.BaseType();
                std::uint32_t depth = 0U;
                while (current != Meta::InvalidTypeId && depth <= descriptors.TypeCount()) {
                    const DraftType* ancestorDraft = FindDraft(current);
                    if (ancestorDraft != nullptr &&
                        ancestorDraft->facets[kind] != InvalidFacetIndex) {
                        facet = ancestorDraft->facets[kind];
                        break;
                    }
                    const Meta::TypeInfo* baseInfo = descriptors.FindType(current);
                    if (baseInfo == nullptr) break;
                    current = baseInfo->BaseType();
                    ++depth;
                }
            }
            if (facet == InvalidFacetIndex) continue;
            facetRefs_.PushBack(facet);
            plan.facetMask |= static_cast<FacetMask>(1U << kind);
            ++plan.facetCount;
        }
        const std::uint32_t position = plans_.Size();
        plans_.PushBack(plan);
        FacetIndex::InsertResult inserted =
            index_.Insert(plan.type, position);
        if (!inserted.inserted) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML facet index contains a duplicate type");
        }
    }

    Base::Result<void> lifecycle = BuildLifecyclePlans(descriptors);
    if (!lifecycle) return lifecycle.GetStatus();
    drafts_.Clear();
    frozen_ = true;
    return {};
}

Base::Span<const std::uint32_t> XamlFacets::LifecyclePlan(
    Meta::TypeId type) const noexcept {
    const XamlTypePlan* plan = FindPlan(type);
    if (plan == nullptr || plan->lifecycleCount == 0U ||
        plan->firstLifecycleRef + plan->lifecycleCount >
            lifecycleRefs_.Size()) {
        return {};
    }
    return {
        lifecycleRefs_.Data() + plan->firstLifecycleRef,
        plan->lifecycleCount};
}

const XamlLifecycleFacet* XamlFacets::LifecycleAt(
    std::uint32_t index) const noexcept {
    return index < lifecycles_.Size() ? &lifecycles_[index] : nullptr;
}

const XamlLifecycleFacet* XamlFacets::FindLifecycle(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlLifecycleFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept { return FindLifecycleExact(current); });
}

const XamlNameScopeFacet* XamlFacets::FindNameScope(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlNameScopeFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept { return FindNameScopeExact(current); });
}

const XamlResourceScopeFacet* XamlFacets::FindResourceScope(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlResourceScopeFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept { return FindResourceScopeExact(current); });
}

const XamlDeferredContentFacet* XamlFacets::FindDeferredContent(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlDeferredContentFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept { return FindDeferredContentExact(current); });
}

const XamlImplicitResourceKeyFacet*
XamlFacets::FindImplicitResourceKey(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlImplicitResourceKeyFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept { return FindImplicitResourceKeyExact(current); });
}

const XamlPropertyTargetFacet* XamlFacets::FindPropertyTarget(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlPropertyTargetFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept { return FindPropertyTargetExact(current); });
}

const XamlMarkupExtensionFacet* XamlFacets::FindMarkupExtension(
    Meta::TypeId type) const noexcept {
    return FacetColumnAt(markupExtensions_, FindFacetIndex(type, FacetKind::MarkupExtension));
}

const XamlLifecycleFacet* XamlFacets::FindLifecycleExact(
    Meta::TypeId type) const noexcept {
    return FacetColumnAt(lifecycles_, FindFacetIndex(type, FacetKind::Lifecycle));
}

const XamlNameScopeFacet* XamlFacets::FindNameScopeExact(
    Meta::TypeId type) const noexcept {
    return FacetColumnAt(nameScopes_, FindFacetIndex(type, FacetKind::NameScope));
}

const XamlResourceScopeFacet* XamlFacets::FindResourceScopeExact(
    Meta::TypeId type) const noexcept {
    return FacetColumnAt(resourceScopes_, FindFacetIndex(type, FacetKind::ResourceScope));
}

const XamlDeferredContentFacet*
XamlFacets::FindDeferredContentExact(
    Meta::TypeId type) const noexcept {
    return FacetColumnAt(deferredContents_, FindFacetIndex(type, FacetKind::DeferredContent));
}

const XamlImplicitResourceKeyFacet*
XamlFacets::FindImplicitResourceKeyExact(
    Meta::TypeId type) const noexcept {
    return FacetColumnAt(implicitResourceKeys_, FindFacetIndex(type, FacetKind::ImplicitResourceKey));
}

const XamlPropertyTargetFacet*
XamlFacets::FindPropertyTargetExact(
    Meta::TypeId type) const noexcept {
    return FacetColumnAt(propertyTargets_, FindFacetIndex(type, FacetKind::PropertyTarget));
}

} // namespace Aero::Markup

// Markup extension token types + PopulateMarkupMetadata (who-defines-registers).
namespace Aero::Markup {
namespace {

using namespace Aero::Meta;
using namespace Aero::Threading;

class DynamicResourceExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        DynamicResourceExtensionToken,
        Base::Object,
        "urn:aero",
        "DynamicResource")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class StaticExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        StaticExtensionToken,
        Base::Object,
        "http://schemas.microsoft.com/winfx/2006/xaml",
        "Static")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class TypeExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TypeExtensionToken,
        Base::Object,
        "http://schemas.microsoft.com/winfx/2006/xaml",
        "Type")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class TemplateBindingExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TemplateBindingExtensionToken,
        Base::Object,
        "urn:aero",
        "TemplateBinding")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class StaticResourceExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        StaticResourceExtensionToken,
        Base::Object,
        Meta::AeroNamespaceUri(),
        "StaticResourceExtension")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

// AeroGUI's application samples expose Loc both as a markup extension and as
// an attached Source property.  The token deliberately lives in the normal
// schema so the legacy AeroGUIExtensions namespace resolves to the same type
// as other compatibility extensions.
class LocExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        LocExtensionToken,
        Base::Object,
        "urn:aero",
        "Loc")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr Meta::AttachedPropertyRef<
        LocExtensionToken, Base::ResourceUri>
        SourceProperty{"Source"};
};

} // namespace

Base::Result<void> PopulateMarkupMetadata(
    Meta::Registration& context) noexcept {
    Base::Result<void> status =
        Meta::Register<MarkupExtension>(
            context,
            TypeFlags::MarkupExtension |
                TypeFlags::Abstract).Result();
    if (!status) return status.GetStatus();
    status =
        Meta::Register<DynamicResourceExtensionToken>(
            context,
            TypeFlags::MarkupExtension |
                TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<TypeExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<TemplateBindingExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
                TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticResourceExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    auto loc = Meta::Register<LocExtensionToken>(
        context,
        TypeFlags::MarkupExtension | TypeFlags::Abstract);
    loc.Property(
        LocExtensionToken::SourceProperty,
        FrameworkPropertyMetadata(Base::ResourceUri{}, Inherits).Changed(
            &LocExtension::OnSourceChanged));
    status = loc.Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticResourceObject>(context)
        .Property(
            StaticResourceObject::ResourceKeyProperty,
            Base::String{})
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    status = ::Aero::PopulateVisualStateMetadata(context);
    if (!status) return status.GetStatus();
    return {};
}

} // namespace Aero::Markup

// ============================================================================
// SECTION: XamlSchemaContext
// Source: src/gui/markup/XamlSchemaContext.cpp
// ============================================================================
// Consolidated implementation. Keep sections ordered by dependency.



// ===== Schema =====




// Query surface is public; registration/execution ops are private (friends).


namespace Aero::Markup {

namespace {

Base::Status RuntimeSchemaNotReady() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML runtime schema requires a frozen descriptor runtime");
}

Base::Status RuntimeTypeNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML runtime type was not found");
}

Base::Status RuntimeMemberNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML runtime member was not found");
}



} // namespace

Schema::Schema(
    ::Aero::Meta::Registry& metadata,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      domain_(&metadata) {
    AERO_ASSERT(metadata.IsSealed());
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::Markup, facets_));
}

Schema::~Schema() noexcept {
    if (facets_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::Markup, facets_);
    facets_ = nullptr;
}


Base::Result<void> Schema::AddType(
    const XamlTypeFacet& registration) noexcept {
    return facets_->Add(registration, Types());
}

Base::Result<void> Schema::AddLifecycle(
    const XamlLifecycleFacet& registration) noexcept {
    return facets_->Add(registration, Types());
}

Base::Result<void> Schema::AddNameScope(
    const XamlNameScopeFacet& registration) noexcept {
    return facets_->Add(registration, Types());
}

Base::Result<void> Schema::AddResourceScope(
    const XamlResourceScopeFacet& registration) noexcept {
    return facets_->Add(registration, Types());
}

Base::Result<void> Schema::AddDeferredContent(
    const XamlDeferredContentFacet& registration) noexcept {
    return facets_->Add(registration, Types());
}

Base::Result<void> Schema::AddImplicitResourceKey(
    const XamlImplicitResourceKeyFacet& registration) noexcept {
    return facets_->Add(registration, Types());
}

Base::Result<void> Schema::AddPropertyTarget(
    const XamlPropertyTargetFacet& registration) noexcept {
    return facets_->Add(registration, Types());
}

Base::Result<void> Schema::AddMarkupExtension(
    const XamlMarkupExtensionFacet& registration) noexcept {
    return facets_->Add(registration, Types());
}

Base::Result<const Meta::TypeInfo*> Schema::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }

    const Meta::TypeInfo* descriptor =
        domain_->Types().FindType(
            IsSystemNamespace(xamlNamespace)
                ? Meta::AeroNamespaceUri()
                : CanonicalXamlNamespace(xamlNamespace),
            CanonicalXamlTypeName(localName));
    if (descriptor == nullptr) return RuntimeTypeNotFound();
    return descriptor;
}

Base::Result<ResolvedMember> Schema::ResolveMember(
    Meta::TypeId targetType,
    const QualifiedName& name,
    MemberSyntax syntax) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }

    const Meta::TypeInfo* target =
        domain_->Types().FindType(targetType);
    if (target == nullptr || name.LocalName().Empty()) {
        return RuntimeMemberNotFound();
    }

    Base::Result<ParsedXamlMemberName> parsed =
        ParseXamlMemberName(name.LocalName(), name.NamespaceUri(), target->XamlNamespace());
    if (!parsed) return parsed.GetStatus();

    const ParsedXamlMemberName& member = parsed.Value();
    if (!member.hasOwner) {
        if (!name.NamespaceUri().Empty() &&
            CanonicalXamlNamespace(name.NamespaceUri()) !=
                target->XamlNamespace()) {
            return RuntimeMemberNotFound();
        }
        Base::Result<ResolvedMember> resolved =
            ResolvePropertyOrEvent(
            targetType, targetType, member.memberName, syntax, false);
        if (resolved || member.memberName != Base::StringView("ToolTip")) {
            return resolved;
        }

        // WPF exposes ToolTip as a FrameworkElement property even though the
        // storage and display policy live in ToolTipService. Retain that XAML
        // surface while keeping the existing shared service implementation.
        const Meta::TypeInfo* service =
            domain_->Types().FindType(
                Meta::AeroNamespaceUri(), "ToolTipService");
        if (service == nullptr) return resolved.GetStatus();
        return ResolvePropertyOrEvent(
            targetType, service->Id(), member.memberName, syntax, false);
    }

    if (IsAeroExtensionsFacade(
            member.ownerNamespace, member.ownerName)) {
        const Meta::TypeInfo* aeroOwner = domain_->Types().FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(member.ownerName));
        if (aeroOwner != nullptr) {
            return ResolvePropertyOrEvent(
                targetType, aeroOwner->Id(), member.memberName, syntax, true);
        }
        return ResolvePropertyOrEvent(
            targetType,
            targetType,
            member.memberName,
            syntax,
            false);
    }
    const Meta::TypeInfo* owner =
        domain_->Types().FindType(
            CanonicalXamlNamespace(member.ownerNamespace),
            CanonicalXamlTypeName(member.ownerName));
    if (owner == nullptr && name.NamespaceUri().Empty()) {
        owner = domain_->Types().FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(member.ownerName));
    }
    if (owner == nullptr) return RuntimeMemberNotFound();
    if (member.memberName == Base::StringView("ContextMenu")) {
        const Meta::TypeInfo* service = domain_->Types().FindType(
            Meta::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return ResolvePropertyOrEvent(
                targetType, service->Id(), member.memberName, syntax, true);
        }
    }
    return ResolvePropertyOrEvent(
        targetType, owner->Id(), member.memberName, syntax, true);
}

Base::Result<ResolvedMember> Schema::ResolveMember(
    Meta::TypeId targetType,
    Meta::MemberId memberId) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }
    if (targetType == Meta::InvalidTypeId ||
        memberId == Meta::InvalidMemberId) {
        return RuntimeMemberNotFound();
    }

    const Meta::TypeRegistry& descriptors = domain_->Types();
    if (descriptors.FindType(targetType) == nullptr) {
        return RuntimeMemberNotFound();
    }
    if (const Meta::PropertyInfo* property =
            descriptors.FindProperty(memberId)) {
        const bool attached = HasPropertyFlag(
            property->Flags(), Meta::PropertyFlags::Attached);
        if (!attached && targetType != property->OwnerType() &&
            !descriptors.IsDerivedFrom(
                targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AXB2 member owner is incompatible with the target type");
        }
        return MakeResolvedPropertyMember(
            property->Id(),
            property->OwnerType(),
            property->ValueType(),
            property->Flags());
    }
    if (const Meta::EventInfo* event =
            descriptors.FindEvent(memberId)) {
        const bool attached = HasEventFlag(
            event->Flags(), Meta::EventFlags::Attached);
        if (!attached && targetType != event->OwnerType() &&
            !descriptors.IsDerivedFrom(
                targetType, event->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AXB2 event owner is incompatible with the target type");
        }
        return MakeResolvedEventMember(
            event->Id(),
            event->OwnerType(),
            event->EventArgsType(),
            event->Flags());
    }
    return RuntimeMemberNotFound();
}

Base::Result<ResolvedMember>
Schema::ResolvePropertyOrEvent(
    Meta::TypeId targetType,
    Meta::TypeId ownerType,
    Base::StringView memberName,
    MemberSyntax syntax,
    bool ownerWasExplicit) const noexcept {
    const Meta::TypeRegistry& descriptors = domain_->Types();
    const Meta::PropertyInfo* property =
        descriptors.FindProperty(ownerType, memberName, true);
    if (property != nullptr &&
        syntax == MemberSyntax::Attribute &&
        HasPropertyFlag(
            property->Flags(),
            Meta::PropertyFlags::Collection)) {
        char aliasBuffer[128];
        if (memberName.SizeBytes() + 4U < sizeof(aliasBuffer)) {
            std::memcpy(aliasBuffer, memberName.Data(), memberName.SizeBytes());
            std::memcpy(aliasBuffer + memberName.SizeBytes(), "Text", 4U);
            const Meta::PropertyInfo* alias =
                descriptors.FindProperty(
                    ownerType,
                    Base::StringView(aliasBuffer, memberName.SizeBytes() + 4U),
                    true);
            if (alias != nullptr &&
                !HasPropertyFlag(
                    alias->Flags(),
                    Meta::PropertyFlags::Collection)) {
                property = alias;
            }
        }
    }
    if (property != nullptr) {
        const bool attached = HasPropertyFlag(
            property->Flags(), Meta::PropertyFlags::Attached);
        if (ownerWasExplicit && syntax == MemberSyntax::Attribute &&
            !attached && targetType != property->OwnerType() &&
            !descriptors.IsDerivedFrom(targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Explicit XAML attribute owner requires an attached property");
        }
        if (ownerWasExplicit && syntax == MemberSyntax::PropertyElement &&
            !attached && targetType != property->OwnerType() &&
            !descriptors.IsDerivedFrom(targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML property element owner is incompatible with the target type");
        }

        return MakeResolvedPropertyMember(
            property->Id(),
            property->OwnerType(),
            property->ValueType(),
            property->Flags());
    }

    const Meta::EventInfo* event =
        descriptors.FindEvent(ownerType, memberName, true);
    if (event != nullptr) {
        const bool attached = HasEventFlag(
            event->Flags(), Meta::EventFlags::Attached);
        const bool routed = HasEventFlag(
            event->Flags(), Meta::EventFlags::Routed);
        if (ownerWasExplicit && syntax == MemberSyntax::Attribute &&
            !attached && !routed) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Explicit XAML attribute owner requires an attached event");
        }
        if (ownerWasExplicit && syntax == MemberSyntax::PropertyElement &&
            !attached && targetType != event->OwnerType() &&
            !descriptors.IsDerivedFrom(targetType, event->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML event element owner is incompatible with the target type");
        }

        ResolvedMember resolved = MakeResolvedEventMember(
            event->Id(),
            event->OwnerType(),
            event->EventArgsType(),
            event->Flags());
        if (ownerWasExplicit && syntax == MemberSyntax::Attribute && routed) {
            resolved.attached = true;
        }
        return resolved;
    }

    return RuntimeMemberNotFound();
}

Base::Result<ResolvedMember> Schema::ResolveContentMember(
    Meta::TypeId targetType) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }

    const Meta::MemberId contentMember =
        domain_->FindContentMember(targetType);
    if (contentMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML runtime type has no content facet");
    }
    const Meta::PropertyInfo* property =
        domain_->Types().FindProperty(contentMember);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Content facet references a missing property descriptor");
    }

    return MakeResolvedPropertyMember(
        property->Id(),
        property->OwnerType(),
        property->ValueType(),
        property->Flags());
}

Base::Result<Base::Ref<Base::Object>> Schema::CreateObject(
    Meta::TypeId type) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }
    return domain_->CreateObject(type);
}

Base::Result<::Aero::DependencyObject*>
Schema::ResolvePropertyTarget(
    Base::Object& object) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }
    ::Aero::DependencyObject* target = nullptr;
    if (domain_->Types().IsAssignableFrom(
            Meta::TypeOf<::Aero::DependencyObject>(),
            object.RuntimeType())) {
        target = static_cast<::Aero::DependencyObject*>(&object);
    } else {
        const XamlPropertyTargetFacet* facet =
            facets_->FindPropertyTarget(
                object.RuntimeType(), domain_->Types());
        if (facet != nullptr && facet->resolve != nullptr) {
            target = facet->resolve(object, facet->context);
        }
    }
    if (target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML target does not support dependency properties");
    }
    if (&DependencyObjectAccess::PropertyRegistry(target) !=
        &static_cast<const ::Aero::Meta::Registry&>(
            *domain_).DependencyProperties()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML target belongs to a different metadata domain");
    }
    return target;
}

Base::Result<void> Schema::SetMember(
    Base::Object& object,
    Meta::TypeId objectType,
    const ResolvedMember& member,
    const Meta::Value& value) const noexcept {
    if (!frozen_ || domain_ == nullptr ||
        !domain_->IsReady() || !member.IsValid()) {
        return RuntimeSchemaNotReady();
    }
    if (member.kind != Meta::MemberKind::Property) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML runtime event assignment requires an event adapter");
    }
    if (!member.attached &&
        objectType != member.ownerType &&
        !domain_->Types().IsDerivedFrom(objectType, member.ownerType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML runtime member owner is incompatible with the target object");
    }

    const bool runtimeWritable =
        domain_->CanWriteProperty(member.id);
    Base::Result<Meta::ContentInfo> content =
        !runtimeWritable
        ? domain_->GetContentInfo(member.id)
        : Base::Result<Meta::ContentInfo>(
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Runtime content metadata was not requested"));
    const bool runtimeContentWritable = content &&
        content.Value().writable;
    if (!runtimeWritable && !runtimeContentWritable) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML runtime member has no writable facet or adapter");
    }

    Meta::Value convertedValue = value;
    const bool metadataAcceptsAnyValue =
        (static_cast<std::uint32_t>(
             member.propertyFlags) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::AnyValue)) != 0U;
    // Meta::Value is the metadata representation of WPF's object-valued
    // member and must retain the concrete type supplied by markup (enums,
    // scalars, objects, or null), even when an older descriptor omitted the
    // redundant AnyValue flag.
    const bool acceptsAnyValue = metadataAcceptsAnyValue ||
        member.valueType == Meta::TypeOf<Meta::Value>();
    if (!acceptsAnyValue) {
        if (member.id == VisualStateManager::VisualStateGroupsProperty.Handle().value &&
            convertedValue.Type() == VisualStateGroup::StaticTypeId() &&
            convertedValue.AsObject()) {
            auto& target = static_cast<::Aero::DependencyObject&>(object);
            Base::Ref<VisualStateGroupCollection> valueStore = target.GetValue(VisualStateManager::VisualStateGroupsProperty);
            if (!valueStore) {
                Base::Result<Base::Ref<VisualStateGroupCollection>> created =
                    Base::MakeRef<VisualStateGroupCollection>();
                if (!created) return created.GetStatus();
                valueStore = std::move(created).Value();
                target.SetValue(
                    VisualStateManager::VisualStateGroupsProperty,
                    valueStore);
            }
            (void)valueStore->Add(
                Base::Ref<VisualStateGroup>::FromBorrowed(
                    *static_cast<VisualStateGroup*>(convertedValue.AsObject().Get())));
            return {};
        }

        bool compatible = convertedValue.Type() == member.valueType;
        if (convertedValue.Kind() == Meta::ValueKind::Object &&
            convertedValue.AsObject()) {
            compatible = domain_->Types().IsDerivedFrom(
                convertedValue.Type(), member.valueType);
        }
        if (!compatible) {
            const Meta::TypeInfo* owner =
                domain_->Types().FindType(member.ownerType);
            const Meta::TypeInfo* expected =
                domain_->Types().FindType(member.valueType);
            const Meta::TypeInfo* actual =
                domain_->Types().FindType(convertedValue.Type());
            thread_local char message[384]{};
            std::snprintf(
                message, sizeof(message),
                "XAML member on '%.*s' expects '%.*s' but received '%.*s'",
                owner != nullptr
                    ? static_cast<int>(owner->Name().SizeBytes()) : 9,
                owner != nullptr ? owner->Name().Data() : "<unknown>",
                expected != nullptr
                    ? static_cast<int>(expected->Name().SizeBytes()) : 9,
                expected != nullptr ? expected->Name().Data() : "<unknown>",
                actual != nullptr
                    ? static_cast<int>(actual->Name().SizeBytes()) : 9,
                actual != nullptr ? actual->Name().Data() : "<unknown>");
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                message);
        }
    }

    if (runtimeWritable) {
        return domain_->SetProperty(
            object, member.id, convertedValue);
    }
    if (runtimeContentWritable) {
        if (convertedValue.Kind() != Meta::ValueKind::Object ||
            convertedValue.IsNullObject() || !convertedValue.AsObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML content member requires a non-null object value");
        }
        return domain_->WriteContent(
            object, member.id, convertedValue.AsObject());
    }
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "XAML runtime member is not writable");
}

MemberWritePolicy Schema::ResolveMemberWritePolicy(
    const ResolvedMember& member) const noexcept {
    if (domain_ == nullptr || !domain_->IsReady()) return {};
    // WPF attached VisualStateManager.VisualStateGroups is a collection of
    // VisualStateGroup children. The property type is VisualStateGroupCollection,
    // but markup writes one VisualStateGroup at a time.
    if (member.id ==
        VisualStateManager::VisualStateGroupsProperty.Handle().value) {
        return {MemberWriteMode::Collection, false, true};
    }
    if (domain_->CanWriteProperty(member.id)) {
        Base::Result<Meta::ContentInfo> content =
            domain_->GetContentInfo(member.id);
        const bool acceptsAnyValue =
            (static_cast<std::uint32_t>(
                 member.propertyFlags) &
             static_cast<std::uint32_t>(
                 Meta::PropertyFlags::AnyValue)) != 0U ||
            member.valueType == Meta::TypeOf<Meta::Value>();
        return {
            content && content.Value().writable &&
                    content.Value().kind ==
                        Meta::ContentKind::Collection
                ? MemberWriteMode::Collection
                : MemberWriteMode::SetOnce,
            acceptsAnyValue,
            true};
    }
    Base::Result<Meta::ContentInfo> content =
        domain_->GetContentInfo(member.id);
    if (content && content.Value().writable) {
        return {
            content.Value().kind == Meta::ContentKind::Collection
                ? MemberWriteMode::Collection
                : MemberWriteMode::SetOnce,
            false,
            true};
    }
    return {};
}

} // namespace Aero::Markup

// XAML object construction and lifecycle operations.




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

bool SchemaHasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Result<Meta::TypeReference> ResolveTypeReference(
    Base::StringView name,
    const ExtensionServices& services) noexcept {
    if (services.schema == nullptr || name.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Type reference requires a qualified type name");
    }
    Base::StringView prefix;
    Base::StringView localName;
    switch (SplitQualifiedXamlName(name, prefix, localName)) {
    case QualifiedNameSplitStatus::MultipleColons:
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Type reference contains multiple namespace prefixes");
    case QualifiedNameSplitStatus::MalformedPrefix:
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Type reference namespace prefix is malformed");
    case QualifiedNameSplitStatus::Ok:
        break;
    }
    Base::Result<Base::StringView> uri =
        services.namespaces.Lookup(prefix);
    if (!uri) return uri.GetStatus();
    Base::Result<const Meta::TypeInfo*> resolved =
        services.schema->ResolveType(
            uri.Value(), localName);
    if (!resolved) return resolved.GetStatus();
    const Meta::TypeInfo* type = resolved.Value();
    if (type == nullptr ||
        SchemaHasTypeFlag(
            type->Flags(),
            Meta::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Type reference target was not found or is not an object type");
    }
    return Meta::TypeReference{type->Id()};
}

} // namespace

Base::Result<void> Schema::Freeze() noexcept {
    if (frozen_) return {};
    if (domain_ == nullptr || domain_ == nullptr ||
        !domain_->IsSealed() || !domain_->IsReady()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Meta::Registry must be complete before XAML schema freeze");
    }
    Base::Result<void> facetsFrozen =
        facets_->Freeze(domain_->Types());
    if (!facetsFrozen) return facetsFrozen.GetStatus();
    frozen_ = true;
    return {};
}

Base::Result<Meta::Value> Schema::ConvertText(
    Meta::TypeId type,
    Base::StringView text,
    const ExtensionServices* services) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    if (type == Meta::TypeOf<Meta::TypeReference>()) {
        if (services == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Type-reference conversion requires markup services");
        }
        Base::Result<Meta::TypeReference> reference =
            ResolveTypeReference(text, *services);
        return reference
            ? Meta::ValueCodec<Meta::TypeReference>::Encode(
                  reference.Value())
            : Base::Result<Meta::Value>(
                  reference.GetStatus());
    }
    // Members flagged AnyValue still need a concrete runtime value. Preserve
    // literal XAML text as a String so style and template finalizers can
    // convert it after resolving the actual target dependency property.
    if (type == Meta::TypeOf<Meta::Value>()) {
        return Meta::Value::TryFromString(
            Meta::TypeOf<Base::String>(), text);
    }
    if (type == Meta::TypeOf<Base::Object>()) {
        return BoxXamlStringAsObject(text);
    }
    const bool fontFamilyValue =
        type == Meta::TypeOf<Media::FontFamily>();
    const bool fontFamilySource =
        type == Meta::TypeOf<Base::String>() &&
        services != nullptr &&
        services->targetObjectType ==
            Media::FontFamily::StaticTypeId() &&
        services->targetMember == Meta::MakeMemberId(
            Media::FontFamily::StaticTypeId(),
            Meta::MemberKind::Property,
            "Source");
    if ((fontFamilyValue || fontFamilySource) &&
        services != nullptr &&
        services->baseUri != nullptr &&
        !services->baseUri->Empty()) {
        std::uint32_t familySeparator = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < text.SizeBytes(); ++index) {
            if (text[index] == '#') {
                familySeparator = index;
                break;
            }
        }
        if (familySeparator != UINT32_MAX && familySeparator != 0U) {
            Base::Result<Base::ResourceUri> uri =
                Base::ResourceUri::Resolve(
                    *services->baseUri,
                    text);
            if (!uri) return uri.GetStatus();
            const Base::StringView resolved =
                uri.Value().Scheme() == Base::StringView("file")
                ? uri.Value().Path()
                : uri.Value().Canonical();
            return domain_->TryConvertText(type, resolved);
        }
    }
    if (type == Meta::TypeOf<Base::ResourceUri>() &&
        services != nullptr &&
        services->baseUri != nullptr &&
        !services->baseUri->Empty()) {
        // A leading-slash WPF component URI names an assembly resource; it
        // must not inherit the file scheme of the containing XAML document.
        bool componentUri = false;
        if (!text.Empty() && text[0] == '/') {
            for (std::uint32_t index = 1U;
                 index + 11U <= text.SizeBytes(); ++index) {
                if (text.Substr(index, 11U) ==
                        Base::StringView(";component/")) {
                    componentUri = true;
                    break;
                }
            }
        }
        Base::Result<Base::ResourceUri> uri = componentUri
            ? Base::ResourceUri::Parse(text)
            : Base::ResourceUri::Resolve(
                  *services->baseUri,
                  text);
        if (!uri) return uri.GetStatus();
        return domain_->TryCreateValue(
            type,
            &uri.Value());
    }
    Base::Result<Meta::Value> converted =
        domain_->TryConvertText(type, text);
    if (!converted || services == nullptr ||
        services->baseUri == nullptr || services->baseUri->Empty() ||
        converted.Value().Kind() != Meta::ValueKind::Object ||
        converted.Value().IsNullObject()) {
        return converted;
    }
    const Base::Ref<Base::Object> object =
        converted.Value().AsObject();
    if (!object || object->RuntimeType() !=
            Media::BitmapImage::StaticTypeId()) {
        return converted;
    }
    auto& bitmap = static_cast<Media::BitmapImage&>(*object);
    const Base::ResourceUri authored = bitmap.GetUriSource();
    if (authored.Empty() || authored.IsAbsolute()) {
        return converted;
    }
    Base::Result<Base::ResourceUri> resolved =
        Base::ResourceUri::Resolve(*services->baseUri, authored.Canonical());
    if (!resolved) return resolved.GetStatus();
    bitmap.SetUriSource(std::move(resolved).Value());
    return converted;
}

Base::Result<ProvidedValue> Schema::ProvideMarkupExtensionValue(
    Meta::TypeId type,
    Base::StringView arguments,
    const ExtensionServices& services) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    const Meta::TypeInfo* info =
        domain_->Types().FindType(type);
    const XamlMarkupExtensionFacet* registration =
        facets_->FindMarkupExtension(type);
    if (registration != nullptr && registration->provideValue != nullptr) {
        return registration->provideValue(
            arguments,
            services,
            registration->context);
    }
    const bool isMarkupExtension =
        info != nullptr &&
        (SchemaHasTypeFlag(info->Flags(), Meta::TypeFlags::MarkupExtension) ||
         domain_->Types().IsDerivedFrom(
             type, MarkupExtension::StaticTypeId()));
    if (!isMarkupExtension) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageMissingMarkupExtension);
    }
    Base::Result<Base::Ref<Base::Object>> created = CreateObject(type);
    if (!created) return created.GetStatus();
    MarkupExtension* extension =
        ::Aero::TryCast<MarkupExtension>(created.Value().Get());
    if (extension == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageMissingMarkupExtension);
    }
    (void)arguments;
    MarkupExtensionContext context = MakeMarkupExtensionContext(services);
    Base::Result<Meta::Value> provided = extension->ProvideValue(context);
    if (!provided) return provided.GetStatus();
    return ProvidedValue::FromValue(std::move(provided).Value());
}

Base::Result<void> Schema::BeginInit(
    Meta::TypeId type,
    Base::Object& object) const noexcept {
    const Base::Span<const std::uint32_t> lifecycle =
        facets_->LifecyclePlan(type);
    for (std::uint32_t reference : lifecycle) {
        const XamlLifecycleFacet* facet =
            facets_->LifecycleAt(reference);
        if (facet == nullptr || facet->beginInit == nullptr) continue;
        Base::Result<void> initialized =
            facet->beginInit(object, facet->context);
        if (!initialized) return initialized.GetStatus();
    }
    return {};
}

Base::Result<void> Schema::EndInit(
    Meta::TypeId type,
    Base::Object& object,
    const ExtensionServices& services) const noexcept {
    const Base::Span<const std::uint32_t> lifecycle =
        facets_->LifecyclePlan(type);
    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            facets_->LifecycleAt(lifecycle[index - 1U]);
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
    Meta::TypeId type,
    Base::Object& object) const noexcept {
    const Base::Span<const std::uint32_t> lifecycle =
        facets_->LifecyclePlan(type);
    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            facets_->LifecycleAt(lifecycle[index - 1U]);
        if (facet != nullptr && facet->abortInit != nullptr) {
            facet->abortInit(object, facet->context);
        }
    }
}

bool Schema::CreatesNameScope(Meta::TypeId type) const noexcept {
    const XamlNameScopeFacet* facet = facets_->FindNameScope(
        type, domain_->Types());
    return facet != nullptr && facet->createsNameScope;
}

bool Schema::CreatesResourceScope(
    Meta::TypeId type) const noexcept {
    const XamlResourceScopeFacet* facet = facets_->FindResourceScope(
        type, domain_->Types());
    return facet != nullptr && facet->createsResourceScope;
}

bool Schema::DefersVisualContent(
    Meta::TypeId type) const noexcept {
    const XamlDeferredContentFacet* facet =
        facets_->FindDeferredContent(
        type, domain_->Types());
    return facet != nullptr && facet->defersVisualContent;
}

Base::Result<void> Schema::RegisterName(
    Meta::TypeId scopeType,
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object) const noexcept {
    const XamlNameScopeFacet* facet = facets_->FindNameScope(
        scopeType, domain_->Types());
    if (facet == nullptr || facet->registerName == nullptr) return {};
    return facet->registerName(
        scopeOwner, name, object, facet->context);
}

Base::Result<void> Schema::AddResource(
    Meta::TypeId scopeType,
    Base::Object& scopeOwner,
    const Aero::ResourceKey& key,
    const Meta::Value& value) const noexcept {
    const XamlResourceScopeFacet* facet = facets_->FindResourceScope(
        scopeType, domain_->Types());
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
    if (resources != nullptr) {
        resources->Add(key, value);
    }
    return {};
}

Aero::ResourceDictionary* Schema::ResolveResourceScope(
    Meta::TypeId scopeType,
    Base::Object& scopeOwner) const noexcept {
    const XamlResourceScopeFacet* facet = facets_->FindResourceScope(
        scopeType, domain_->Types());
    return facet != nullptr && facet->resolveResourceScope != nullptr
        ? facet->resolveResourceScope(scopeOwner, facet->context)
        : nullptr;
}

Base::Result<Aero::ResourceKey>
Schema::ResolveImplicitResourceKey(
    Meta::TypeId type,
    const Base::Object& object) const noexcept {
    const XamlImplicitResourceKeyFacet* facet =
        facets_->FindImplicitResourceKey(
            type, domain_->Types());
    if (facet == nullptr || facet->resolve == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML type has no implicit resource-key facet");
    }
    return facet->resolve(object, facet->context);
}

} // namespace Aero::Markup

