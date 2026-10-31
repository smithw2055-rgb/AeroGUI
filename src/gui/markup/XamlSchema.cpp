#include "gui/markup/XamlSchema.hpp"
#include "gui/markup/XamlBytecode.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
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
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls.hpp>
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
        [this](Meta::TypeId current) noexcept {
            return FindLifecycleExact(current);
        });
}

const XamlNameScopeFacet* XamlFacets::FindNameScope(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    if (const XamlNameScopeFacet* exact = FindNameScopeExact(type)) {
        return exact;
    }
    return FindByPolicy<XamlNameScopeFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindNameScopeExact(current);
        });
}

const XamlResourceScopeFacet* XamlFacets::FindResourceScope(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    if (const XamlResourceScopeFacet* exact = FindResourceScopeExact(type)) {
        return exact;
    }
    return FindByPolicy<XamlResourceScopeFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindResourceScopeExact(current);
        });
}

const XamlDeferredContentFacet* XamlFacets::FindDeferredContent(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    if (const XamlDeferredContentFacet* exact = FindDeferredContentExact(type)) {
        return exact;
    }
    return FindByPolicy<XamlDeferredContentFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindDeferredContentExact(current);
        });
}

const XamlImplicitResourceKeyFacet*
XamlFacets::FindImplicitResourceKey(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    if (const XamlImplicitResourceKeyFacet* exact = FindImplicitResourceKeyExact(type)) {
        return exact;
    }
    return FindByPolicy<XamlImplicitResourceKeyFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindImplicitResourceKeyExact(current);
        });
}

const XamlPropertyTargetFacet* XamlFacets::FindPropertyTarget(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    if (const XamlPropertyTargetFacet* exact = FindPropertyTargetExact(type)) {
        return exact;
    }
    return FindByPolicy<XamlPropertyTargetFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindPropertyTargetExact(current);
        });
}

const XamlMarkupExtensionFacet* XamlFacets::FindMarkupExtension(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::MarkupExtension);
    return index < markupExtensions_.Size()
        ? &markupExtensions_[index]
        : nullptr;
}

const XamlLifecycleFacet* XamlFacets::FindLifecycleExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(type, FacetKind::Lifecycle);
    return index < lifecycles_.Size() ? &lifecycles_[index] : nullptr;
}

const XamlNameScopeFacet* XamlFacets::FindNameScopeExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(type, FacetKind::NameScope);
    return index < nameScopes_.Size() ? &nameScopes_[index] : nullptr;
}

const XamlResourceScopeFacet* XamlFacets::FindResourceScopeExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::ResourceScope);
    return index < resourceScopes_.Size()
        ? &resourceScopes_[index]
        : nullptr;
}

const XamlDeferredContentFacet*
XamlFacets::FindDeferredContentExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::DeferredContent);
    return index < deferredContents_.Size()
        ? &deferredContents_[index]
        : nullptr;
}

const XamlImplicitResourceKeyFacet*
XamlFacets::FindImplicitResourceKeyExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::ImplicitResourceKey);
    return index < implicitResourceKeys_.Size()
        ? &implicitResourceKeys_[index]
        : nullptr;
}

const XamlPropertyTargetFacet*
XamlFacets::FindPropertyTargetExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::PropertyTarget);
    return index < propertyTargets_.Size()
        ? &propertyTargets_[index]
        : nullptr;
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
        Base::Result<Meta::Value> strVal =
            Meta::Value::TryFromString(Meta::TypeOf<Base::String>(), text);
        if (!strVal) return strVal.GetStatus();
        Base::Result<Base::Ref<Controls::BoxedItemValue>> boxed =
            Base::MakeRef<Controls::BoxedItemValue>(std::move(strVal).Value());
        if (!boxed) return boxed.GetStatus();
        return Meta::Value::FromObject(
            type,
            Base::Ref<Base::Object>(std::move(boxed).Value()));
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

// ============================================================================
// SECTION: XamlSchemaManifest
// Source: src/gui/markup/XamlSchemaManifest.cpp
// ============================================================================

// ===== SchemaManifest =====


// Immutable compiled-schema manifest implementation.



namespace Aero::Markup {

enum class ManifestMemberKind : std::uint8_t {
    Property = 0U,
    Event
};

namespace {

constexpr std::uint32_t ManifestMagic = UINT32_C(0x48435341); // ASCH
constexpr std::uint32_t ManifestEncodingVersion = 1U;

Base::Status InvalidManifest(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed, message);
}

Base::Status ManifestNotReady() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML schema manifest is not initialized");
}

Base::Status TypeNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML schema manifest type was not found");
}

Base::Status MemberNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML schema manifest member was not found");
}



} // namespace

struct SchemaManifest::State {
    struct TypeRecord {
        explicit TypeRecord(Base::IAllocator& allocator) noexcept
            : xamlNamespace(&allocator), name(&allocator) {}

        Meta::TypeId id = Meta::InvalidTypeId;
        Meta::TypeId baseType = Meta::InvalidTypeId;
        Meta::MetadataTypeKind kind = Meta::MetadataTypeKind::Object;
        Meta::TypeFlags flags = Meta::TypeFlags::None;
        Meta::MemberId contentMember = Meta::InvalidMemberId;
        Base::String xamlNamespace;
        Base::String name;
    };

    struct MemberRecord {
        explicit MemberRecord(Base::IAllocator& allocator) noexcept
            : name(&allocator) {}

        Meta::MemberId id = Meta::InvalidMemberId;
        ManifestMemberKind kind = ManifestMemberKind::Property;
        Meta::TypeId ownerType = Meta::InvalidTypeId;
        Meta::TypeId valueType = Meta::InvalidTypeId;
        std::uint32_t flags = 0U;
        Base::String name;
    };

    explicit State(Base::IAllocator& allocator) noexcept
        : types(&allocator),
          members(&allocator),
          typeIndex(&allocator),
          memberIndex(&allocator) {}

    CompiledCacheIdentity identity;
    Base::Vector<TypeRecord> types;
    Base::Vector<MemberRecord> members;
    Base::HashMap<Meta::TypeId, std::uint32_t> typeIndex;
    Base::HashMap<Meta::MemberId, std::uint32_t> memberIndex;
    bool valid = false;

    Base::Result<void> RebuildIndexes() noexcept {
        typeIndex.Clear();
        memberIndex.Clear();
        for (std::uint32_t index = 0U; index < types.Size(); ++index) {
            typename Base::HashMap<Meta::TypeId, std::uint32_t>::InsertResult inserted = typeIndex.Insert(types[index].id, index);
            if (!inserted.inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate TypeId values");
            }
        }
        for (std::uint32_t index = 0U; index < members.Size(); ++index) {
            typename Base::HashMap<Meta::MemberId, std::uint32_t>::InsertResult inserted = memberIndex.Insert(members[index].id, index);
            if (!inserted.inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate MemberId values");
            }
        }
        return {};
    }

    const TypeRecord* FindType(Meta::TypeId id) const noexcept {
        const std::uint32_t* index = typeIndex.Find(id);
        return index != nullptr && *index < types.Size()
            ? &types[*index] : nullptr;
    }

    const TypeRecord* FindType(
        Base::StringView xamlNamespace,
        Base::StringView name) const noexcept {
        for (const TypeRecord& type : types) {
            if (type.xamlNamespace.View() == xamlNamespace &&
                type.name.View() == name) {
                return &type;
            }
        }
        return nullptr;
    }

    const MemberRecord* FindMember(Meta::MemberId id) const noexcept {
        const std::uint32_t* index = memberIndex.Find(id);
        return index != nullptr && *index < members.Size()
            ? &members[*index] : nullptr;
    }

    const MemberRecord* FindMember(
        Meta::TypeId ownerType,
        Base::StringView name,
        ManifestMemberKind kind,
        bool includeBaseTypes) const noexcept {
        Meta::TypeId current = ownerType;
        for (std::uint32_t depth = 0U;
             current != Meta::InvalidTypeId && depth <= types.Size();
             ++depth) {
            for (const MemberRecord& member : members) {
                if (member.ownerType == current &&
                    member.kind == kind &&
                    member.name.View() == name) {
                    return &member;
                }
            }
            if (!includeBaseTypes) break;
            const TypeRecord* type = FindType(current);
            if (type == nullptr) break;
            current = type->baseType;
        }
        return nullptr;
    }

