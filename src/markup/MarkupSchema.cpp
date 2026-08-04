#include "markup/MarkupPrivate.hpp"
// Consolidated implementation. Keep sections ordered by dependency.

// ===== CompiledSchema =====


// Canonical compiled-schema bridge used by Loader.

#include <Aero/Markup.hpp>

#include <cstdio>
#include <utility>

namespace Aero::Markup {
namespace {

Base::Status SchemaNodeFailure(
    Base::Status status,
    const Node& node) noexcept {
    thread_local char message[512];
    const Base::StringView localName = node.Name().LocalName();
    const ::Aero::Diagnostics::SourcePosition position = node.Source().begin;
    std::snprintf(
        message,
        sizeof(message),
        "Compiled XAML schema error at %u:%u for '%.*s': %s",
        position.line,
        position.column,
        static_cast<int>(localName.SizeBytes()),
        localName.Data(),
        status.message != nullptr ? status.message : "operation failed");
    return Base::Status::Failure(status.code, message);
}

Base::Result<SchemaTypeInfo> ResolveTypeInfo(
    const Schema& schema,
    Base::StringView xamlNamespace,
    Base::StringView localName) noexcept {
    Base::Result<const Meta::TypeInfo*> type =
        schema.ResolveType(xamlNamespace, localName);
    if (!type) return type.GetStatus();
    return SchemaTypeInfo{
        type.Value()->Id(),
        type.Value()->Kind(),
        type.Value()->Flags()};
}

Base::Result<SchemaTypeInfo> ResolveTypeInfo(
    const SchemaManifest& schema,
    Base::StringView xamlNamespace,
    Base::StringView localName) noexcept {
    return schema.ResolveType(xamlNamespace, localName);
}

template<class TSchema>
Base::Result<void> ValidateSchemaCore(
    const CompiledDocument& document,
    const TSchema& schema,
    bool bindInstructions = false) noexcept {
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML schema validation requires a valid document");
    }
    enum class FrameKind : std::uint8_t {
        Object = 0U,
        ValueObject,
        Member,
        PropertyElement,
        NullObject
    };
    struct Frame {
        FrameKind kind = FrameKind::Object;
        Meta::TypeId type = Meta::InvalidTypeId;
    };
    Base::Vector<Frame> frames;
    bool rootSeen = false;
    for (const Node& node : document.Nodes()) {
        switch (node.Kind()) {
        case NodeKind::NamespaceDeclaration:
        case NodeKind::Value:
            break;
        case NodeKind::StartObject: {
            const bool nullObject =
                node.Name().NamespaceUri() == LanguageNamespaceUri() &&
                node.Name().LocalName() == Base::StringView("Null");
            bool propertyElement = false;
            for (std::uint32_t index = 0U;
                 index < node.Name().LocalName().SizeBytes(); ++index) {
                propertyElement = propertyElement ||
                    node.Name().LocalName()[index] == '.';
            }
            if (propertyElement && !frames.Empty() &&
                frames.Back().kind == FrameKind::Object) {
                Base::Result<ResolvedMember> member = schema.ResolveMember(
                    frames.Back().type,
                    node.Name(),
                    MemberSyntax::PropertyElement);
                if (!member) {
                    return SchemaNodeFailure(member.GetStatus(), node);
                }
                if (bindInstructions) {
                    const_cast<Node&>(node).BindCompiledMember(
                        member.Value().id);
                }
                Base::Result<void> appended = frames.PushBack({
                    FrameKind::PropertyElement,
                    Meta::InvalidTypeId});
                if (!appended) return appended.GetStatus();
                break;
            }
            if (nullObject) {
                Base::Result<void> appended = frames.PushBack({
                    FrameKind::NullObject,
                    Meta::InvalidTypeId});
                if (!appended) return appended.GetStatus();
                break;
            }
            Base::Result<SchemaTypeInfo> type = ResolveTypeInfo(
                schema,
                node.Name().NamespaceUri(),
                node.Name().LocalName());
            if (!type) {
                return SchemaNodeFailure(type.GetStatus(), node);
            }
            if (bindInstructions) {
                const_cast<Node&>(node).BindCompiledType(type.Value().id);
            }
            if (!frames.Empty() &&
                frames.Back().kind == FrameKind::Object) {
                Base::Result<ResolvedMember> content =
                    schema.ResolveContentMember(frames.Back().type);
                if (!content) return content.GetStatus();
            } else if (!frames.Empty() &&
                frames.Back().kind != FrameKind::Member &&
                frames.Back().kind != FrameKind::PropertyElement) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML object nesting is invalid");
            } else if (frames.Empty()) {
                if (rootSeen) {
                    return Base::Status::Failure(
                        Base::ErrorCode::AlreadyExists,
                        "Compiled XAML contains multiple roots");
                }
                rootSeen = true;
            }
            Base::Result<void> appended = frames.PushBack({
                Meta::HasTypeFlag(
                    type.Value().flags,
                    Meta::TypeFlags::ValueType)
                    ? FrameKind::ValueObject
                    : FrameKind::Object,
                type.Value().id});
            if (!appended) return appended.GetStatus();
            break;
        }
        case NodeKind::EndObject:
            if (frames.Empty() ||
                (frames.Back().kind != FrameKind::Object &&
                 frames.Back().kind != FrameKind::ValueObject &&
                 frames.Back().kind != FrameKind::PropertyElement &&
                 frames.Back().kind != FrameKind::NullObject)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML object frame is unbalanced");
            }
            frames.PopBack();
            break;
        case NodeKind::StartMember:
            if (frames.Empty() ||
                (frames.Back().kind != FrameKind::Object &&
                 frames.Back().kind != FrameKind::ValueObject)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML member has no object owner");
            }
            if (frames.Back().kind == FrameKind::ValueObject &&
                node.Name().NamespaceUri() != LanguageNamespaceUri()) {
                if (!node.IsFromAttribute() ||
                    node.Name().LocalName() != Base::StringView("Value")) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Compiled XAML value-type member was not found");
                }
            } else if (node.Name().NamespaceUri() !=
                       LanguageNamespaceUri()) {
                Base::Result<ResolvedMember> member = schema.ResolveMember(
                    frames.Back().type,
                    node.Name(),
                    MemberSyntax::Attribute);
                if (!member) {
                    return SchemaNodeFailure(member.GetStatus(), node);
                }
                if (bindInstructions) {
                    const_cast<Node&>(node).BindCompiledMember(
                        member.Value().id);
                }
            } else if (
                (frames.Back().kind == FrameKind::ValueObject &&
                 node.Name().LocalName() != Base::StringView("Key")) ||
                (frames.Back().kind == FrameKind::Object &&
                 node.Name().LocalName() != Base::StringView("Name") &&
                 node.Name().LocalName() != Base::StringView("Key") &&
                 node.Name().LocalName() != Base::StringView("Class"))) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Compiled XAML directive is not supported");
            }
            {
                Base::Result<void> appended = frames.PushBack({
                    FrameKind::Member,
                    Meta::InvalidTypeId});
                if (!appended) return appended.GetStatus();
            }
            break;
        case NodeKind::EndMember:
            if (frames.Empty() ||
                frames.Back().kind != FrameKind::Member) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML member frame is unbalanced");
            }
            frames.PopBack();
            break;
        case NodeKind::EndOfDocument:
            if (!frames.Empty() || !rootSeen) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML document is incomplete");
            }
            return {};
        case NodeKind::None:
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML contains an empty node");
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Compiled XAML has no end-of-document node");
}

} // namespace

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const Schema& schema) noexcept {
    return Compile(reader, schema, {});
}

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const Schema& schema,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledDocument> compiled =
        Compile(reader, schema.Domain(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(schema.Domain());
    if (!identity) return identity.GetStatus();
    compiled.Value().identity_ = identity.Value();
    Base::Result<void> valid = compiled.Value().BindSchema(schema);
    if (!valid) return valid.GetStatus();
    return std::move(compiled).Value();
}

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const SchemaManifest& manifest) noexcept {
    return Compile(reader, manifest, {});
}

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const SchemaManifest& manifest,
    const Base::ResourceUri& originUri) noexcept {
    if (!manifest.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML requires a valid schema manifest");
    }
    Base::Result<CompiledDocument> compiled =
        CompileWithIdentity(reader, manifest.Identity(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<void> valid = compiled.Value().BindSchema(manifest);
    if (!valid) return valid.GetStatus();
    return std::move(compiled).Value();
}

Base::Result<void> CompiledDocument::ValidateSchema(
    const Schema& schema) const noexcept {
    if (!schema.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML validation requires a frozen runtime schema");
    }
    return ValidateSchemaCore(*this, schema);
}

Base::Result<void> CompiledDocument::BindSchema(
    const Schema& schema) noexcept {
    if (!schema.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 binding requires a frozen runtime schema");
    }
    return ValidateSchemaCore(*this, schema, true);
}

Base::Result<void> CompiledDocument::ValidateSchema(
    const SchemaManifest& manifest) const noexcept {
    if (!manifest.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML validation requires a valid schema manifest");
    }
    if (CompareCompiledCacheIdentity(identity_, manifest.Identity()) !=
        CompiledCacheCompatibility::Compatible) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML identity does not match the schema manifest");
    }
    return ValidateSchemaCore(*this, manifest);
}

Base::Result<void> CompiledDocument::BindSchema(
    const SchemaManifest& manifest) noexcept {
    if (!manifest.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 binding requires a valid schema manifest");
    }
    if (CompareCompiledCacheIdentity(identity_, manifest.Identity()) !=
        CompiledCacheCompatibility::Compatible) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 identity does not match the schema manifest");
    }
    return ValidateSchemaCore(*this, manifest, true);
}

} // namespace Aero::Markup


