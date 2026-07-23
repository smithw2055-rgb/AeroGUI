#include <Aero/Markup/XamlSchemaContext.hpp>

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

bool HasPropertyFlag(
    Core::PropertyFlags value,
    Core::PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool HasEventFlag(
    Core::EventFlags value,
    Core::EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

} // namespace

XamlSchemaContext::XamlSchemaContext(
    Core::MetadataDomain& domain,
    Core::MetadataRuntime& runtime) noexcept
    : types_(&domain.Types()),
      domain_(&domain),
      runtime_(&runtime),
      memberAccessor_(domain.Types()),
      scalarTypes_(),
      textConverters_(),
      memberAdapters_(),
      memberProviders_(),
      typeAdapters_(),
      markupExtensions_() {
    Base::Result<void> bound = memberAccessor_.UseRuntime(runtime);
    AERO_ASSERT(bound);
    static_cast<void>(bound);
}

Base::Result<const Core::TypeInfo*> XamlSchemaContext::ResolveTypeRuntime(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (runtime_ == nullptr) return ResolveType(xamlNamespace, localName);
    if (!frozen_ || !runtime_->IsFrozen()) return RuntimeSchemaNotReady();

    const Core::MetadataTypeDescriptor* descriptor =
        runtime_->Descriptors().FindType(xamlNamespace, localName);
    if (descriptor == nullptr) return RuntimeTypeNotFound();
    const Core::TypeInfo* registrationView = types_->FindType(descriptor->Id());
    if (registrationView == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Descriptor type has no matching registration view");
    }
    return registrationView;
}

Base::Result<XamlResolvedMember> XamlSchemaContext::ResolveMemberRuntime(
    Core::TypeId targetType,
    const XamlQualifiedName& name,
    XamlMemberSyntax syntax) const noexcept {
    if (runtime_ == nullptr) return ResolveMember(targetType, name, syntax);
    if (!frozen_ || !runtime_->IsFrozen()) return RuntimeSchemaNotReady();

    const Core::MetadataTypeDescriptor* target =
        runtime_->Descriptors().FindType(targetType);
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
            name.NamespaceUri() != target->XamlNamespace()) {
            return RuntimeMemberNotFound();
        }
        return ResolvePropertyOrEventRuntime(
            targetType, targetType, localName, syntax, false);
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
    const Core::MetadataTypeDescriptor* owner =
        runtime_->Descriptors().FindType(ownerNamespace, ownerName);
    if (owner == nullptr) return RuntimeMemberNotFound();
    return ResolvePropertyOrEventRuntime(
        targetType, owner->Id(), memberName, syntax, true);
}

Base::Result<XamlResolvedMember>
XamlSchemaContext::ResolvePropertyOrEventRuntime(
    Core::TypeId targetType,
    Core::TypeId ownerType,
    Base::StringView memberName,
    XamlMemberSyntax syntax,
    bool ownerWasExplicit) const noexcept {
    const Core::MetadataDescriptorStore& descriptors = runtime_->Descriptors();
    const Core::MetadataPropertyDescriptor* property =
        descriptors.FindProperty(ownerType, memberName, true);
    if (property != nullptr) {
        const bool attached = HasPropertyFlag(
            property->Flags(), Core::PropertyFlags::Attached);
        if (ownerWasExplicit && syntax == XamlMemberSyntax::Attribute &&
            !attached) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Explicit XAML attribute owner requires an attached property");
        }
        if (ownerWasExplicit && syntax == XamlMemberSyntax::PropertyElement &&
            !attached &&
            !descriptors.IsDerivedFrom(targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML property element owner is incompatible with the target type");
        }

        XamlResolvedMember resolved;
        resolved.id = property->Id();
        resolved.kind = Core::MemberKind::Property;
        resolved.ownerType = property->OwnerType();
        resolved.valueType = property->ValueType();
        resolved.propertyFlags = property->Flags();
        resolved.attached = attached;
        resolved.propertyDescriptor = property;
        return resolved;
    }

    const Core::MetadataEventDescriptor* event =
        descriptors.FindEvent(ownerType, memberName, true);
    if (event != nullptr) {
        const bool attached = HasEventFlag(
            event->Flags(), Core::EventFlags::Attached);
        if (ownerWasExplicit && syntax == XamlMemberSyntax::Attribute &&
            !attached) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Explicit XAML attribute owner requires an attached event");
        }
        if (ownerWasExplicit && syntax == XamlMemberSyntax::PropertyElement &&
            !attached &&
            !descriptors.IsDerivedFrom(targetType, event->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML event element owner is incompatible with the target type");
        }

        XamlResolvedMember resolved;
        resolved.id = event->Id();
        resolved.kind = Core::MemberKind::Event;
        resolved.ownerType = event->OwnerType();
        resolved.valueType = event->EventArgsType();
        resolved.eventFlags = event->Flags();
        resolved.attached = attached;
        resolved.eventDescriptor = event;
        return resolved;
    }

    return RuntimeMemberNotFound();
}