    bool IsDerivedFrom(
        Meta::TypeId type,
        Meta::TypeId expectedBase) const noexcept {
        if (type == Meta::InvalidTypeId ||
            expectedBase == Meta::InvalidTypeId) {
            return false;
        }
        Meta::TypeId current = type;
        for (std::uint32_t depth = 0U;
             current != Meta::InvalidTypeId && depth <= types.Size();
             ++depth) {
            if (current == expectedBase) return true;
            const TypeRecord* descriptor = FindType(current);
            if (descriptor == nullptr) return false;
            current = descriptor->baseType;
        }
        return false;
    }

    Base::Result<ResolvedMember> ResolvePropertyOrEvent(
        Meta::TypeId targetType,
        Meta::TypeId ownerType,
        Base::StringView memberName,
        MemberSyntax syntax,
        bool ownerWasExplicit) const noexcept {
        const MemberRecord* property = FindMember(
            ownerType,
            memberName,
            ManifestMemberKind::Property,
            true);
        if (property != nullptr &&
            syntax == MemberSyntax::Attribute &&
            HasPropertyFlag(
                static_cast<Meta::PropertyFlags>(property->flags),
                Meta::PropertyFlags::Collection)) {
            char aliasBuffer[128];
            if (memberName.SizeBytes() + 4U < sizeof(aliasBuffer)) {
                std::memcpy(aliasBuffer, memberName.Data(), memberName.SizeBytes());
                std::memcpy(aliasBuffer + memberName.SizeBytes(), "Text", 4U);
                const MemberRecord* alias = FindMember(
                    ownerType,
                    Base::StringView(aliasBuffer, memberName.SizeBytes() + 4U),
                    ManifestMemberKind::Property,
                    true);
                if (alias != nullptr &&
                    !HasPropertyFlag(
                        static_cast<Meta::PropertyFlags>(alias->flags),
                        Meta::PropertyFlags::Collection)) {
                    property = alias;
                }
            }
        }
        if (property != nullptr) {
            const Meta::PropertyFlags flags =
                static_cast<Meta::PropertyFlags>(property->flags);
            const bool attached = HasPropertyFlag(
                flags, Meta::PropertyFlags::Attached);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute && !attached &&
                !IsDerivedFrom(targetType, property->ownerType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Explicit XAML attribute owner requires an attached property");
            }
            if (ownerWasExplicit &&
                syntax == MemberSyntax::PropertyElement && !attached &&
                !IsDerivedFrom(targetType, property->ownerType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "XAML property element owner is incompatible with the target type");
            }
            return MakeResolvedPropertyMember(
                property->id,
                property->ownerType,
                property->valueType,
                flags);
        }

        const MemberRecord* event = FindMember(
            ownerType,
            memberName,
            ManifestMemberKind::Event,
            true);
        if (event != nullptr) {
            const Meta::EventFlags flags =
                static_cast<Meta::EventFlags>(event->flags);
            const bool attached = HasEventFlag(
                flags, Meta::EventFlags::Attached);
            const bool routed = HasEventFlag(
                flags, Meta::EventFlags::Routed);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute &&
                !attached && !routed) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Explicit XAML attribute owner requires an attached event");
            }
            if (ownerWasExplicit &&
                syntax == MemberSyntax::PropertyElement && !attached &&
                !IsDerivedFrom(targetType, event->ownerType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "XAML event element owner is incompatible with the target type");
            }
            ResolvedMember resolved = MakeResolvedEventMember(
                event->id,
                event->ownerType,
                event->valueType,
                flags);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute && routed) {
                resolved.attached = true;
            }
            return resolved;
        }
        return MemberNotFound();
    }
};

static_assert(
    sizeof(SchemaManifest::State) <= 2048,
    "SchemaManifest inline state storage is too small");
static_assert(
    alignof(SchemaManifest::State) <= alignof(std::max_align_t),
    "SchemaManifest inline state alignment is insufficient");

namespace {

template<class T>
Base::Result<T*> AllocateObject(
    Base::IAllocator& allocator) noexcept {
    void* memory = allocator.Allocate({
        sizeof(T), alignof(T), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML schema manifest allocation failed");
    }
    return new (memory) T(allocator);
}

void DestroyManifestState(
    Base::IAllocator& allocator,
    SchemaManifest::State*& state) noexcept {
    if (state == nullptr) return;
    state->~State();
    allocator.Deallocate(
        state,
        sizeof(SchemaManifest::State),
        alignof(SchemaManifest::State),
        Base::MemoryTag::Markup);
    state = nullptr;
}

Base::Result<void> AppendIdentity(
    Base::Vector<std::uint8_t>& output,
    const CompiledCacheIdentity& identity) noexcept {
    Base::Result<void> result = AppendU32(
        output, identity.cacheFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.typeIdAlgorithmVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.metadataSchemaFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.metadataProgramFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.schemaVersion);
    if (!result) return result.GetStatus();
    return AppendU64(output, identity.metadataSchemaHash);
}

Base::Result<CompiledCacheIdentity> ReadIdentity(
    Decoder& decoder) noexcept {
    CompiledCacheIdentity identity;
    Base::Result<std::uint32_t> value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.cacheFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.typeIdAlgorithmVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.metadataSchemaFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.metadataProgramFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.schemaVersion = value.Value();
    Base::Result<std::uint64_t> hash = decoder.ReadU64();
    if (!hash) return hash.GetStatus();
    identity.metadataSchemaHash = hash.Value();
    return identity;
}

} // namespace

SchemaManifest::SchemaManifest(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator : &Base::GetDefaultAllocator()) {}

SchemaManifest::SchemaManifest(
    Base::IAllocator& allocator,
    SchemaManifest::State* state) noexcept
    : allocator_(&allocator),
      state_(state) {}

SchemaManifest::~SchemaManifest() noexcept {
    if (state_ == nullptr) return;
    DestroyManifestState(*allocator_, state_);
    state_ = nullptr;
}

SchemaManifest::SchemaManifest(
    SchemaManifest&& other) noexcept
    : allocator_(other.allocator_),
      state_(other.state_) {
    other.state_ = nullptr;
    other.allocator_ = &Base::GetDefaultAllocator();
}

SchemaManifest& SchemaManifest::operator=(
    SchemaManifest&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        DestroyManifestState(*allocator_, state_);
    }
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.state_ = nullptr;
    other.allocator_ = &Base::GetDefaultAllocator();
    return *this;
}

Base::Result<SchemaManifest> SchemaManifest::Capture(
    const Schema& schema,
    Base::IAllocator* allocator) noexcept {
    if (!schema.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML schema manifest capture requires a frozen schema");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Base::Result<SchemaManifest::State*> created = AllocateObject<SchemaManifest::State>(selected);
    if (!created) return created.GetStatus();
    SchemaManifest::State* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(schema.Domain());
    if (!identity) {
        DestroyManifestState(selected, impl);
        return identity.GetStatus();
    }
    impl->identity = identity.Value();

    const Meta::TypeRegistry& descriptors = schema.Types();
    impl->types.Reserve(descriptors.TypeCount());
    impl->members.Reserve(
        descriptors.PropertyCount() + descriptors.EventCount());

    for (const Meta::TypeInfo& type : descriptors.Types()) {
        SchemaManifest::State::TypeRecord record(selected);
        record.id = type.Id();
        record.baseType = type.BaseType();
        record.kind = type.Kind();
        record.flags = type.Flags();
        Base::Result<void> assigned = record.xamlNamespace.Assign(
            type.XamlNamespace());
        if (assigned) assigned = record.name.Assign(type.Name());
        if (!assigned) {
            DestroyManifestState(selected, impl);
            return assigned.GetStatus();
        }
        Base::Result<ResolvedMember> content =
            schema.ResolveContentMember(type.Id());
        if (content) {
            record.contentMember = content.Value().id;
        } else if (content.GetStatus().code != Base::ErrorCode::NotFound) {
            DestroyManifestState(selected, impl);
            return content.GetStatus();
        }
        impl->types.PushBack(
            std::move(record));

        for (const Meta::PropertyInfo& property : type.Properties()) {
            SchemaManifest::State::MemberRecord member(selected);
            member.id = property.Id();
            member.kind = ManifestMemberKind::Property;
            member.ownerType = property.OwnerType();
            member.valueType = property.ValueType();
            member.flags = static_cast<std::uint32_t>(property.Flags());
            assigned = member.name.Assign(property.Name());
            if (!assigned) {
                DestroyManifestState(selected, impl);
                return assigned.GetStatus();
            }
            impl->members.PushBack(std::move(member));
        }

        for (const Meta::EventInfo& event : type.Events()) {
            SchemaManifest::State::MemberRecord member(selected);
            member.id = event.Id();
            member.kind = ManifestMemberKind::Event;
            member.ownerType = event.OwnerType();
            member.valueType = event.EventArgsType();
            member.flags = static_cast<std::uint32_t>(event.Flags());
            assigned = member.name.Assign(event.Name());
            if (!assigned) {
                DestroyManifestState(selected, impl);
                return assigned.GetStatus();
            }
            impl->members.PushBack(std::move(member));
        }
    }

    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyManifestState(selected, impl);
        return indexed.GetStatus();
    }
    impl->valid = true;
    return SchemaManifest(selected, impl);
}