// ===== Metadata =====



// Markup-specific metadata declarations.

#include <Aero/Meta.hpp>
#include <Aero/Styling.hpp>




namespace Aero::Markup::Detail {
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

void AddElementVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            VisualStateGroup::StaticTypeId()) {
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

void ClearElementVisualStateGroups(
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

} // namespace

Base::Result<void> PopulateMarkupMetadata(
    Meta::Registration& context) noexcept {
    Base::Result<void> status =
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
    auto loc = Meta::Register<LocExtensionToken>(
        context,
        TypeFlags::MarkupExtension | TypeFlags::Abstract);
    loc.Property(
        LocExtensionToken::SourceProperty,
        PropertyOptions(Base::ResourceUri{}).Inherits().Changed(
            &LocExtension::OnSourceChanged));
    status = loc.Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticResourceObject>(context)
        .Property(
            StaticResourceObject::ResourceKeyProperty,
            PropertyOptions(Base::String{}))
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
            PropertyOptions(Base::Ref<VisualStateGroupCollection>{})
                .Structural())
        .ContentAccessor(
            VisualStateManager::VisualStateGroupsProperty.Id(),
            ContentKind::Collection,
            &AddElementVisualStateGroup,
            &ClearElementVisualStateGroups);
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

} // namespace Aero::Markup::Detail

// ===== FacetStore =====



#include <cstdint>

namespace Aero::Markup::Detail {
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
    return ::Aero::GuiPrivate::Detail::CompactFacetIndex::CountBefore(mask, kind);
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

} // namespace Aero::Markup::Detail


// ===== SchemaManifest =====


// Immutable compiled-schema manifest implementation.

#include <Aero/Base/Assert.hpp>

#include <new>

