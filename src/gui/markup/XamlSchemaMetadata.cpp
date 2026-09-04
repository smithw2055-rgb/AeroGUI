#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include <Aero/Markup/MarkupExtension.hpp>

// ===== Metadata =====



// Markup-specific metadata declarations.

#include <Aero/Meta.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/VisualStateManager.hpp>




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

void AddGroupState(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            VisualState::StaticTypeId()) {
        return;
    }
    (void)static_cast<VisualStateGroup&>(object).AddState(
        Base::Ref<VisualState>::FromBorrowed(
            *static_cast<VisualState*>(value.Get())));
}

void ClearGroupStates(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualStateGroup&>(object).ClearStates();
}

void AddGroupTransition(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            VisualTransition::StaticTypeId()) {
        return;
    }
    (void)static_cast<VisualStateGroup&>(object).AddTransition(
        Base::Ref<VisualTransition>::FromBorrowed(
            *static_cast<VisualTransition*>(value.Get())));
}

void ClearGroupTransitions(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualStateGroup&>(
        object).ClearTransitions();
}

[[maybe_unused]] void AddElementVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != VisualStateGroup::StaticTypeId()) {
        return;
    }
    auto& target = static_cast<::Aero::DependencyObject&>(object);
    Base::Ref<VisualStateGroupCollection> valueStore = target.GetValueOr(
        VisualStateManager::VisualStateGroupsProperty,
        Base::Ref<VisualStateGroupCollection>{});
    if (!valueStore) {
        Base::Result<Base::Ref<VisualStateGroupCollection>> created =
            Base::MakeRef<VisualStateGroupCollection>();
        if (!created) return;
        valueStore = std::move(created).Value();
        target.SetValue(
            VisualStateManager::VisualStateGroupsProperty,
            valueStore);
    }
    (void)valueStore->Add(
        Base::Ref<VisualStateGroup>::FromBorrowed(
            *static_cast<VisualStateGroup*>(value.Get())));
}

[[maybe_unused]] void ClearElementVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    static_cast<::Aero::DependencyObject&>(object).SetValue(
        VisualStateManager::VisualStateGroupsProperty,
        Base::Ref<VisualStateGroupCollection>{});
}

void AddStateContent(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return;
    }
    auto& state =
        static_cast<VisualState&>(object);
    if (value->RuntimeType() == Setter::StaticTypeId()) {
        (void)state.AddSetter(value);
        return;
    }
    if (value->RuntimeType() ==
        Media::Animation::Storyboard::StaticTypeId()) {
        state.SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(value.Get())));
        return;
    }
    return;
}

void ClearStateContent(
    Base::Object& object,
    void*) noexcept {
    auto& state =
        static_cast<VisualState&>(object);
    state.ClearSetters();
    state.SetStoryboard({});
}

void SetTransitionStoryboard(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            Media::Animation::Storyboard::StaticTypeId()) {
        return;
    }
    static_cast<VisualTransition&>(
        object).SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(
                    value.Get())));
}

void ClearTransitionStoryboard(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualTransition&>(object).SetStoryboard({});
}

[[maybe_unused]] void AddVisualStateGroupToCollection(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != VisualStateGroup::StaticTypeId()) return;
    auto& collection = static_cast<VisualStateGroupCollection&>(owner);
    (void)collection.Add(
        Base::Ref<VisualStateGroup>::FromBorrowed(
            *static_cast<VisualStateGroup*>(value.Get())));
}