Base::Result<SchemaManifest> SchemaManifest::Deserialize(
    Base::Span<const std::uint8_t> bytes,
    const SchemaManifestLimits& limits,
    Base::IAllocator* allocator) noexcept {
    if (limits.maxTypes == 0U ||
        limits.maxMembers == 0U ||
        limits.maxStringBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML schema manifest limits must be positive");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Decoder decoder(bytes, "XAML schema manifest payload is truncated");
    Base::Result<std::uint32_t> magic = decoder.ReadU32();
    if (!magic) return magic.GetStatus();
    Base::Result<std::uint32_t> encoding = decoder.ReadU32();
    if (!encoding) return encoding.GetStatus();
    Base::Result<std::uint32_t> format = decoder.ReadU32();
    if (!format) return format.GetStatus();
    if (magic.Value() != ManifestMagic ||
        encoding.Value() != ManifestEncodingVersion ||
        format.Value() != XamlSchemaManifestFormatVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML schema manifest format is not supported");
    }

    Base::Result<SchemaManifest::State*> created = AllocateObject<SchemaManifest::State>(selected);
    if (!created) return created.GetStatus();
    SchemaManifest::State* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity = ReadIdentity(decoder);
    if (!identity) {
        DestroyManifestState(selected, impl);
        return identity.GetStatus();
    }
    CompiledCacheIdentity current;
    current.metadataSchemaHash = identity.Value().metadataSchemaHash;
    if (CompareCompiledCacheIdentity(identity.Value(), current) !=
        CompiledCacheCompatibility::Compatible) {
        DestroyManifestState(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML schema manifest ABI is incompatible with this tool");
    }
    impl->identity = identity.Value();

    Base::Result<std::uint32_t> typeCount = decoder.ReadU32();
    if (!typeCount) {
        DestroyManifestState(selected, impl);
        return typeCount.GetStatus();
    }
    Base::Result<std::uint32_t> memberCount = decoder.ReadU32();
    if (!memberCount) {
        DestroyManifestState(selected, impl);
        return memberCount.GetStatus();
    }
    if (typeCount.Value() > limits.maxTypes ||
        memberCount.Value() > limits.maxMembers) {
        DestroyManifestState(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML schema manifest descriptor count exceeds limits");
    }
    impl->types.Reserve(typeCount.Value());
    impl->members.Reserve(memberCount.Value());

    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U; index < typeCount.Value(); ++index) {
        SchemaManifest::State::TypeRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyManifestState(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint64_t> baseType = decoder.ReadU64();
        if (!baseType) {
            DestroyManifestState(selected, impl);
            return baseType.GetStatus();
        }
        Base::Result<std::uint32_t> kind = decoder.ReadU32();
        if (!kind) {
            DestroyManifestState(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyManifestState(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<std::uint64_t> content = decoder.ReadU64();
        if (!content) {
            DestroyManifestState(selected, impl);
            return content.GetStatus();
        }
        Base::Result<Base::String> xamlNamespace = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!xamlNamespace) {
            DestroyManifestState(selected, impl);
            return xamlNamespace.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyManifestState(selected, impl);
            return name.GetStatus();
        }
        if (id.Value() == Meta::InvalidTypeId || name.Value().Empty() ||
            Meta::MakeTypeId(
                xamlNamespace.Value().View(),
                name.Value().View()) != id.Value() ||
            kind.Value() > static_cast<std::uint32_t>(Meta::MetadataTypeKind::Primitive)) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest type descriptor is invalid");
        }
        record.id = id.Value();
        record.baseType = baseType.Value();
        record.kind = static_cast<Meta::MetadataTypeKind>(kind.Value());
        record.flags = static_cast<Meta::TypeFlags>(flags.Value());
        record.contentMember = content.Value();
        record.xamlNamespace = std::move(xamlNamespace).Value();
        record.name = std::move(name).Value();
        impl->types.PushBack(std::move(record));
    }

    for (std::uint32_t index = 0U; index < memberCount.Value(); ++index) {
        SchemaManifest::State::MemberRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyManifestState(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint8_t> kind = decoder.ReadU8();
        if (!kind) {
            DestroyManifestState(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> reservedField = decoder.ReadU32();
        if (!reservedField) {
            DestroyManifestState(selected, impl);
            return reservedField.GetStatus();
        }
        Base::Result<std::uint64_t> owner = decoder.ReadU64();
        if (!owner) {
            DestroyManifestState(selected, impl);
            return owner.GetStatus();
        }
        Base::Result<std::uint64_t> valueType = decoder.ReadU64();
        if (!valueType) {
            DestroyManifestState(selected, impl);
            return valueType.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyManifestState(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyManifestState(selected, impl);
            return name.GetStatus();
        }
        const bool validKind =
            kind.Value() <= static_cast<std::uint8_t>(ManifestMemberKind::Event);
        const Meta::MemberKind metadataKind =
            kind.Value() == static_cast<std::uint8_t>(ManifestMemberKind::Event)
            ? Meta::MemberKind::Event
            : Meta::MemberKind::Property;
        if (id.Value() == Meta::InvalidMemberId ||
            owner.Value() == Meta::InvalidTypeId ||
            valueType.Value() == Meta::InvalidTypeId ||
            name.Value().Empty() ||
            reservedField.Value() != 0U ||
            !validKind ||
            Meta::MakeMemberId(
                owner.Value(), metadataKind, name.Value().View()) != id.Value()) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest member descriptor is invalid");
        }
        record.id = id.Value();
        record.kind = static_cast<ManifestMemberKind>(kind.Value());
        record.ownerType = owner.Value();
        record.valueType = valueType.Value();
        record.flags = flags.Value();
        record.name = std::move(name).Value();
        impl->members.PushBack(std::move(record));
    }

    if (!decoder.AtEnd()) {
        DestroyManifestState(selected, impl);
        return InvalidManifest("XAML schema manifest has trailing bytes");
    }
    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyManifestState(selected, impl);
        return indexed.GetStatus();
    }
    for (const SchemaManifest::State::TypeRecord& type : impl->types) {
        if (type.baseType != Meta::InvalidTypeId &&
            impl->FindType(type.baseType) == nullptr) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest base type is missing");
        }
        Meta::TypeId ancestorId = type.id;
        std::uint32_t depth = 0U;
        while (ancestorId != Meta::InvalidTypeId && depth <= impl->types.Size()) {
            const SchemaManifest::State::TypeRecord* currentType = impl->FindType(ancestorId);
            if (currentType == nullptr) break;
            ancestorId = currentType->baseType;
            ++depth;
        }
        if (ancestorId != Meta::InvalidTypeId) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest type hierarchy contains a cycle");
        }
        if (type.contentMember != Meta::InvalidMemberId) {
            const SchemaManifest::State::MemberRecord* content = impl->FindMember(type.contentMember);
            if (content == nullptr ||
                content->kind != ManifestMemberKind::Property ||
                !impl->IsDerivedFrom(type.id, content->ownerType)) {
                DestroyManifestState(selected, impl);
                return InvalidManifest("XAML schema manifest content member is missing or incompatible");
            }
        }
    }
    for (const SchemaManifest::State::MemberRecord& member : impl->members) {
        if (impl->FindType(member.ownerType) == nullptr ||
            impl->FindType(member.valueType) == nullptr) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest member type is missing");
        }
    }
    impl->valid = true;
    return SchemaManifest(selected, impl);
}

Base::Result<Base::Vector<std::uint8_t>>
SchemaManifest::Serialize() const noexcept {
    if (!IsValid()) return ManifestNotReady();
    Base::Vector<std::uint8_t> output(allocator_);
    Base::Result<void> result = AppendU32(output, ManifestMagic);
    if (result) result = AppendU32(output, ManifestEncodingVersion);
    if (result) result = AppendU32(output, XamlSchemaManifestFormatVersion);
    if (result) result = AppendIdentity(output, state_->identity);
    if (result) result = AppendU32(output, state_->types.Size());
    if (result) result = AppendU32(output, state_->members.Size());
    if (!result) return result.GetStatus();

    for (const SchemaManifest::State::TypeRecord& type : state_->types) {
        result = AppendU64(output, type.id);
        if (result) result = AppendU64(output, type.baseType);
        if (result) result = AppendU32(
            output, static_cast<std::uint32_t>(type.kind));
        if (result) result = AppendU32(
            output, static_cast<std::uint32_t>(type.flags));
        if (result) result = AppendU64(output, type.contentMember);
        if (result) result = AppendString(output, type.xamlNamespace.View());
        if (result) result = AppendString(output, type.name.View());
        if (!result) return result.GetStatus();
    }
    for (const SchemaManifest::State::MemberRecord& member : state_->members) {
        result = AppendU64(output, member.id);
        if (result) result = AppendU8(
            output, static_cast<std::uint8_t>(member.kind));
        if (result) result = AppendU32(output, 0U);
        if (result) result = AppendU64(output, member.ownerType);
        if (result) result = AppendU64(output, member.valueType);
        if (result) result = AppendU32(output, member.flags);
        if (result) result = AppendString(output, member.name.View());
        if (!result) return result.GetStatus();
    }
    return output;
}

bool SchemaManifest::IsValid() const noexcept {
    return state_ != nullptr && state_->valid;
}

std::uint32_t SchemaManifest::TypeCount() const noexcept {
    return IsValid() ? state_->types.Size() : 0U;
}

std::uint32_t SchemaManifest::MemberCount() const noexcept {
    return IsValid() ? state_->members.Size() : 0U;
}

const CompiledCacheIdentity& SchemaManifest::Identity() const noexcept {
    AERO_ASSERT(IsValid());
    return state_->identity;
}

Base::Result<SchemaTypeInfo> SchemaManifest::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifest::State::TypeRecord* type = state_->FindType(
        IsSystemNamespace(xamlNamespace)
            ? Meta::AeroNamespaceUri()
            : CanonicalXamlNamespace(xamlNamespace),
        CanonicalXamlTypeName(localName));
    if (type == nullptr) return TypeNotFound();
    return SchemaTypeInfo{type->id, type->kind, type->flags};
}

Base::Result<ResolvedMember> SchemaManifest::ResolveMember(
    Meta::TypeId targetType,
    const QualifiedName& name,
    MemberSyntax syntax) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifest::State::TypeRecord* target = state_->FindType(targetType);
    if (target == nullptr || name.LocalName().Empty()) return MemberNotFound();

    Base::Result<ParsedXamlMemberName> parsed =
        ParseXamlMemberName(name.LocalName(), name.NamespaceUri(), target->xamlNamespace.View());
    if (!parsed) return parsed.GetStatus();

    const ParsedXamlMemberName& member = parsed.Value();
    if (!member.hasOwner) {
        if (!name.NamespaceUri().Empty() &&
            CanonicalXamlNamespace(name.NamespaceUri()) !=
                target->xamlNamespace.View()) {
            return MemberNotFound();
        }
        Base::Result<ResolvedMember> resolved = state_->ResolvePropertyOrEvent(
            targetType, targetType, member.memberName, syntax, false);
        if (resolved || member.memberName != Base::StringView("ToolTip")) {
            return resolved;
        }

        // WPF exposes ToolTip as a FrameworkElement property even though the
        // storage and display policy live in ToolTipService.
        const SchemaManifest::State::TypeRecord* service =
            state_->FindType(Meta::AeroNamespaceUri(), "ToolTipService");
        if (service == nullptr) return resolved.GetStatus();
        return state_->ResolvePropertyOrEvent(
            targetType, service->id, member.memberName, syntax, false);
    }

    if (IsAeroExtensionsFacade(
            member.ownerNamespace, member.ownerName)) {
        const SchemaManifest::State::TypeRecord* aeroOwner = state_->FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(member.ownerName));
        if (aeroOwner != nullptr) {
            return state_->ResolvePropertyOrEvent(
                targetType, aeroOwner->id, member.memberName, syntax, true);
        }
        return state_->ResolvePropertyOrEvent(
            targetType,
            targetType,
            member.memberName,
            syntax,
            false);
    }
    const SchemaManifest::State::TypeRecord* owner = state_->FindType(
        CanonicalXamlNamespace(member.ownerNamespace),
        CanonicalXamlTypeName(member.ownerName));
    if (owner == nullptr && name.NamespaceUri().Empty()) {
        owner = state_->FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(member.ownerName));
    }
    if (owner == nullptr) return MemberNotFound();
    // WPF exposes ContextMenu through FrameworkElement property-element
    // syntax (for example Border.ContextMenu) while storage is supplied by
    // the attached ContextMenuService property.
    if (member.memberName == Base::StringView("ContextMenu")) {
        const SchemaManifest::State::TypeRecord* service = state_->FindType(
            Meta::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return state_->ResolvePropertyOrEvent(
                targetType, service->id, member.memberName, syntax, true);
        }
    }
    return state_->ResolvePropertyOrEvent(
        targetType, owner->id, member.memberName, syntax, true);
}