Base::Result<XamlResolvedMember> XamlSchemaContext::ResolveContentMemberRuntime(
    Core::TypeId targetType) const noexcept {
    if (runtime_ == nullptr) return ResolveContentMember(targetType);
    if (!frozen_ || !runtime_->IsFrozen()) return RuntimeSchemaNotReady();

    const Core::MemberId contentMember =
        runtime_->Facets().FindContentMember(targetType);
    if (contentMember == Core::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML runtime type has no content facet");
    }
    const Core::MetadataPropertyDescriptor* property =
        runtime_->Descriptors().FindProperty(contentMember);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Content facet references a missing property descriptor");
    }

    XamlResolvedMember resolved;
    resolved.id = property->Id();
    resolved.kind = Core::MemberKind::Property;
    resolved.ownerType = property->OwnerType();
    resolved.valueType = property->ValueType();
    resolved.propertyFlags = property->Flags();
    resolved.attached = HasPropertyFlag(
        property->Flags(), Core::PropertyFlags::Attached);
    resolved.propertyDescriptor = property;
    return resolved;
}

Base::Result<Base::Ref<Base::Object>> XamlSchemaContext::CreateObjectRuntime(
    Core::TypeId type) const noexcept {
    if (runtime_ == nullptr) return CreateObject(type);
    if (!frozen_ || !runtime_->IsFrozen()) return RuntimeSchemaNotReady();
    return runtime_->CreateObject(type);
}

Base::Result<XamlValue> XamlSchemaContext::ConvertTextRuntime(
    Core::TypeId type,
    Base::StringView text) const noexcept {
    if (runtime_ == nullptr) return ConvertText(type, text);
    if (!frozen_ || !runtime_->IsFrozen()) return RuntimeSchemaNotReady();

    Base::Result<Core::Value> converted = runtime_->TryConvertText(type, text);
    if (converted) return converted;
    if (converted.GetStatus().code != Base::ErrorCode::NotFound &&
        converted.GetStatus().code != Base::ErrorCode::Unsupported) {
        return converted.GetStatus();
    }
    // XAML-local scalar and markup converters remain schema facets. The legacy
    // method is used only after the sealed Core text facet reports no converter.
    return ConvertText(type, text);
}

Base::Result<void> XamlSchemaContext::SetMemberRuntime(
    Base::Object& object,
    Core::TypeId objectType,
    const XamlResolvedMember& member,
    const XamlValue& value,
    const XamlServiceProvider* services) const noexcept {
    if (runtime_ == nullptr) {
        return SetMember(object, objectType, member, value, services);
    }
    if (!frozen_ || !runtime_->IsFrozen() || !member.IsValid()) {
        return RuntimeSchemaNotReady();
    }
    if (member.kind != Core::MemberKind::Property) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML runtime event assignment requires an event adapter");
    }
    if (!member.attached &&
        !runtime_->Descriptors().IsDerivedFrom(objectType, member.ownerType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML runtime member owner is incompatible with the target object");
    }

    const XamlMemberAdapterRegistration* adapter = FindMemberAdapter(member.id);
    const bool runtimeWritable = adapter == nullptr &&
        runtime_->Facets().FindPropertyAccessor(member.id) != nullptr;
    const XamlMemberProviderRegistration* provider =
        adapter == nullptr && !runtimeWritable
            ? FindMemberProvider(member) : nullptr;
    if ((adapter == nullptr ||
         (adapter->set == nullptr && adapter->setWithServices == nullptr)) &&
        !runtimeWritable && provider == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML runtime member has no writable facet or adapter");
    }

    const bool acceptsAnyValue = adapter != nullptr
        ? adapter->acceptsAnyValue
        : (runtimeWritable ? false : provider->acceptsAnyValue);
    if (!acceptsAnyValue) {
        bool compatible = value.Type() == member.valueType;
        if (value.Kind() == XamlValueKind::Object && value.AsObject()) {
            compatible = runtime_->Descriptors().IsDerivedFrom(
                value.Type(), member.valueType);
        }
        if (!compatible) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML runtime value type does not match the member descriptor");
        }
    }

    if (adapter != nullptr && adapter->setWithServices != nullptr) {
        if (services == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML runtime member adapter requires service context");
        }
        return adapter->setWithServices(
            object, value, *services, adapter->context);
    }
    if (adapter != nullptr) {
        return adapter->set(object, value, adapter->context);
    }
    if (runtimeWritable) {
        return runtime_->SetProperty(object, member.id, value);
    }
    if (services == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML runtime member provider requires service context");
    }
    return provider->set(object, value, *services, provider->context);
}

XamlMemberWritePolicy XamlSchemaContext::ResolveMemberWritePolicyRuntime(
    const XamlResolvedMember& member) const noexcept {
    if (runtime_ == nullptr) return ResolveMemberWritePolicy(member);
    const XamlMemberAdapterRegistration* adapter = FindMemberAdapter(member.id);
    if (adapter != nullptr &&
        (adapter->set != nullptr || adapter->setWithServices != nullptr)) {
        return {adapter->mode, adapter->acceptsAnyValue, true};
    }
    if (runtime_->Facets().FindPropertyAccessor(member.id) != nullptr) {
        return {XamlMemberWriteMode::SetOnce, false, true};
    }
    const XamlMemberProviderRegistration* provider = FindMemberProvider(member);
    if (provider != nullptr) {
        return {provider->mode, provider->acceptsAnyValue, true};
    }
    return {};
}

} // namespace Aero::Markup