[[maybe_unused]] void ClearVisualStateGroupCollection(
    Base::Object& owner,
    void*) noexcept {
    static_cast<VisualStateGroupCollection&>(owner).Clear();
}

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
        FrameworkPropertyMetadata(Base::ResourceUri{}).Inherits().Changed(
            &LocExtension::OnSourceChanged));
    status = loc.Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticResourceObject>(context)
        .Property(
            StaticResourceObject::ResourceKeyProperty,
            FrameworkPropertyMetadata(Base::String{}))
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<VisualStateGroupCollection>(
        context, TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();

    auto visualStateManager =
        Meta::Register<VisualStateManager>(
            context, TypeFlags::Abstract);
    visualStateManager
        .Property(
            VisualStateManager::VisualStateGroupsProperty,
            FrameworkPropertyMetadata(
                Base::Ref<VisualStateGroupCollection>{})
                .Structural());
    status = visualStateManager.Result();
    if (!status) return status.GetStatus();

    auto stateGroup =
        Meta::Register<VisualStateGroup>(context);
    stateGroup
        .Property(
            "Name",
            &VisualStateGroup::GetName,
            &VisualStateGroup::SetName)
        .Content<VisualState>(
            "States",
            ContentKind::Collection,
            &AddGroupState,
            &ClearGroupStates)
        .Collection<VisualTransition>(
            "Transitions",
            &AddGroupTransition,
            &ClearGroupTransitions)
        .Factory();
    status = stateGroup.Result();
    if (!status) return status.GetStatus();

    auto state = Meta::Register<VisualState>(context);
    state
        .Property(
            "Name",
            &VisualState::GetName,
            &VisualState::SetName)
        .Property<
            Base::Ref<Media::Animation::Storyboard>,
            &VisualState::GetStoryboard,
            &VisualState::SetStoryboard>(
            "Storyboard",
            PropertyFlags::Structural)
        .Content<Base::Object>(
            "Content",
            ContentKind::Collection,
            &AddStateContent,
            &ClearStateContent)
        .Factory();
    status = state.Result();
    if (!status) return status.GetStatus();

    auto transition =
        Meta::Register<VisualTransition>(context);
    transition
        .Property(
            "From",
            &VisualTransition::GetFrom,
            &VisualTransition::SetFrom)
        .Property(
            "To",
            &VisualTransition::GetTo,
            &VisualTransition::SetTo)
        .Property(
            "GeneratedDuration",
            &VisualTransition::GetGeneratedDuration,
            &VisualTransition::SetGeneratedDuration)
        .Property<
            Base::Ref<Media::Animation::EasingFunctionBase>,
            &VisualTransition::GetGeneratedEasingFunction,
            &VisualTransition::SetGeneratedEasingFunction>(
            "GeneratedEasingFunction",
            PropertyFlags::Structural)
        .Content<Media::Animation::Storyboard>(
            "Storyboard",
            ContentKind::Single,
            &SetTransitionStoryboard,
            &ClearTransitionStoryboard)
        .Factory();
    status = transition.Result();
    return status;
}

} // namespace Aero::Markup

// ===== FacetStore =====



#include <cstdint>