Base::Result<ResolvedMember> SchemaManifest::ResolveContentMember(
    Meta::TypeId targetType) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifest::State::TypeRecord* type = state_->FindType(targetType);
    if (type == nullptr) return TypeNotFound();
    if (type->contentMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML schema manifest type has no content member");
    }
    const SchemaManifest::State::MemberRecord* member = state_->FindMember(type->contentMember);
    if (member == nullptr || member->kind != ManifestMemberKind::Property) {
        return InvalidManifest("XAML schema manifest content member is invalid");
    }
    return MakeResolvedPropertyMember(
        member->id,
        member->ownerType,
        member->valueType,
        static_cast<Meta::PropertyFlags>(member->flags));
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: StyleSupport
// Source: src/gui/markup/StyleSupport.cpp
// ============================================================================

// ===== StyleSupport =====






namespace Aero::Markup {

namespace {

Base::Status InvalidStyleXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

bool HasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status MissingStyleProperty(
    const char* role,
    Base::StringView property,
    Meta::TypeId targetType,
    const Meta::TypeRegistry& types) noexcept {
    thread_local char message[384];
    const Meta::TypeInfo* target =
        types.FindType(targetType);
    const Base::StringView typeName =
        target != nullptr
        ? target->Name()
        : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "Style %s '%.*s' was not found on TargetType '%.*s'",
        role,
        static_cast<int>(property.SizeBytes()),
        property.Data(),
        static_cast<int>(typeName.SizeBytes()),
        typeName.Data());
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        message);
}

inline const Meta::DependencyProperty* ResolveStyleProperty(
    const Meta::DependencyPropertyRegistry& properties,
    Meta::TypeId targetType,
    Base::StringView name) noexcept {
    return ResolveXamlDependencyProperty(properties, targetType, name);
}

Base::Result<Meta::PropertyValue> ToPropertyValue(
    const Meta::Value& value,
    Meta::TypeId expectedType) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML style value cannot be represented as a dependency-property value");
    }
    if (value.IsNullObject() && value.Type() != expectedType) {
        return Meta::PropertyValue::NullObject(expectedType);
    }
    return value;
}

Aero::ResourceDictionary* ResolveStyleResources(
    Base::Object& object,
    void*) noexcept {
    return object.RuntimeType() ==
            Aero::Style::StaticTypeId()
        ? &static_cast<Aero::Style&>(object).GetResources()
        : nullptr;
}

Base::Result<Aero::ResourceKey> ResolveStyleImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    if (object.RuntimeType() != Aero::Style::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Implicit Style key requires a Style object");
    }
    const Meta::TypeId target =
        static_cast<const Aero::Style&>(object).GetTargetType();
    if (target == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Implicit Style requires TargetType");
    }
    return Aero::ResourceKey::FromType(target);
}

} // namespace

} // namespace Aero::Markup