namespace Aero::Markup {
namespace {

constexpr std::uint32_t ManifestMagic = UINT32_C(0x48435341); // ASCH
constexpr std::uint32_t ManifestEncodingVersion = 1U;

enum class ManifestMemberKind : std::uint8_t {
    Property = 0U,
    Event
};

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

bool HasPropertyFlag(
    Meta::PropertyFlags value,
    Meta::PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool HasEventFlag(
    Meta::EventFlags value,
    Meta::EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr Base::StringView WpfPresentationNamespace(
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation");
constexpr Base::StringView BehaviorsNamespace(
    "http://schemas.microsoft.com/xaml/behaviors");
constexpr Base::StringView SystemNamespacePrefix(
    "clr-namespace:System");

Base::StringView CanonicalXamlNamespace(
    Base::StringView value) noexcept {
    constexpr Base::StringView AeroExtensionsPrefix(
        "clr-namespace:AeroGUIExtensions");
    const bool aeroExtensions = value.SizeBytes() >=
            AeroExtensionsPrefix.SizeBytes() &&
        value.Substr(0U, AeroExtensionsPrefix.SizeBytes()) ==
            AeroExtensionsPrefix;
    return value == WpfPresentationNamespace ||
            value == BehaviorsNamespace || aeroExtensions
        ? Meta::AeroNamespaceUri()
        : value;
}

bool IsSystemNamespace(
    Base::StringView value) noexcept {
    return value.SizeBytes() >=
            SystemNamespacePrefix.SizeBytes() &&
        value.Substr(0U, SystemNamespacePrefix.SizeBytes()) ==
            SystemNamespacePrefix;
}

Base::StringView CanonicalXamlTypeName(
    Base::StringView value) noexcept {
    return value == Base::StringView("HierarchicalDataTemplate")
        ? Base::StringView("DataTemplate")
        // WPF's Geometry type converter materializes a StreamGeometry for
        // textual path data. Aero exposes that concrete representation, so
        // preserve the portable <Geometry> XAML spelling as its alias.
        : value == Base::StringView("Geometry")
        ? Base::StringView("StreamGeometry")
        : value;
}

bool IsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    constexpr Base::StringView Prefix(
        "clr-namespace:AeroGUIExtensions");
    const bool namespaceMatches =
        xamlNamespace.SizeBytes() >=
            Prefix.SizeBytes() &&
        xamlNamespace.Substr(
            0U, Prefix.SizeBytes()) == Prefix;
    return namespaceMatches &&
        (ownerName == Base::StringView("Text") ||
         ownerName == Base::StringView("Path") ||
         ownerName == Base::StringView("Brush") ||
          ownerName == Base::StringView("Element") ||
          ownerName == Base::StringView("RichText"));
}

Base::Result<void> AppendU8(
    Base::Vector<std::uint8_t>& output,
    std::uint8_t value) noexcept {
    return output.PushBack(value);
}

Base::Result<void> AppendU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        Base::Result<void> appended = output.PushBack(
            static_cast<std::uint8_t>(value >> shift));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> AppendU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        Base::Result<void> appended = output.PushBack(
            static_cast<std::uint8_t>(value >> shift));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> AppendString(
    Base::Vector<std::uint8_t>& output,
    Base::StringView value) noexcept {
    Base::Result<void> result = AppendU32(output, value.SizeBytes());
    if (!result) return result.GetStatus();
    for (std::uint32_t index = 0U; index < value.SizeBytes(); ++index) {
        result = AppendU8(
            output,
            static_cast<std::uint8_t>(value[index]));
        if (!result) return result.GetStatus();
    }
    return {};
}

class Decoder {
public:
    explicit Decoder(Base::Span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    Base::Result<std::uint8_t> ReadU8() noexcept {
        if (offset_ >= bytes_.Size()) return Truncated();
        return bytes_[offset_++];
    }

    Base::Result<std::uint32_t> ReadU32() noexcept {
        if (bytes_.Size() - offset_ < 4U) return Truncated();
        std::uint32_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<std::uint64_t> ReadU64() noexcept {
        if (bytes_.Size() - offset_ < 8U) return Truncated();
        std::uint64_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<Base::String> ReadString(
        Base::IAllocator& allocator,
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        Base::Result<std::uint32_t> length = ReadU32();
        if (!length) return length.GetStatus();
        if (length.Value() > bytes_.Size() - offset_ ||
            length.Value() > maxStringBytes ||
            totalStringBytes > maxStringBytes - length.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML schema manifest string bounds are invalid");
        }
        Base::String value(&allocator);
        Base::Result<void> assigned = value.Assign(
            Base::StringView(
                reinterpret_cast<const char*>(bytes_.Data() + offset_),
                length.Value()));
        if (!assigned) return assigned.GetStatus();
        offset_ += length.Value();
        totalStringBytes += length.Value();
        return value;
    }

    bool AtEnd() const noexcept { return offset_ == bytes_.Size(); }

private:
    static Base::Status Truncated() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML schema manifest payload is truncated");
    }

    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t offset_ = 0U;
};

} // namespace

struct SchemaManifest::Impl {
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

    explicit Impl(Base::IAllocator& allocator) noexcept
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
            Base::Result<typename Base::HashMap<Meta::TypeId, std::uint32_t>::InsertResult>
                inserted = typeIndex.Insert(types[index].id, index);
            if (!inserted) return inserted.GetStatus();
            if (!inserted.Value().inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate TypeId values");
            }
        }
        for (std::uint32_t index = 0U; index < members.Size(); ++index) {
            Base::Result<typename Base::HashMap<Meta::MemberId, std::uint32_t>::InsertResult>
                inserted = memberIndex.Insert(members[index].id, index);
            if (!inserted) return inserted.GetStatus();
            if (!inserted.Value().inserted) {
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
        if (property != nullptr) {
            const Meta::PropertyFlags flags =
                static_cast<Meta::PropertyFlags>(property->flags);
            const bool attached = HasPropertyFlag(
                flags, Meta::PropertyFlags::Attached);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute && !attached) {
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
            ResolvedMember resolved;
            resolved.id = property->id;
            resolved.kind = Meta::MemberKind::Property;
            resolved.ownerType = property->ownerType;
            resolved.valueType = property->valueType;
            resolved.propertyFlags = flags;
            resolved.attached = attached;
            return resolved;
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
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute && !attached) {
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
            ResolvedMember resolved;
            resolved.id = event->id;
            resolved.kind = Meta::MemberKind::Event;
            resolved.ownerType = event->ownerType;
            resolved.valueType = event->valueType;
            resolved.eventFlags = flags;
            resolved.attached = attached;
            return resolved;
        }
        return MemberNotFound();
    }
};

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

void DestroyImpl(
    Base::IAllocator& allocator,
    SchemaManifest::Impl*& impl) noexcept {
    if (impl == nullptr) return;
    impl->~Impl();
    allocator.Deallocate(
        impl,
        sizeof(SchemaManifest::Impl),
        alignof(SchemaManifest::Impl),
        Base::MemoryTag::Markup);
    impl = nullptr;
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
    Impl* impl) noexcept
    : allocator_(&allocator), impl_(impl) {}

SchemaManifest::~SchemaManifest() noexcept {
    if (allocator_ != nullptr) DestroyImpl(*allocator_, impl_);
}

SchemaManifest::SchemaManifest(
    SchemaManifest&& other) noexcept
    : allocator_(other.allocator_), impl_(other.impl_) {
    other.allocator_ = &Base::GetDefaultAllocator();
    other.impl_ = nullptr;
}

SchemaManifest& SchemaManifest::operator=(
    SchemaManifest&& other) noexcept {
    if (this == &other) return *this;
    if (allocator_ != nullptr) DestroyImpl(*allocator_, impl_);
    allocator_ = other.allocator_;
    impl_ = other.impl_;
    other.allocator_ = &Base::GetDefaultAllocator();
    other.impl_ = nullptr;
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
    Base::Result<Impl*> created = AllocateObject<Impl>(selected);
    if (!created) return created.GetStatus();
    Impl* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(schema.Domain());
    if (!identity) {
        DestroyImpl(selected, impl);
        return identity.GetStatus();
    }
    impl->identity = identity.Value();

    const Meta::TypeRegistry& descriptors = schema.Types();
    Base::Result<void> reserved = impl->types.Reserve(descriptors.TypeCount());
    if (!reserved) {
        DestroyImpl(selected, impl);
        return reserved.GetStatus();
    }
    reserved = impl->members.Reserve(
        descriptors.PropertyCount() + descriptors.EventCount());
    if (!reserved) {
        DestroyImpl(selected, impl);
        return reserved.GetStatus();
    }

    for (const Meta::TypeInfo& type : descriptors.Types()) {
        Impl::TypeRecord record(selected);
        record.id = type.Id();
        record.baseType = type.BaseType();
        record.kind = type.Kind();
        record.flags = type.Flags();
        Base::Result<void> assigned = record.xamlNamespace.Assign(
            type.XamlNamespace());
        if (assigned) assigned = record.name.Assign(type.Name());
        if (!assigned) {
            DestroyImpl(selected, impl);
            return assigned.GetStatus();
        }
        Base::Result<ResolvedMember> content =
            schema.ResolveContentMember(type.Id());
        if (content) {
            record.contentMember = content.Value().id;
        } else if (content.GetStatus().code != Base::ErrorCode::NotFound) {
            DestroyImpl(selected, impl);
            return content.GetStatus();
        }
        Base::Result<void> appended = impl->types.PushBack(
            std::move(record));
        if (!appended) {
            DestroyImpl(selected, impl);
            return appended.GetStatus();
        }

        for (const Meta::PropertyInfo& property : type.Properties()) {
            Impl::MemberRecord member(selected);
            member.id = property.Id();
            member.kind = ManifestMemberKind::Property;
            member.ownerType = property.OwnerType();
            member.valueType = property.ValueType();
            member.flags = static_cast<std::uint32_t>(property.Flags());
            assigned = member.name.Assign(property.Name());
            if (!assigned) {
                DestroyImpl(selected, impl);
                return assigned.GetStatus();
            }
            appended = impl->members.PushBack(std::move(member));
            if (!appended) {
                DestroyImpl(selected, impl);
                return appended.GetStatus();
            }
        }

        for (const Meta::EventInfo& event : type.Events()) {
            Impl::MemberRecord member(selected);
            member.id = event.Id();
            member.kind = ManifestMemberKind::Event;
            member.ownerType = event.OwnerType();
            member.valueType = event.EventArgsType();
            member.flags = static_cast<std::uint32_t>(event.Flags());
            assigned = member.name.Assign(event.Name());
            if (!assigned) {
                DestroyImpl(selected, impl);
                return assigned.GetStatus();
            }
            appended = impl->members.PushBack(std::move(member));
            if (!appended) {
                DestroyImpl(selected, impl);
                return appended.GetStatus();
            }
        }
    }

    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyImpl(selected, impl);
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
    Decoder decoder(bytes);
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

    Base::Result<Impl*> created = AllocateObject<Impl>(selected);
    if (!created) return created.GetStatus();
    Impl* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity = ReadIdentity(decoder);
    if (!identity) {
        DestroyImpl(selected, impl);
        return identity.GetStatus();
    }
    CompiledCacheIdentity current;
    current.metadataSchemaHash = identity.Value().metadataSchemaHash;
    if (CompareCompiledCacheIdentity(identity.Value(), current) !=
        CompiledCacheCompatibility::Compatible) {
        DestroyImpl(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML schema manifest ABI is incompatible with this tool");
    }
    impl->identity = identity.Value();

    Base::Result<std::uint32_t> typeCount = decoder.ReadU32();
    if (!typeCount) {
        DestroyImpl(selected, impl);
        return typeCount.GetStatus();
    }
    Base::Result<std::uint32_t> memberCount = decoder.ReadU32();
    if (!memberCount) {
        DestroyImpl(selected, impl);
        return memberCount.GetStatus();
    }
    if (typeCount.Value() > limits.maxTypes ||
        memberCount.Value() > limits.maxMembers) {
        DestroyImpl(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML schema manifest descriptor count exceeds limits");
    }
    Base::Result<void> reserved = impl->types.Reserve(typeCount.Value());
    if (reserved) reserved = impl->members.Reserve(memberCount.Value());
    if (!reserved) {
        DestroyImpl(selected, impl);
        return reserved.GetStatus();
    }

    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U; index < typeCount.Value(); ++index) {
        Impl::TypeRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyImpl(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint64_t> baseType = decoder.ReadU64();
        if (!baseType) {
            DestroyImpl(selected, impl);
            return baseType.GetStatus();
        }
        Base::Result<std::uint32_t> kind = decoder.ReadU32();
        if (!kind) {
            DestroyImpl(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyImpl(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<std::uint64_t> content = decoder.ReadU64();
        if (!content) {
            DestroyImpl(selected, impl);
            return content.GetStatus();
        }
        Base::Result<Base::String> xamlNamespace = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!xamlNamespace) {
            DestroyImpl(selected, impl);
            return xamlNamespace.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyImpl(selected, impl);
            return name.GetStatus();
        }
        if (id.Value() == Meta::InvalidTypeId || name.Value().Empty() ||
            Meta::MakeTypeId(
                xamlNamespace.Value().View(),
                name.Value().View()) != id.Value() ||
            kind.Value() > static_cast<std::uint32_t>(Meta::MetadataTypeKind::Primitive)) {
            DestroyImpl(selected, impl);
            return InvalidManifest("XAML schema manifest type descriptor is invalid");
        }
        record.id = id.Value();
        record.baseType = baseType.Value();
        record.kind = static_cast<Meta::MetadataTypeKind>(kind.Value());
        record.flags = static_cast<Meta::TypeFlags>(flags.Value());
        record.contentMember = content.Value();
        record.xamlNamespace = std::move(xamlNamespace).Value();
        record.name = std::move(name).Value();
        Base::Result<void> appended = impl->types.PushBack(std::move(record));
        if (!appended) {
            DestroyImpl(selected, impl);
            return appended.GetStatus();
        }
    }

    for (std::uint32_t index = 0U; index < memberCount.Value(); ++index) {
        Impl::MemberRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyImpl(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint8_t> kind = decoder.ReadU8();
        if (!kind) {
            DestroyImpl(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> reservedField = decoder.ReadU32();
        if (!reservedField) {
            DestroyImpl(selected, impl);
            return reservedField.GetStatus();
        }
        Base::Result<std::uint64_t> owner = decoder.ReadU64();
        if (!owner) {
            DestroyImpl(selected, impl);
            return owner.GetStatus();
        }
        Base::Result<std::uint64_t> valueType = decoder.ReadU64();
        if (!valueType) {
            DestroyImpl(selected, impl);
            return valueType.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyImpl(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyImpl(selected, impl);
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
            DestroyImpl(selected, impl);
            return InvalidManifest("XAML schema manifest member descriptor is invalid");
        }
        record.id = id.Value();
        record.kind = static_cast<ManifestMemberKind>(kind.Value());
        record.ownerType = owner.Value();
        record.valueType = valueType.Value();
        record.flags = flags.Value();
        record.name = std::move(name).Value();
        Base::Result<void> appended = impl->members.PushBack(std::move(record));
        if (!appended) {
            DestroyImpl(selected, impl);
            return appended.GetStatus();
        }
    }

    if (!decoder.AtEnd()) {
        DestroyImpl(selected, impl);
        return InvalidManifest("XAML schema manifest has trailing bytes");
    }
    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyImpl(selected, impl);
        return indexed.GetStatus();
    }
    for (const Impl::TypeRecord& type : impl->types) {
        if (type.baseType != Meta::InvalidTypeId &&
            impl->FindType(type.baseType) == nullptr) {
            DestroyImpl(selected, impl);
            return InvalidManifest("XAML schema manifest base type is missing");
        }
        Meta::TypeId current = type.id;
        std::uint32_t depth = 0U;
        while (current != Meta::InvalidTypeId && depth <= impl->types.Size()) {
            const Impl::TypeRecord* currentType = impl->FindType(current);
            if (currentType == nullptr) break;
            current = currentType->baseType;
            ++depth;
        }
        if (current != Meta::InvalidTypeId) {
            DestroyImpl(selected, impl);
            return InvalidManifest("XAML schema manifest type hierarchy contains a cycle");
        }
        if (type.contentMember != Meta::InvalidMemberId) {
            const Impl::MemberRecord* content = impl->FindMember(type.contentMember);
            if (content == nullptr ||
                content->kind != ManifestMemberKind::Property ||
                !impl->IsDerivedFrom(type.id, content->ownerType)) {
                DestroyImpl(selected, impl);
                return InvalidManifest("XAML schema manifest content member is missing or incompatible");
            }
        }
    }
    for (const Impl::MemberRecord& member : impl->members) {
        if (impl->FindType(member.ownerType) == nullptr ||
            impl->FindType(member.valueType) == nullptr) {
            DestroyImpl(selected, impl);
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
    if (result) result = AppendIdentity(output, impl_->identity);
    if (result) result = AppendU32(output, impl_->types.Size());
    if (result) result = AppendU32(output, impl_->members.Size());
    if (!result) return result.GetStatus();

    for (const Impl::TypeRecord& type : impl_->types) {
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
    for (const Impl::MemberRecord& member : impl_->members) {
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
    return impl_ != nullptr && impl_->valid;
}

std::uint32_t SchemaManifest::TypeCount() const noexcept {
    return IsValid() ? impl_->types.Size() : 0U;
}

std::uint32_t SchemaManifest::MemberCount() const noexcept {
    return IsValid() ? impl_->members.Size() : 0U;
}

const CompiledCacheIdentity& SchemaManifest::Identity() const noexcept {
    AERO_ASSERT(IsValid());
    return impl_->identity;
}

Base::Result<SchemaTypeInfo> SchemaManifest::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const Impl::TypeRecord* type = impl_->FindType(
        IsSystemNamespace(xamlNamespace) &&
            (localName == Base::StringView("String") ||
             localName == Base::StringView("Double"))
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
    const Impl::TypeRecord* target = impl_->FindType(targetType);
    if (target == nullptr || name.LocalName().Empty()) return MemberNotFound();

    const Base::StringView localName = name.LocalName();
    std::uint32_t dot = localName.SizeBytes();
    for (std::uint32_t index = 0U; index < localName.SizeBytes(); ++index) {
        if (localName[index] != '.') continue;
        if (dot != localName.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML schema manifest member contains multiple owner separators");
        }
        dot = index;
    }

    if (dot == localName.SizeBytes()) {
        if (!name.NamespaceUri().Empty() &&
            CanonicalXamlNamespace(name.NamespaceUri()) !=
                target->xamlNamespace.View()) {
            return MemberNotFound();
        }
        return impl_->ResolvePropertyOrEvent(
            targetType, targetType, localName, syntax, false);
    }
    if (dot == 0U || dot + 1U >= localName.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML schema manifest member owner syntax is invalid");
    }
    const Base::StringView ownerName = localName.Substr(0U, dot);
    const Base::StringView memberName = localName.Substr(
        dot + 1U, localName.SizeBytes() - dot - 1U);
    const Base::StringView ownerNamespace = name.NamespaceUri().Empty()
        ? target->xamlNamespace.View() : name.NamespaceUri();
    if (IsAeroExtensionsFacade(
            ownerNamespace, ownerName)) {
        // The reference Gallery uses the legacy Element.BlendingMode
        // extension name. Element is also a real Aero extension owner
        // (PPAAOut), so normalize this one compatibility alias before
        // looking up the owner rather than letting that type shadow the
        // inherited UIElement BlendMode property.
        if (ownerName == Base::StringView("Element") &&
            memberName == Base::StringView("BlendingMode")) {
            return impl_->ResolvePropertyOrEvent(
                targetType,
                targetType,
                Base::StringView("BlendMode"),
                syntax,
                false);
        }
        // The legacy AeroGUIExtensions facade predates real attached
        // properties. Prefer a registered Aero owner (for example
        // aero:Path.TrimEnd) and retain the facade only for extension-only
        // members such as aero:Text.*.
        const Impl::TypeRecord* aeroOwner = impl_->FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(ownerName));
        if (aeroOwner != nullptr) {
            return impl_->ResolvePropertyOrEvent(
                targetType, aeroOwner->id, memberName, syntax, true);
        }
        return impl_->ResolvePropertyOrEvent(
            targetType,
            targetType,
            memberName,
            syntax,
            false);
    }
    const Impl::TypeRecord* owner = impl_->FindType(
        CanonicalXamlNamespace(ownerNamespace),
        CanonicalXamlTypeName(ownerName));
    if (owner == nullptr) return MemberNotFound();
    // WPF exposes ContextMenu through FrameworkElement property-element
    // syntax (for example Border.ContextMenu) while storage is supplied by
    // the attached ContextMenuService property.
    if (memberName == Base::StringView("ContextMenu")) {
        const Impl::TypeRecord* service = impl_->FindType(
            Meta::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return impl_->ResolvePropertyOrEvent(
                targetType, service->id, memberName, syntax, true);
        }
    }
    return impl_->ResolvePropertyOrEvent(
        targetType, owner->id, memberName, syntax, true);
}

Base::Result<ResolvedMember> SchemaManifest::ResolveContentMember(
    Meta::TypeId targetType) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const Impl::TypeRecord* type = impl_->FindType(targetType);
    if (type == nullptr) return TypeNotFound();
    if (type->contentMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML schema manifest type has no content member");
    }
    const Impl::MemberRecord* member = impl_->FindMember(type->contentMember);
    if (member == nullptr || member->kind != ManifestMemberKind::Property) {
        return InvalidManifest("XAML schema manifest content member is invalid");
    }
    const Meta::PropertyFlags flags =
        static_cast<Meta::PropertyFlags>(member->flags);
    ResolvedMember resolved;
    resolved.id = member->id;
    resolved.kind = Meta::MemberKind::Property;
    resolved.ownerType = member->ownerType;
    resolved.valueType = member->valueType;
    resolved.propertyFlags = flags;
    resolved.attached = HasPropertyFlag(flags, Meta::PropertyFlags::Attached);
    return resolved;
}

} // namespace Aero::Markup


// ===== Schema =====



#include <Aero/FrameworkElement.hpp>

// Query surface is public; execution operations are reached by source-side
// friends and SchemaPrivate.

namespace Aero::Markup {
using namespace Detail;

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

bool SchemaHasPropertyFlag(
    Meta::PropertyFlags value,
    Meta::PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool SchemaHasEventFlag(
    Meta::EventFlags value,
    Meta::EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr Base::StringView SchemaWpfPresentationNamespace(
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation");
constexpr Base::StringView SchemaBehaviorsNamespace(
    "http://schemas.microsoft.com/xaml/behaviors");
constexpr Base::StringView SchemaSystemNamespacePrefix(
    "clr-namespace:System");

Base::StringView SchemaCanonicalXamlNamespace(
    Base::StringView value) noexcept {
    constexpr Base::StringView AeroExtensionsPrefix(
        "clr-namespace:AeroGUIExtensions");
    const bool aeroExtensions = value.SizeBytes() >=
            AeroExtensionsPrefix.SizeBytes() &&
        value.Substr(0U, AeroExtensionsPrefix.SizeBytes()) ==
            AeroExtensionsPrefix;
    return value == SchemaWpfPresentationNamespace ||
            value == SchemaBehaviorsNamespace || aeroExtensions
        ? Meta::AeroNamespaceUri()
        : value;
}

bool SchemaIsSystemNamespace(
    Base::StringView value) noexcept {
    return value.SizeBytes() >=
            SchemaSystemNamespacePrefix.SizeBytes() &&
        value.Substr(0U, SchemaSystemNamespacePrefix.SizeBytes()) ==
            SchemaSystemNamespacePrefix;
}

Base::StringView SchemaCanonicalXamlTypeName(
    Base::StringView value) noexcept {
    // HierarchicalDataTemplate shares the ordinary data-template factory;
    // hierarchy-specific item expansion is applied by TreeViewItem later.
    return value == Base::StringView("HierarchicalDataTemplate")
        ? Base::StringView("DataTemplate")
        : value == Base::StringView("Geometry")
        ? Base::StringView("StreamGeometry")
        : value;
}

bool SchemaIsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    constexpr Base::StringView Prefix(
        "clr-namespace:AeroGUIExtensions");
    const bool namespaceMatches =
        xamlNamespace.SizeBytes() >=
            Prefix.SizeBytes() &&
        xamlNamespace.Substr(
            0U, Prefix.SizeBytes()) == Prefix;
    return namespaceMatches &&
        (ownerName == Base::StringView("Text") ||
         ownerName == Base::StringView("Path") ||
         ownerName == Base::StringView("Brush") ||
          ownerName == Base::StringView("Element") ||
          ownerName == Base::StringView("RichText"));
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
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl();
}

Schema::~Schema() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Markup);
    impl_ = nullptr;
}

Base::Result<const Meta::TypeInfo*> Schema::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }

    const Meta::TypeInfo* descriptor =
        domain_->Types().FindType(
            SchemaIsSystemNamespace(xamlNamespace) &&
                (localName == Base::StringView("String") ||
                 localName == Base::StringView("Double"))
                ? Meta::AeroNamespaceUri()
                : SchemaCanonicalXamlNamespace(xamlNamespace),
            SchemaCanonicalXamlTypeName(localName));
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

    const Base::StringView localName = name.LocalName();
    std::uint32_t dot = localName.SizeBytes();
    for (std::uint32_t index = 0U; index < localName.SizeBytes(); ++index) {
        if (localName[index] != '.') continue;
        if (dot != localName.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML runtime member contains multiple owner separators");
        }
        dot = index;
    }

    if (dot == localName.SizeBytes()) {
        if (!name.NamespaceUri().Empty() &&
            SchemaCanonicalXamlNamespace(name.NamespaceUri()) !=
                target->XamlNamespace()) {
            return RuntimeMemberNotFound();
        }
        Base::Result<ResolvedMember> resolved =
            ResolvePropertyOrEvent(
            targetType, targetType, localName, syntax, false);
        if (resolved || localName != Base::StringView("ToolTip")) {
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
            targetType, service->Id(), localName, syntax, false);
    }

    if (dot == 0U || dot + 1U >= localName.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML runtime member owner syntax is invalid");
    }

    const Base::StringView ownerName = localName.Substr(0U, dot);
    const Base::StringView memberName = localName.Substr(
        dot + 1U, localName.SizeBytes() - dot - 1U);
    const Base::StringView ownerNamespace = name.NamespaceUri().Empty()
        ? target->XamlNamespace() : name.NamespaceUri();
    if (SchemaIsAeroExtensionsFacade(
            ownerNamespace, ownerName)) {
        // Keep the runtime schema in lockstep with the compiled manifest:
        // Element is a real extension owner, but the legacy Gallery alias
        // Element.BlendingMode targets UIElement.BlendMode.
        if (ownerName == Base::StringView("Element") &&
            memberName == Base::StringView("BlendingMode")) {
            return ResolvePropertyOrEvent(
                targetType,
                targetType,
                Base::StringView("BlendMode"),
                syntax,
                false);
        }
        const Meta::TypeInfo* aeroOwner = domain_->Types().FindType(
            Meta::AeroNamespaceUri(),
            SchemaCanonicalXamlTypeName(ownerName));
        if (aeroOwner != nullptr) {
            return ResolvePropertyOrEvent(
                targetType, aeroOwner->Id(), memberName, syntax, true);
        }
        return ResolvePropertyOrEvent(
            targetType,
            targetType,
            memberName,
            syntax,
            false);
    }
    const Meta::TypeInfo* owner =
        domain_->Types().FindType(
            SchemaCanonicalXamlNamespace(ownerNamespace),
            SchemaCanonicalXamlTypeName(ownerName));
    if (owner == nullptr) return RuntimeMemberNotFound();
    if (memberName == Base::StringView("ContextMenu")) {
        const Meta::TypeInfo* service = domain_->Types().FindType(
            Meta::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return ResolvePropertyOrEvent(
                targetType, service->Id(), memberName, syntax, true);
        }
    }
    return ResolvePropertyOrEvent(
        targetType, owner->Id(), memberName, syntax, true);
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
        const bool attached = SchemaHasPropertyFlag(
            property->Flags(), Meta::PropertyFlags::Attached);
        if (!attached && !descriptors.IsDerivedFrom(
                targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AXB2 member owner is incompatible with the target type");
        }
        ResolvedMember resolved;
        resolved.id = property->Id();
        resolved.kind = Meta::MemberKind::Property;
        resolved.ownerType = property->OwnerType();
        resolved.valueType = property->ValueType();
        resolved.propertyFlags = property->Flags();
        resolved.attached = attached;
        return resolved;
    }
    if (const Meta::EventInfo* event =
            descriptors.FindEvent(memberId)) {
        const bool attached = SchemaHasEventFlag(
            event->Flags(), Meta::EventFlags::Attached);
        if (!attached && !descriptors.IsDerivedFrom(
                targetType, event->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AXB2 event owner is incompatible with the target type");
        }
        ResolvedMember resolved;
        resolved.id = event->Id();
        resolved.kind = Meta::MemberKind::Event;
        resolved.ownerType = event->OwnerType();
        resolved.valueType = event->EventArgsType();
        resolved.eventFlags = event->Flags();
        resolved.attached = attached;
        return resolved;
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
        SchemaHasPropertyFlag(
            property->Flags(),
            Meta::PropertyFlags::Collection)) {
        Base::String textAlias;
        Base::Result<void> aliasStatus =
            textAlias.Assign(memberName);
        if (aliasStatus) {
            aliasStatus = textAlias.Append("Text");
        }
        if (!aliasStatus) return aliasStatus.GetStatus();

        const Meta::PropertyInfo* alias =
            descriptors.FindProperty(
                ownerType, textAlias.View(), true);
        if (alias != nullptr &&
            !SchemaHasPropertyFlag(
                alias->Flags(),
                Meta::PropertyFlags::Collection)) {
            property = alias;
        }
    }
    if (property != nullptr) {
        const bool attached = SchemaHasPropertyFlag(
            property->Flags(), Meta::PropertyFlags::Attached);
        if (ownerWasExplicit && syntax == MemberSyntax::Attribute &&
            !attached) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Explicit XAML attribute owner requires an attached property");
        }
        if (ownerWasExplicit && syntax == MemberSyntax::PropertyElement &&
            !attached &&
            !descriptors.IsDerivedFrom(targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML property element owner is incompatible with the target type");
        }

        ResolvedMember resolved;
        resolved.id = property->Id();
        resolved.kind = Meta::MemberKind::Property;
        resolved.ownerType = property->OwnerType();
        resolved.valueType = property->ValueType();
        resolved.propertyFlags = property->Flags();
        resolved.attached = attached;
        return resolved;
    }

    const Meta::EventInfo* event =
        descriptors.FindEvent(ownerType, memberName, true);
    if (event != nullptr) {
        const bool attached = SchemaHasEventFlag(
            event->Flags(), Meta::EventFlags::Attached);
        if (ownerWasExplicit && syntax == MemberSyntax::Attribute &&
            !attached) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Explicit XAML attribute owner requires an attached event");
        }
        if (ownerWasExplicit && syntax == MemberSyntax::PropertyElement &&
            !attached &&
            !descriptors.IsDerivedFrom(targetType, event->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML event element owner is incompatible with the target type");
        }

        ResolvedMember resolved;
        resolved.id = event->Id();
        resolved.kind = Meta::MemberKind::Event;
        resolved.ownerType = event->OwnerType();
        resolved.valueType = event->EventArgsType();
        resolved.eventFlags = event->Flags();
        resolved.attached = attached;
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

    ResolvedMember resolved;
    resolved.id = property->Id();
    resolved.kind = Meta::MemberKind::Property;
    resolved.ownerType = property->OwnerType();
    resolved.valueType = property->ValueType();
    resolved.propertyFlags = property->Flags();
    resolved.attached = SchemaHasPropertyFlag(
        property->Flags(), Meta::PropertyFlags::Attached);
    return resolved;
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
            impl_->facets.FindPropertyTarget(
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
    if (&target->PropertyRegistry() !=
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


#include <Aero/Value.hpp>


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
        impl_->facets.Freeze(domain_->Types());
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
    if (type == Meta::TypeOf<Base::ResourceUri>() &&
        services != nullptr &&
        services->baseUri != nullptr &&
        !services->baseUri->Empty()) {
        Base::Result<Base::ResourceUri> uri =
            Base::ResourceUri::Resolve(
                *services->baseUri,
                text);
        if (!uri) return uri.GetStatus();
        return domain_->TryCreateValue(
            type,
            &uri.Value());
    }
    return domain_->TryConvertText(type, text);
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
        impl_->facets.FindMarkupExtension(type);
    if (info == nullptr ||
        !SchemaHasTypeFlag(info->Flags(), Meta::TypeFlags::MarkupExtension) ||
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
    Meta::TypeId type,
    Base::Object& object) const noexcept {
    const Base::Span<const std::uint32_t> lifecycle =
        impl_->facets.LifecyclePlan(type);
    for (std::uint32_t reference : lifecycle) {
        const XamlLifecycleFacet* facet =
            impl_->facets.LifecycleAt(reference);
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
        impl_->facets.LifecyclePlan(type);
    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            impl_->facets.LifecycleAt(lifecycle[index - 1U]);
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
        impl_->facets.LifecyclePlan(type);
    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            impl_->facets.LifecycleAt(lifecycle[index - 1U]);
        if (facet != nullptr && facet->abortInit != nullptr) {
            facet->abortInit(object, facet->context);
        }
    }
}

bool Schema::CreatesNameScope(Meta::TypeId type) const noexcept {
    const XamlNameScopeFacet* facet = impl_->facets.FindNameScope(
        type, domain_->Types());
    return facet != nullptr && facet->createsNameScope;
}

bool Schema::CreatesResourceScope(
    Meta::TypeId type) const noexcept {
    const XamlResourceScopeFacet* facet = impl_->facets.FindResourceScope(
        type, domain_->Types());
    return facet != nullptr && facet->createsResourceScope;
}

bool Schema::DefersVisualContent(
    Meta::TypeId type) const noexcept {
    const XamlDeferredContentFacet* facet =
        impl_->facets.FindDeferredContent(
        type, domain_->Types());
    return facet != nullptr && facet->defersVisualContent;
}

Base::Result<void> Schema::RegisterName(
    Meta::TypeId scopeType,
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object) const noexcept {
    const XamlNameScopeFacet* facet = impl_->facets.FindNameScope(
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
    const XamlResourceScopeFacet* facet = impl_->facets.FindResourceScope(
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
    return resources != nullptr
        ? resources->Add(key, value)
        : Base::Result<void>();
}

Aero::ResourceDictionary* Schema::ResolveResourceScope(
    Meta::TypeId scopeType,
    Base::Object& scopeOwner) const noexcept {
    const XamlResourceScopeFacet* facet = impl_->facets.FindResourceScope(
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
        impl_->facets.FindImplicitResourceKey(
            type, domain_->Types());
    if (facet == nullptr || facet->resolve == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML type has no implicit resource-key facet");
    }
    return facet->resolve(object, facet->context);
}

} // namespace Aero::Markup