namespace Aero::Markup {
namespace {

bool HasTypeFlag(
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
        HasTypeFlag(descriptor->Flags(), Meta::TypeFlags::ValueType)) {
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
    Base::Result<DraftType*> added = drafts_.EmplaceBack();
    if (!added) return added.GetStatus();
    added.Value()->type = type;
    return added.Value();
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

    Base::Result<void> reserved = drafts_.Reserve(
        drafts_.Size() + (existing == nullptr ? 1U : 0U));
    if (!reserved) return reserved.GetStatus();
    if (addLifecycle) {
        reserved = lifecycles_.Reserve(lifecycles_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addNameScope) {
        reserved = nameScopes_.Reserve(nameScopes_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addResourceScope) {
        reserved = resourceScopes_.Reserve(resourceScopes_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addDeferredContent) {
        reserved = deferredContents_.Reserve(deferredContents_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addImplicitResourceKey) {
        reserved = implicitResourceKeys_.Reserve(
            implicitResourceKeys_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addPropertyTarget) {
        reserved = propertyTargets_.Reserve(propertyTargets_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
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

#define AERO_ADD_XAML_FACET(                                              \
    FacetType, Column, KindValue, InvalidExpression, InvalidMessage,       \
    DuplicateMessage)                                                     \
    if (frozen_) return FrozenStatus();                                   \
    if (facet.abiVersion != XamlFacetAbiVersion) {                        \
        return Base::Status::Failure(                                     \
            Base::ErrorCode::Unsupported,                                 \
            "XAML capability facet ABI version is incompatible");       \
    }                                                                     \
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);\
    if (!valid) return valid.GetStatus();                                  \
    if (InvalidExpression) return InvalidFacet(InvalidMessage);           \
    DraftType* existing = FindDraft(facet.type);                          \
    if (existing != nullptr &&                                            \
        existing->facets[static_cast<std::uint8_t>(KindValue)] !=         \
            InvalidFacetIndex) {                                          \
        return DuplicateFacet(DuplicateMessage);                          \
    }                                                                     \
    Base::Result<void> reserved = drafts_.Reserve(                     \
        drafts_.Size() + (existing == nullptr ? 1U : 0U));                \
    if (!reserved) return reserved.GetStatus();                           \
    reserved = Column.Reserve(Column.Size() + 1U);                     \
    if (!reserved) return reserved.GetStatus();                           \
    Base::Result<DraftType*> ensured = EnsureType(facet.type);            \
    if (!ensured) return ensured.GetStatus();                             \
    ensured.Value()->facets[static_cast<std::uint8_t>(KindValue)] =       \
        Column.Size();                                                     \
    return Column.PushBack(facet)

Base::Result<void> XamlFacets::Add(
    const XamlLifecycleFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlLifecycleFacet, lifecycles_, FacetKind::Lifecycle,
        facet.beginInit == nullptr && facet.endInit == nullptr &&
            facet.abortInit == nullptr &&
            facet.endInitWithServices == nullptr,
        "XAML lifecycle facet is invalid",
        "XAML lifecycle facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlNameScopeFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlNameScopeFacet, nameScopes_, FacetKind::NameScope,
        !facet.createsNameScope && facet.registerName == nullptr,
        "XAML name-scope facet is invalid",
        "XAML name-scope facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlResourceScopeFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlResourceScopeFacet, resourceScopes_, FacetKind::ResourceScope,
        !facet.createsResourceScope && facet.addResource == nullptr &&
            facet.resolveResourceScope == nullptr,
        "XAML resource-scope facet is invalid",
        "XAML resource-scope facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlDeferredContentFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlDeferredContentFacet, deferredContents_,
        FacetKind::DeferredContent,
        !facet.defersVisualContent,
        "XAML deferred-content facet is invalid",
        "XAML deferred-content facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlImplicitResourceKeyFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlImplicitResourceKeyFacet, implicitResourceKeys_,
        FacetKind::ImplicitResourceKey,
        facet.resolve == nullptr,
        "XAML implicit-resource-key facet is invalid",
        "XAML implicit-resource-key facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlPropertyTargetFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlPropertyTargetFacet, propertyTargets_, FacetKind::PropertyTarget,
        facet.resolve == nullptr,
        "XAML property-target facet is invalid",
        "XAML property-target facet is already registered");
}

#undef AERO_ADD_XAML_FACET

Base::Result<void> XamlFacets::Add(
    const XamlMarkupExtensionFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    const Meta::TypeInfo* type = descriptors.FindType(facet.type);
    if (facet.abiVersion != XamlFacetAbiVersion ||
        type == nullptr || facet.provideValue == nullptr ||
        !HasTypeFlag(type->Flags(), Meta::TypeFlags::MarkupExtension)) {
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
    Base::Result<void> reserved = drafts_.Reserve(
        drafts_.Size() + (existing == nullptr ? 1U : 0U));
    if (!reserved) return reserved.GetStatus();
    reserved = markupExtensions_.Reserve(markupExtensions_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    Base::Result<DraftType*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    ensured.Value()->facets[static_cast<std::uint8_t>(
        FacetKind::MarkupExtension)] = markupExtensions_.Size();
    return markupExtensions_.PushBack(facet);
}

Base::Result<void> XamlFacets::BuildLifecyclePlans(
    const Meta::TypeRegistry& descriptors) noexcept {
    Base::Vector<Meta::TypeId> ancestry;
    Base::Result<void> reserved = ancestry.Reserve(descriptors.TypeCount());
    if (!reserved) return reserved.GetStatus();

    for (XamlTypePlan& plan : plans_) {
        plan.firstLifecycleRef = lifecycleRefs_.Size();
        ancestry.Clear();
        Meta::TypeId current = plan.type;
        std::uint32_t depth = 0U;
        while (current != Meta::InvalidTypeId &&
               depth <= descriptors.TypeCount()) {
            Base::Result<void> added = ancestry.PushBack(current);
            if (!added) return added.GetStatus();
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
            Base::Result<void> added = lifecycleRefs_.PushBack(facet);
            if (!added) return added.GetStatus();
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

    Base::Result<void> reserved = plans_.Reserve(descriptors.TypeCount());
    if (!reserved) return reserved.GetStatus();
    reserved = index_.Reserve(descriptors.TypeCount());
    if (!reserved) return reserved.GetStatus();
    reserved = facetRefs_.Reserve(
        lifecycles_.Size() + nameScopes_.Size() + resourceScopes_.Size() +
        deferredContents_.Size() + implicitResourceKeys_.Size() +
        propertyTargets_.Size() + markupExtensions_.Size());
    if (!reserved) return reserved.GetStatus();

    for (const Meta::TypeInfo& descriptor : descriptors.Types()) {
        XamlTypePlan plan;
        plan.type = descriptor.Id();
        plan.firstFacetRef = facetRefs_.Size();
        const DraftType* draft = FindDraft(plan.type);
        if (draft != nullptr) {
            for (std::uint8_t kind = 0U;
                 kind < static_cast<std::uint8_t>(FacetKind::Count);
                 ++kind) {
                const std::uint32_t facet = draft->facets[kind];
                if (facet == InvalidFacetIndex) continue;
                Base::Result<void> added = facetRefs_.PushBack(facet);
                if (!added) return added.GetStatus();
                plan.facetMask |= static_cast<FacetMask>(1U << kind);
                ++plan.facetCount;
            }
        }
        const std::uint32_t position = plans_.Size();
        Base::Result<void> added = plans_.PushBack(plan);
        if (!added) return added.GetStatus();
        Base::Result<FacetIndex::InsertResult> inserted =
            index_.Insert(plan.type, position);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) {
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
    return FindByPolicy<XamlNameScopeFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindNameScopeExact(current);
        });
}

const XamlResourceScopeFacet* XamlFacets::FindResourceScope(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlResourceScopeFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindResourceScopeExact(current);
        });
}

const XamlDeferredContentFacet* XamlFacets::FindDeferredContent(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
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
    return FindByPolicy<XamlImplicitResourceKeyFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindImplicitResourceKeyExact(current);
        });
}

const XamlPropertyTargetFacet* XamlFacets::FindPropertyTarget(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
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