namespace Aero::Markup {

XamlStyleSchemaFacet::XamlStyleSchemaFacet(
    const UiObjectModelOptions& options) noexcept
    : options_(options) {}

Base::Result<void> XamlStyleSchemaFacet::Register(
    Schema& schema,
    Meta::TypeId styleType,
    Meta::TypeId setterType,
    Meta::DependencyPropertyHandle styleProperty,
    Meta::TypeId triggerType) noexcept {
    if (schema.IsFrozen() ||
        options_.properties == nullptr || !styleProperty.IsValid() ||
        !options_.properties->IsFrozen() || !options_.properties->Types().IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML Style extension registries are not ready");
    }
    if (schema_ != nullptr ||
        styleType == Meta::InvalidTypeId || setterType == Meta::InvalidTypeId) {
        return InvalidStyleXaml("XAML Style extension registration is invalid");
    }

    const Meta::TypeInfo* styleInfo = schema.Types().FindType(styleType);
    const Meta::TypeInfo* setterInfo = schema.Types().FindType(setterType);
    const Meta::TypeInfo* triggerInfo =
        triggerType != Meta::InvalidTypeId ? schema.Types().FindType(triggerType) : nullptr;
    if (styleInfo == nullptr || setterInfo == nullptr ||
        HasTypeFlag(styleInfo->Flags(), Meta::TypeFlags::ValueType) ||
        HasTypeFlag(setterInfo->Flags(), Meta::TypeFlags::ValueType) ||
        (triggerType != Meta::InvalidTypeId &&
         (triggerInfo == nullptr || HasTypeFlag(triggerInfo->Flags(), Meta::TypeFlags::ValueType)))) {
        return InvalidStyleXaml("XAML Style, Setter, and Trigger must be registered object types");
    }


    const Meta::DependencyProperty* styleDependency = options_.properties->Find(styleProperty);
    if (styleDependency == nullptr || styleDependency->ValueType() != styleType) {
        return InvalidStyleXaml("XAML Style property does not accept the registered Style type");
    }

    schema_ = &schema;
    styleType_ = styleType;
    setterType_ = setterType;
    triggerType_ = triggerType;
    Base::Result<void> styleAdapter =
        schema.AddType({
        styleType_,
        nullptr,
        &EndStyleInit,
        nullptr,
        this,
        false,
        true,
        nullptr,
        nullptr,
        &ResolveStyleResources,
        nullptr,
        false,
        &ResolveStyleImplicitKey});
    if (!styleAdapter) return styleAdapter.GetStatus();
    return {};
}

Base::Result<Meta::PropertyValue> XamlStyleSchemaFacet::ConvertValueForProperty(
    const Meta::Value& value,
    Meta::TypeId targetType,
    Base::StringView propertyName) const noexcept {
    if (schema_ == nullptr || options_.properties == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Style conversion requires an initialized extension");
    }
    // Setter.Value="{Binding ...}" is a declaration. Converting it to the
    // target DP type (Canvas.Left is double) would drop the Binding and
    // fail style sealing, so ListBoxItem orbit placement never applied.
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        (value.Type() == Data::Binding::StaticTypeId() ||
         (value.AsObject() &&
          value.AsObject()->RuntimeType() ==
              Data::Binding::StaticTypeId()))) {
        return value;
    }
    const Meta::DependencyProperty* property =
        ResolveStyleProperty(
            *options_.properties,
            targetType,
            propertyName);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style property was not found on TargetType");
    }
    const Meta::Value* candidate = &value;
    Base::Result<Meta::Value> converted = Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Style conversion was not attempted");
    if (candidate->Kind() == Meta::ValueKind::String) {
        converted = schema_->ConvertText(property->ValueType(), candidate->AsString());
        if (!converted) return converted.GetStatus();
        candidate = &converted.Value();
    }
    Base::Result<Meta::Value> coerced = CoerceXamlValueForProperty(
        *property, *candidate, options_.properties->Types());
    if (!coerced) return coerced.GetStatus();
    return ToPropertyValue(coerced.Value(), property->ValueType());
}

bool SetterValueIsPending(const Aero::Setter* setter) noexcept {
    return setter != nullptr &&
        !setter->GetPropertyName().Empty() &&
        setter->GetAuthoredValue().IsUnset();
}

bool StyleHasPendingResourceValues(Aero::Style& style) noexcept {
    const Aero::SetterBaseCollection setters = style.GetSetters();
    for (std::uint32_t index = 0U; index < setters.GetCount(); ++index) {
        if (SetterValueIsPending(
                ::Aero::TryCast<Aero::Setter>(setters.GetItem(index)))) {
            return true;
        }
    }
    const Aero::TriggerCollection triggers = style.GetTriggers();
    for (std::uint32_t index = 0U; index < triggers.GetCount(); ++index) {
        Aero::TriggerBase* authored = triggers.GetItem(index);
        if (authored == nullptr) {
            continue;
        }
        if (auto* setterTrigger = ::Aero::TryCast<Aero::SetterTriggerBase>(authored)) {
            if (authored->RuntimeType() == Aero::Trigger::StaticTypeId()) {
                auto* trigger = static_cast<Aero::Trigger*>(authored);
                if (!trigger->GetPropertyName().Empty() &&
                    trigger->GetAuthoredValue().IsUnset()) {
                    return true;
                }
            } else if (authored->RuntimeType() == Aero::DataTrigger::StaticTypeId()) {
                auto* trigger = static_cast<Aero::DataTrigger*>(authored);
                if (trigger->GetBinding() &&
                    trigger->GetAuthoredValue().IsUnset()) {
                    return true;
                }
            } else if (authored->RuntimeType() ==
                    Aero::MultiDataTrigger::StaticTypeId()) {
                auto* trigger =
                    static_cast<Aero::MultiDataTrigger*>(authored);
                for (const Base::Ref<Aero::Condition>& condition :
                     trigger->GetConditions()) {
                    if (condition &&
                        condition->GetBinding() &&
                        condition->GetAuthoredValue().IsUnset()) {
                        return true;
                    }
                }
            }
            for (const Base::Ref<Aero::Setter>& setterEntry :
                 setterTrigger->GetAuthoredSetters()) {
                if (SetterValueIsPending(setterEntry.Get())) {
                    return true;
                }
            }
            continue;
        }
    }
    return false;
}


