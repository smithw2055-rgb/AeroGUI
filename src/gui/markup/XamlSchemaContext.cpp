#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
// Consolidated implementation. Keep sections ordered by dependency.


#include "gui/markup/XamlCompiledSchema.inl"
#include "gui/markup/XamlSchemaMetadata.inl"
#include "gui/markup/XamlSchemaManifest.inl"

// ===== Schema =====



#include <Aero/FrameworkElement.hpp>

// Query surface is public; execution operations are reached by source-side
// friends and SchemaPrivate.

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
constexpr Base::StringView SchemaBlendInteractivityNamespace(
    "http://schemas.microsoft.com/expression/2010/interactivity");
constexpr Base::StringView SchemaSystemNamespacePrefix(
    "clr-namespace:System");

bool SchemaMatchesClrNamespacePrefix(
    Base::StringView value,
    Base::StringView prefix) noexcept {
    return value.SizeBytes() >= prefix.SizeBytes() &&
        value.Substr(0U, prefix.SizeBytes()) == prefix;
}

bool SchemaIsExtensionsClrNamespace(Base::StringView value) noexcept {
    return SchemaMatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:AeroGUIExtensions")) ||
        SchemaMatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Aero.GUI.Extensions")) ||
        SchemaMatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:NoesisGUIExtensions")) ||
        SchemaMatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Noesis.GUI.Extensions"));
}

Base::StringView SchemaCanonicalXamlNamespace(
    Base::StringView value) noexcept {
    return value == SchemaWpfPresentationNamespace ||
            value == SchemaBehaviorsNamespace ||
            value == SchemaBlendInteractivityNamespace ||
            SchemaIsExtensionsClrNamespace(value)
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
    if (value == Base::StringView("Geometry")) {
        return Base::StringView("StreamGeometry");
    }
    if (value == Base::StringView("VisualStateTransition")) {
        return Base::StringView("VisualTransition");
    }
    if (value == Base::StringView("MonochromeBrush")) {
        return Base::StringView("MonochromeShader");
    }
    if (value == Base::StringView("ConicGradientBrush")) {
        return Base::StringView("ConicGradientShader");
    }
    if (value == Base::StringView("WavesBrush")) {
        return Base::StringView("WavesShader");
    }
    return value;
}

bool SchemaIsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    return SchemaIsExtensionsClrNamespace(xamlNamespace) &&
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
    state_ = new (stateStorage_) SchemaState();
}

Schema::~Schema() noexcept {
    if (state_ == nullptr) return;
    state_->~SchemaState();
    state_ = nullptr;
}

Base::Result<const Meta::TypeInfo*> Schema::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }

    const Meta::TypeInfo* descriptor =
        domain_->Types().FindType(
            SchemaIsSystemNamespace(xamlNamespace)
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
    if (owner == nullptr && name.NamespaceUri().Empty()) {
        owner = domain_->Types().FindType(
            Meta::AeroNamespaceUri(),
            SchemaCanonicalXamlTypeName(ownerName));
    }
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
            !attached &&
            !descriptors.IsDerivedFrom(targetType, property->OwnerType())) {
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
        const bool routed = SchemaHasEventFlag(
            event->Flags(), Meta::EventFlags::Routed);
        if (ownerWasExplicit && syntax == MemberSyntax::Attribute &&
            !attached && !routed) {
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
        resolved.attached = attached ||
            (ownerWasExplicit &&
             syntax == MemberSyntax::Attribute && routed);
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
            state_->facets.FindPropertyTarget(
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
        if (member.id == VisualStateManager::VisualStateGroupsProperty.Handle().value &&
            convertedValue.Type() == VisualStateGroup::StaticTypeId() &&
            convertedValue.AsObject()) {
            auto& target = static_cast<::Aero::DependencyObject&>(object);
            Base::Ref<VisualStateGroupCollection> valueStore = target.GetValueOr(
                VisualStateManager::VisualStateGroupsProperty,
                Base::Ref<VisualStateGroupCollection>{});
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


#include <Aero/Value.hpp>


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
        state_->facets.Freeze(domain_->Types());
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
        state_->facets.FindMarkupExtension(type);
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
    (void)services;
    Base::Result<Meta::Value> provided = extension->ProvideValue();
    if (!provided) return provided.GetStatus();
    return ProvidedValue::FromValue(std::move(provided).Value());
}

Base::Result<void> Schema::BeginInit(
    Meta::TypeId type,
    Base::Object& object) const noexcept {
    const Base::Span<const std::uint32_t> lifecycle =
        state_->facets.LifecyclePlan(type);
    for (std::uint32_t reference : lifecycle) {
        const XamlLifecycleFacet* facet =
            state_->facets.LifecycleAt(reference);
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
        state_->facets.LifecyclePlan(type);
    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            state_->facets.LifecycleAt(lifecycle[index - 1U]);
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
        state_->facets.LifecyclePlan(type);
    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            state_->facets.LifecycleAt(lifecycle[index - 1U]);
        if (facet != nullptr && facet->abortInit != nullptr) {
            facet->abortInit(object, facet->context);
        }
    }
}

bool Schema::CreatesNameScope(Meta::TypeId type) const noexcept {
    const XamlNameScopeFacet* facet = state_->facets.FindNameScope(
        type, domain_->Types());
    return facet != nullptr && facet->createsNameScope;
}

bool Schema::CreatesResourceScope(
    Meta::TypeId type) const noexcept {
    const XamlResourceScopeFacet* facet = state_->facets.FindResourceScope(
        type, domain_->Types());
    return facet != nullptr && facet->createsResourceScope;
}

bool Schema::DefersVisualContent(
    Meta::TypeId type) const noexcept {
    const XamlDeferredContentFacet* facet =
        state_->facets.FindDeferredContent(
        type, domain_->Types());
    return facet != nullptr && facet->defersVisualContent;
}

Base::Result<void> Schema::RegisterName(
    Meta::TypeId scopeType,
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object) const noexcept {
    const XamlNameScopeFacet* facet = state_->facets.FindNameScope(
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
    const XamlResourceScopeFacet* facet = state_->facets.FindResourceScope(
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
    const XamlResourceScopeFacet* facet = state_->facets.FindResourceScope(
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
        state_->facets.FindImplicitResourceKey(
            type, domain_->Types());
    if (facet == nullptr || facet->resolve == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML type has no implicit resource-key facet");
    }
    return facet->resolve(object, facet->context);
}

} // namespace Aero::Markup