Base::Result<void> XamlStyleSchemaFacet::FinalizeStyle(
    Aero::Style& style) noexcept {
    if (style.GetIsSealed()) {
        return {};
    }
    // Source-backed sibling dictionaries are committed after this EndInit
    // callback. A Setter.Value="{StaticResource ...}" whose key lives in a
    // theme/merged dictionary is queued as a deferred write; sealing now
    // would report "Style Setter requires Value". Leave the Style unsealed
    // until ObjectWriter reapplies EndInit after those writes.
    if (StyleHasPendingResourceValues(style)) {
        return {};
    }
    if (schema_ == nullptr || options_.properties == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Style TargetType must be assigned before initialization completes");
    }
    if (style.GetTargetType() == Meta::InvalidTypeId) {
        if (!style.SetTargetType(
                Controls::Control::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style TargetType assignment failed");
        }
    }
    const Meta::TypeId targetType = style.GetTargetType();
    const Meta::TypeInfo* targetInfo =
        options_.properties->Types().FindType(targetType);
    if (targetInfo == nullptr ||
        HasTypeFlag(
            targetInfo->Flags(),
            Meta::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Style TargetType must identify an object type");
    }
    auto resolveTriggerSetters = [&](
        Aero::SetterTriggerBase& trigger,
        const char* triggerKindName) -> Base::Result<void> {
        for (const Base::Ref<Aero::Setter>& setterEntry :
             trigger.GetAuthoredSetters()) {
            Aero::Setter* setter = setterEntry.Get();
            if (setter == nullptr || !setter->GetIsAuthored()) {
                thread_local char message[256]{};
                std::snprintf(
                    message,
                    sizeof(message),
                    "Style %s Setter requires Property and Value",
                    triggerKindName);
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    message);
            }
            const Meta::DependencyProperty* property =
                ResolveStyleProperty(
                    *options_.properties,
                    targetType,
                    setter->GetPropertyName());
            if (property == nullptr) {
                thread_local char message[256]{};
                std::snprintf(
                    message,
                    sizeof(message),
                    "%s Setter property",
                    triggerKindName);
                return MissingStyleProperty(
                    message,
                    setter->GetPropertyName(),
                    targetType,
                    options_.properties->Types());
            }
            Base::Result<Meta::PropertyValue> value =
                ConvertValueForProperty(
                    setter->GetAuthoredValue(),
                    targetType,
                    setter->GetPropertyName());
            if (!value) return value.GetStatus();
            if (!setter->Resolve(property->Handle(), value.Value())) {
                return InvalidStyle("Setter resolve value is invalid");
            }
            setter->ReleaseAuthored();
        }
        return {};
    };
    for (const Base::Ref<Aero::SetterBase>& entry :
         style.GetAuthoredSetters()) {
        Aero::Setter* setter =
            ::Aero::TryCast<Aero::Setter>(entry.Get());
        if (setter == nullptr) {
            continue;
        }
        if (setter->GetPropertyName().Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Setter requires Property");
        }
        if (setter->GetAuthoredValue().IsUnset()) {
            thread_local char message[256]{};
            const Base::StringView propertyName =
                setter->GetPropertyName();
            std::snprintf(
                message,
                sizeof(message),
                "Style Setter for '%.*s' requires Value",
                static_cast<int>(propertyName.SizeBytes()),
                propertyName.Data());
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                message);
        }
        const Meta::DependencyProperty* property =
            ResolveStyleProperty(
                *options_.properties,
                targetType,
                setter->GetPropertyName());
        if (property == nullptr) {
            return MissingStyleProperty(
                "Setter property",
                setter->GetPropertyName(),
                targetType,
                options_.properties->Types());
        }
        Base::Result<Meta::PropertyValue> value = ConvertValueForProperty(
            setter->GetAuthoredValue(),
            targetType,
            setter->GetPropertyName());
        if (!value) return value.GetStatus();
        if (!setter->Resolve(property->Handle(), value.Value())) {
            return InvalidStyle("Setter resolve value is invalid");
        }
        style.AddSetter(*setter);
        setter->ReleaseAuthored();
    }
    for (const Base::Ref<Aero::TriggerBase>& entry :
         style.GetAuthoredTriggers()) {
        Aero::TriggerBase* authored = entry.Get();
        if (authored == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Trigger cannot be null");
        }
        if (authored->RuntimeType() == Aero::Trigger::StaticTypeId()) {
            auto* trigger = static_cast<Aero::Trigger*>(authored);
            if (!trigger->GetIsAuthored()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style Trigger requires Property and Value");
            }
            const Meta::DependencyProperty* condition =
                ResolveStyleProperty(
                    *options_.properties,
                    targetType,
                    trigger->GetPropertyName());
            if (condition == nullptr) {
                return MissingStyleProperty(
                    "Trigger property",
                    trigger->GetPropertyName(),
                    targetType,
                    options_.properties->Types());
            }
            Base::Result<Meta::PropertyValue> conditionValue =
                ConvertValueForProperty(
                    trigger->GetAuthoredValue(),
                    targetType,
                    trigger->GetPropertyName());
            if (!conditionValue) return conditionValue.GetStatus();
            trigger->SetProperty(condition->Handle());
            trigger->SetValue(conditionValue.Value());
            Base::Result<void> settersResolved = resolveTriggerSetters(
                *trigger, "Trigger");
            if (!settersResolved) return settersResolved.GetStatus();
            style.AddTrigger(*trigger);
            trigger->ReleaseAuthored();
            continue;
        }
        if (options_.properties->Types().IsDerivedFrom(
                authored->RuntimeType(),
                Media::Animation::EventTrigger::StaticTypeId())) {
            auto* trigger =
                static_cast<Media::Animation::EventTrigger*>(authored);
            if (trigger->GetRoutedEvent().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style EventTrigger requires RoutedEvent");
            }
            // Event triggers are immutable declarations. The View creates a
            // routed-event subscription per styled element after mounting.
            continue;
        }
        if (authored->RuntimeType() == Aero::DataTrigger::StaticTypeId()) {
            auto* trigger = static_cast<Aero::DataTrigger*>(authored);
            if (!trigger->GetBinding() ||
                trigger->GetAuthoredValue().IsUnset() ||
                trigger->GetAuthoredSetters().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style DataTrigger requires Binding, Value, and Setters");
            }
            Base::Result<void> settersResolved = resolveTriggerSetters(
                *trigger, "DataTrigger");
            if (!settersResolved) return settersResolved.GetStatus();
            style.AddTrigger(*trigger);
            trigger->ReleaseAuthored();
            continue;
        }
        if (authored->RuntimeType() ==
                Aero::MultiDataTrigger::StaticTypeId()) {
            auto* trigger =
                static_cast<Aero::MultiDataTrigger*>(authored);
            if (trigger->GetConditions().Empty() ||
                trigger->GetAuthoredSetters().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style MultiDataTrigger requires Conditions and Setters");
            }
            for (const Base::Ref<Aero::Condition>& condition :
                 trigger->GetConditions()) {
                if (!condition || !condition->GetBinding() ||
                    condition->GetAuthoredValue().IsUnset()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Style MultiDataTrigger Condition requires Binding and Value");
                }
            }
            Base::Result<void> settersResolved = resolveTriggerSetters(
                *trigger, "MultiDataTrigger");
            if (!settersResolved) return settersResolved.GetStatus();
            style.AddTrigger(*trigger);
            for (const Base::Ref<Aero::Condition>& condition :
                 trigger->GetConditions()) {
                if (condition) condition->ReleaseAuthored();
            }
            continue;
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Style trigger type is not supported");
    }
    return Aero::SealStyle(
        style, *options_.properties);
}

Base::Result<void> XamlStyleSchemaFacet::EndStyleInit(
    Base::Object& object,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr) {
        return InvalidStyleXaml("Style initialization requires an extension context");
    }
    return extension->FinalizeStyle(
        static_cast<Aero::Style&>(
            object));
}

} // namespace Aero::Markup

namespace Aero::Markup {

struct UiObjectModel::State {
    explicit State(
        const UiObjectModelOptions& options) noexcept
        : style(options),
          templates(
              *options.metadata,
              *options.properties,
              options.allocator) {}

    XamlStyleSchemaFacet style;
    XamlTemplateSchemaFacet templates;
    bool registered = false;
};

UiObjectModel::UiObjectModel(
    const UiObjectModelOptions& options) noexcept
    : allocator_(options.allocator != nullptr
          ? options.allocator
          : &Base::GetDefaultAllocator()) {
    if (options.metadata == nullptr ||
        options.properties == nullptr) {
        return;
    }
    optionsValid_ = true;
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::General, state_, options));
}

UiObjectModel::~UiObjectModel() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::General, state_);
    state_ = nullptr;
}

Base::Result<void> UiObjectModel::Register(
    Schema& schema) noexcept {
    UiObjectModelTypes types;
    types.style = Aero::Style::StaticTypeId();
    types.setter = Aero::Setter::StaticTypeId();
    types.trigger = Aero::Trigger::StaticTypeId();
    types.styleProperty =
        Aero::FrameworkElement::StyleProperty;
    return Register(schema, types);
}

Base::Result<void> UiObjectModel::Register(
    Schema& schema,
    const UiObjectModelTypes& types) noexcept {
    if (!optionsValid_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML UI object model options are invalid");
    }
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML UI object model allocation failed");
    }
    if (state_->registered) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML UI object model is already registered");
    }
    Base::Result<void> registered =
        state_->style.Register(
            schema,
            types.style,
            types.setter,
            types.styleProperty,
            types.trigger);
    if (registered && types.includeTemplates) {
        registered = state_->templates.Register(schema);
    }
    if (registered) {
        state_->registered = true;
    }
    return registered;
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: TemplateSupport
// Source: src/gui/markup/TemplateSupport.cpp
// ============================================================================

// ===== TemplateSupport =====









namespace Aero::Markup {
namespace {

using namespace Aero::Controls;
using namespace Aero::Meta;
using namespace Aero::Threading;




Base::Status InvalidTemplateXaml(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

bool TemplateHasTypeFlag(
    TypeFlags value,
    TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

inline void ApplyTemplateBaseUriIfEmpty(
    FrameworkTemplate& tpl, const Base::ResourceUri* uri) noexcept {
    if (uri != nullptr && ::Aero::Controls::FrameworkTemplateState::BaseUri(tpl).Empty()) {
        ::Aero::Controls::FrameworkTemplateState::SetBaseUri(tpl, *uri);
    }
}

Aero::ResourceDictionary* ResolveTemplateResources(
    Base::Object& object,
    void*) noexcept {
    if (auto* tpl = TryCast<FrameworkTemplate>(&object)) {
        return &tpl->GetResources();
    }
    return nullptr;
}

Base::Result<Aero::ResourceKey>
ResolveTemplateImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    Meta::TypeId key = Meta::InvalidTypeId;
    if (const auto* dt = TryCast<DataTemplate>(&object)) {
        key = dt->GetDataType();
    } else if (const auto* tpl = TryCast<FrameworkTemplate>(&object)) {
        key = tpl->GetTargetType();
    }
    if (key == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Template type has no implicit resource key");
    }
    return Aero::ResourceKey::FromType(key);
}

} // namespace

} // namespace Aero::Markup

namespace Aero::Markup {

struct XamlTemplateSchemaFacet::State {
    State(
        Meta::Registry& metadata,
        DependencyPropertyRegistry& dependencyProperties,
        Base::IAllocator& programAllocator) noexcept
        : allocator(&programAllocator),
          runtime(&metadata),
          properties(&dependencyProperties) {}

    Base::IAllocator* allocator = nullptr;
    Meta::Registry* runtime = nullptr;
    DependencyPropertyRegistry* properties = nullptr;
    Schema* schema = nullptr;

    static Base::Result<void> EndTemplate(
        Base::Object& object,
        const ExtensionServices& services,
        void* context) noexcept {
        auto* self = static_cast<XamlTemplateSchemaFacet::State*>(context);
        const TypeId type = object.RuntimeType();
        const bool isControlTemplate =
            type == ControlTemplate::StaticTypeId();
        const bool isDataTemplateFamily =
            type == DataTemplate::StaticTypeId() ||
            type == HierarchicalDataTemplate::StaticTypeId();
        const bool isItemsPanelTemplate =
            type == ItemsPanelTemplate::StaticTypeId();
        if (self == nullptr || self->runtime == nullptr ||
            self->properties == nullptr ||
            (!isControlTemplate &&
             !isDataTemplateFamily &&
             !isItemsPanelTemplate) ||
            services.deferredContentOwner != &object ||
            services.deferredContent == nullptr) {
            if (isControlTemplate ||
                isDataTemplateFamily ||
                isItemsPanelTemplate) {
                const bool sealed =
                    static_cast<const FrameworkTemplate&>(object).GetIsSealed();
                if (sealed) {
                    return {};
                }
                // WPF allows an empty ControlTemplate/DataTemplate. Failing
                // EndInit here aborts the document and drops visual children.
                return {};
            }
            return InvalidTemplateXaml(
                "Template deferred-content scope is invalid");
        }
        if (isControlTemplate || isDataTemplateFamily) {
            const Meta::TypeId targetType =
                static_cast<const FrameworkTemplate&>(object).GetTargetType();
            // A keyed WPF ControlTemplate may deliberately omit TargetType.
            // Its target is inferred from the Style/Setter that consumes it,
            // so only validate an explicitly authored type here. The apply
            // path still checks that the eventual target is a Control.
            if (targetType != Meta::InvalidTypeId) {
                const Meta::TypeInfo* targetInfo =
                    self->runtime->Types().FindType(targetType);
                if (targetInfo == nullptr ||
                    TemplateHasTypeFlag(
                        targetInfo->Flags(),
                        Meta::TypeFlags::ValueType)) {
                    return InvalidTemplateXaml(
                        "Template type constraint must identify an object type");
                }
                if (isControlTemplate &&
                    !self->runtime->Types().IsDerivedFrom(
                        targetType,
                        Control::StaticTypeId())) {
                    return InvalidTemplateXaml(
                        "ControlTemplate TargetType is not a Control");
                }
            }
        }
        Base::Vector<DeferredContentEdge> edges(
            self->allocator);
        Base::Result<void> copied =
            services.deferredContent->CopyForOwner(
                object, edges);
        if (!copied) return copied.GetStatus();
        Base::Vector<DeferredBindingEdge> bindings(
            self->allocator);
        copied =
            services.deferredContent->
                CopyBindingsForOwner(
                    object, bindings);
        if (!copied) return copied.GetStatus();

        if (services.baseUri != nullptr) {
            if (isControlTemplate) {
                ApplyTemplateBaseUriIfEmpty(static_cast<ControlTemplate&>(object), services.baseUri);
            } else if (isDataTemplateFamily) {
                ApplyTemplateBaseUriIfEmpty(static_cast<DataTemplate&>(object), services.baseUri);
            } else {
                ApplyTemplateBaseUriIfEmpty(static_cast<ItemsPanelTemplate&>(object), services.baseUri);
            }
        }
        if (isDataTemplateFamily || isItemsPanelTemplate) {
            const Base::Ref<Base::Object>* authored = nullptr;
            if (isDataTemplateFamily) {
                authored =
                    &::Aero::Controls::FrameworkTemplateState::AuthoredVisualTree(static_cast<DataTemplate&>(object));
            } else {
                authored =
                    &::Aero::Controls::FrameworkTemplateState::AuthoredVisualTree(static_cast<ItemsPanelTemplate&>(object));
            }
            Base::Ref<Base::Object> visualTree =
                authored != nullptr ? *authored : Base::Ref<Base::Object>{};
            if (!visualTree) {
                for (std::uint32_t index = 0U; index < edges.Size(); ++index) {
                    if (edges[index].owner == &object && edges[index].child) {
                        visualTree = edges[index].child;
                        break;
                    }
                }
            }
            if (!visualTree) {
                // WPF allows a DataTemplate with no visual tree. HierarchicalDataTemplate
                // is a DataTemplate subtype; an empty tree must still seal.
                if (isDataTemplateFamily) {
                    auto& dataTemplate =
                        static_cast<DataTemplate&>(object);
                    Base::Result<void> sealedDt =
                        ::Aero::Controls::FrameworkTemplateState::Seal(dataTemplate);
                    if (!sealedDt) return sealedDt.GetStatus();
                    services.deferredContent->ReleaseOwner(object);
                    return {};
                }
                return InvalidTemplateXaml("Template requires a VisualTree");
            }
            auto& templateObj = static_cast<FrameworkTemplate&>(object);
            auto container = Base::MakeRefWithAllocator<XamlBytecodeContainer>(*self->allocator);
            if (!container) {
                return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
            }
            Base::Result<void> bytecoded =
                CompileDeferredTemplateToBytecode(
                    visualTree,
                    isDataTemplateFamily
                        ? &::Aero::Controls::FrameworkTemplateState::AuthoredNames(static_cast<DataTemplate&>(object))
                        : nullptr,
                    {
                        edges.Data(),
                        edges.Size()},
                    {
                        bindings.Data(),
                        bindings.Size()},
                    *self->runtime,
                    *self->properties,
                    container->program);
            if (!bytecoded) {
                return bytecoded.GetStatus();
            }

            if (isDataTemplateFamily) {
                auto authoredTriggers =
                    ::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(templateObj);
                container->program.dataTemplateTriggers.Reserve(authoredTriggers.Size());
                for (const Base::Ref<Base::Object>& triggerObj : authoredTriggers) {
                    if (triggerObj) {
                        auto retained = Base::Ref<Aero::TriggerBase>::TryFromBorrowed(
                            static_cast<Aero::TriggerBase&>(*triggerObj));
                        if (retained) {
                            Base::Result<CompiledTriggerPlan> plan = CompileTriggerPlan(
                                std::move(retained), container->program,
                                *self->runtime, *self->properties);
                            if (!plan) return plan.GetStatus();
                            container->program.dataTemplateTriggers.PushBack(std::move(plan).Value());
                        }
                    }
                }
            }
            XamlBytecodeProgram* programContext =
                &container->program;
            Base::Ref<Base::Object> programOwner =
                container;
            Base::Result<void> configured =
                ::Aero::Controls::FrameworkTemplateState::Configure(
                    templateObj,
                    &BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
            if (configured) {
                configured = ::Aero::Controls::FrameworkTemplateState::Seal(templateObj);
            }
            if (!configured) {
                return configured.GetStatus();
            }
            services.deferredContent->ReleaseOwner(
                object);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(templateObj);
            if (isDataTemplateFamily) {
                ::Aero::Controls::FrameworkTemplateState::ClearAuthoredTriggers(templateObj);
                ::Aero::Controls::FrameworkTemplateState::ClearAuthoredNames(templateObj);
            }
            return {};
        }
        auto& controlTemplate =
            static_cast<ControlTemplate&>(object);
        // BasedOn inheritance without own VisualTree: the derived template
        // shares the sealed base tree/factory and only authors incremental
        // triggers or visual states (resolved against the base blueprint).
        // Seal() prepends the base plans base-first.
        bool hasOwnTree =
            static_cast<bool>(
                ::Aero::Controls::FrameworkTemplateState::AuthoredVisualTree(
                    controlTemplate));
        if (!hasOwnTree) {
            for (std::uint32_t index = 0U; index < edges.Size(); ++index) {
                if (edges[index].owner == &object && edges[index].child) {
                    hasOwnTree = true;
                    break;
                }
            }
        }
        FrameworkTemplate* templateBase =
            ::Aero::Controls::FrameworkTemplateState::BasedOn(
                static_cast<FrameworkTemplate&>(controlTemplate));
        if (!hasOwnTree && templateBase != nullptr) {
            auto* baseControl =
                TryCast<ControlTemplate>(templateBase);
            if (baseControl == nullptr || !baseControl->GetIsSealed()) {
                return InvalidTemplateXaml(
                    "BasedOn template must be a sealed ControlTemplate");
            }
            const bool addsTriggers =
                !::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(
                    static_cast<FrameworkTemplate&>(controlTemplate)).Empty();
            const bool addsStates =
                !::Aero::Controls::FrameworkTemplateState::AuthoredVisualStateGroups(
                    controlTemplate).Empty();
            const auto baseFactory =
                ::Aero::Controls::FrameworkTemplateState::Factory(*baseControl);
            void* const baseFactoryContext =
                ::Aero::Controls::FrameworkTemplateState::FactoryContext(
                    *baseControl);
            if (addsTriggers || addsStates) {
                if (baseFactory != &BuildCompiledTemplate ||
                    baseFactoryContext == nullptr) {
                    return InvalidTemplateXaml(
                        "Inherited ControlTemplate triggers require a XAML-compiled BasedOn template");
                }
                Base::Result<void> inherited =
                    CompileInheritedControlTemplate(
                        controlTemplate,
                        *static_cast<const XamlBytecodeProgram*>(
                            baseFactoryContext),
                        *self->runtime,
                        *self->properties);
                if (!inherited) {
                    return inherited.GetStatus();
                }
            }
            if (baseFactory == nullptr) {
                return InvalidTemplateXaml(
                    "BasedOn template has no factory to inherit");
            }
            Base::Ref<Base::Object> baseOwner =
                ::Aero::Controls::FrameworkTemplateState::FactoryOwner(
                    *baseControl);
            Base::Result<void> configured =
                ::Aero::Controls::FrameworkTemplateState::ConfigureFactory(
                    controlTemplate,
                    baseFactory,
                    baseFactoryContext,
                    std::move(baseOwner));
            if (configured) {
                configured =
                    ::Aero::Controls::FrameworkTemplateState::Seal(
                        controlTemplate,
                        *self->properties);
            }
            if (!configured) {
                return configured.GetStatus();
            }
            services.deferredContent->ReleaseOwner(
                object);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(controlTemplate);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualStateGroups(controlTemplate);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredTriggers(controlTemplate);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredNames(controlTemplate);
            return {};
        }
        if (controlTemplate.GetTargetType() ==
            Meta::InvalidTypeId) {
            // WPF permits a keyed ControlTemplate to omit TargetType. The
            // consuming Style supplies the concrete control at apply time;
            // compile against the common Control contract so its authored
            // bindings and triggers remain valid until then.
            Base::Result<void> inferred =
                ::Aero::Controls::FrameworkTemplateState::SetTargetType(controlTemplate,
                    Control::StaticTypeId());
            if (!inferred) return inferred.GetStatus();
        }
        Base::Result<CompiledTemplateDefinition>
            compiled =
                CompileControlTemplateDefinition(
                    controlTemplate,
                    {
                        edges.Data(),
                        edges.Size()},
                    {
                        bindings.Data(),
                        bindings.Size()},
                    *self->runtime,
                    *self->properties);
        if (!compiled) {
            return compiled.GetStatus();
        }

        Base::Result<void> configured =
            ::Aero::Controls::FrameworkTemplateState::ConfigureFactory(controlTemplate,
                &BuildCompiledTemplate,
                compiled.Value().program,
                std::move(compiled.Value().programOwner));
        if (configured) {
            for (const TemplateBindingPlan& binding :
                 compiled.Value().
                     contentSourceBindings) {
                configured =
                    ::Aero::Controls::FrameworkTemplateState::AddTemplateBinding(controlTemplate,
                            binding.targetName.View(),
                            binding.sourceProperty,
                            binding.targetProperty);
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            for (TemplatePropertyTrigger& trigger :
                 compiled.Value().propertyTriggers) {
                configured =
                    ::Aero::Controls::FrameworkTemplateState::AddPropertyTrigger(controlTemplate,
                        std::move(trigger));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            for (Controls::VisualStateGroupPlan& group :
                 compiled.Value().visualStateGroups) {
                configured =
                    ::Aero::Controls::FrameworkTemplateState::AddVisualStateGroup(controlTemplate,
                        std::move(group));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            configured =
                ::Aero::Controls::FrameworkTemplateState::Seal(
                    controlTemplate,
                    *self->properties);
        }
        if (!configured) {
            return configured.GetStatus();
        }

        services.deferredContent->ReleaseOwner(
            object);
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(controlTemplate);
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualStateGroups(controlTemplate);
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredTriggers(controlTemplate);
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredNames(controlTemplate);
        return {};
    }

    static Base::Result<void> RegisterTemplateName(
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object,
        void* context) noexcept {
        if (context == nullptr ||
            (scopeOwner.RuntimeType() !=
                 ControlTemplate::StaticTypeId() &&
             scopeOwner.RuntimeType() !=
                 DataTemplate::StaticTypeId() &&
             scopeOwner.RuntimeType() !=
                 HierarchicalDataTemplate::StaticTypeId())) {
            return InvalidTemplateXaml(
                "Template name scope is invalid");
        }
        return ::Aero::Controls::FrameworkTemplateState::RegisterAuthoredName(
            static_cast<FrameworkTemplate&>(scopeOwner), name, object);
    }
};

XamlTemplateSchemaFacet::XamlTemplateSchemaFacet(
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::General, state_, runtime, properties, *allocator_));
}

XamlTemplateSchemaFacet::~XamlTemplateSchemaFacet() noexcept {
    if (state_ == nullptr) {
        return;
    }
    Base::FreeObject(*allocator_, Base::MemoryTag::General, state_);
    state_ = nullptr;
}

Base::Result<void> XamlTemplateSchemaFacet::Register(
    Schema& schema) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML template extension allocation failed");
    }
    if (schema.IsFrozen() ||
        state_->schema != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML template extension registration is invalid");
    }

    state_->schema = &schema;
    Base::Result<void> status =
        schema.AddType({
            ControlTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacet::State::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    if (status) {
        status = schema.AddType({
            DataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacet::State::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = schema.AddType({
            HierarchicalDataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacet::State::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = schema.AddType({
            ItemsPanelTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            nullptr,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            nullptr});
    }
    if (!status) {
        state_->schema = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: GuiSchema
// Source: src/gui/markup/GuiSchema.cpp
// ============================================================================



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
    Markup::UiObjectModel* uiObjectModel = nullptr;
    bool prepared = false;
    bool frozen = false;
    bool terminal = false;

    ~State() noexcept {
        if (uiObjectModel != nullptr) {
            Base::FreeObject(*allocator, Base::MemoryTag::General, uiObjectModel);
            uiObjectModel = nullptr;
        }
        if (schema != nullptr) {
            Base::FreeObject(*allocator, Base::MemoryTag::General, schema);
            schema = nullptr;
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

    status = Markup::RegisterBuiltinMarkupExtensions(*state_->schema);
    if (status) {
        status = Base::AllocateObject(
            *state_->allocator,
            Base::MemoryTag::General,
            state_->uiObjectModel,
            Markup::UiObjectModelOptions{
                &state_->metadata,
                &(state_->metadata).DependencyProperties(),
                &programAllocator});
        if (status) {
            status = state_->uiObjectModel->Register(*state_->schema);
        }
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
