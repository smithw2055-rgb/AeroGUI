#include <Aero/Markup/Schema/XamlSchemaContext.hpp>

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
    : domain_(&domain),
      runtime_(&runtime),
      xamlFacets_() {
    AERO_ASSERT(domain.IsSealed());
    AERO_ASSERT(&runtime.Domain() == &domain);
}

Base::Result<const Core::MetadataTypeDescriptor*> XamlSchemaContext::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }

    const Core::MetadataTypeDescriptor* descriptor =
        runtime_->Descriptors().FindType(xamlNamespace, localName);
    if (descriptor == nullptr) return RuntimeTypeNotFound();
    return descriptor;
}

Base::Result<XamlResolvedMember> XamlSchemaContext::ResolveMember(
    Core::TypeId targetType,
    const XamlQualifiedName& name,
    XamlMemberSyntax syntax) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }

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
        return resolved;
    }

    return RuntimeMemberNotFound();
}

Base::Result<XamlResolvedMember> XamlSchemaContext::ResolveContentMember(
    Core::TypeId targetType) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }

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
    return resolved;
}

Base::Result<Base::Ref<Base::Object>> XamlSchemaContext::CreateObject(
    Core::TypeId type) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }
    return runtime_->CreateObject(type);
}

Base::Result<Core::DependencyObject*>
XamlSchemaContext::ResolvePropertyTarget(
    Base::Object& object) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }
    Core::DependencyObject* target = nullptr;
    if (runtime_->Descriptors().IsAssignableFrom(
            Core::TypeOf<Core::DependencyObject>(),
            object.RuntimeType())) {
        target = static_cast<Core::DependencyObject*>(&object);
    } else {
        const XamlPropertyTargetFacet* facet =
            xamlFacets_.FindPropertyTarget(
                object.RuntimeType(), runtime_->Descriptors());
        if (facet != nullptr && facet->resolve != nullptr) {
            target = facet->resolve(object, facet->context);
        }
    }
    if (target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML target does not support dependency properties");
    }
    if (&target->PropertyRegistry() != &domain_->DependencyProperties()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML target belongs to a different metadata domain");
    }
    return target;
}

Base::Result<void> XamlSchemaContext::SetMember(
    Base::Object& object,
    Core::TypeId objectType,
    const XamlResolvedMember& member,
    const XamlValue& value,
    const XamlServiceProvider* services) const noexcept {
    if (!frozen_ || runtime_ == nullptr ||
        !runtime_->IsFrozen() || !member.IsValid()) {
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
    const Core::ContentFacet* content = adapter == nullptr && !runtimeWritable
        ? runtime_->Facets().FindContentByMember(member.id) : nullptr;
    const bool runtimeContentWritable = content != nullptr &&
        !Core::HasContentFlag(
            content->flags, Core::ContentFlags::Visual);
    const XamlMemberProviderRegistration* provider =
        adapter == nullptr && !runtimeWritable && !runtimeContentWritable
            ? FindMemberProvider(member) : nullptr;
    if ((adapter == nullptr ||
         (adapter->set == nullptr && adapter->setWithServices == nullptr)) &&
         !runtimeWritable && !runtimeContentWritable && provider == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML runtime member has no writable facet or adapter");
    }

    const bool acceptsAnyValue = adapter != nullptr
        ? adapter->acceptsAnyValue
        : ((runtimeWritable || runtimeContentWritable)
            ? false : provider->acceptsAnyValue);
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
    if (runtimeContentWritable) {
        if (content->write == nullptr ||
            value.Kind() != XamlValueKind::Object ||
            value.IsNullObject() || !value.AsObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML content member requires a non-null object value");
        }
        return content->write(
            object, value.AsObject(), content->context);
    }
    if (services == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML runtime member provider requires service context");
    }
    return provider->set(object, value, *services, provider->context);
}

XamlMemberWritePolicy XamlSchemaContext::ResolveMemberWritePolicy(
    const XamlResolvedMember& member) const noexcept {
    if (runtime_ == nullptr || !runtime_->IsFrozen()) return {};
    const XamlMemberAdapterRegistration* adapter = FindMemberAdapter(member.id);
    if (adapter != nullptr &&
        (adapter->set != nullptr || adapter->setWithServices != nullptr)) {
        return {adapter->mode, adapter->acceptsAnyValue, true};
    }
    if (runtime_->Facets().FindPropertyAccessor(member.id) != nullptr) {
        return {XamlMemberWriteMode::SetOnce, false, true};
    }
    const Core::ContentFacet* content =
        runtime_->Facets().FindContentByMember(member.id);
    if (content != nullptr && content->write != nullptr) {
        return {
            content->kind == Core::ContentKind::Collection
                ? XamlMemberWriteMode::Collection
                : XamlMemberWriteMode::SetOnce,
            false,
            true};
    }
    const XamlMemberProviderRegistration* provider = FindMemberProvider(member);
    if (provider != nullptr) {
        return {provider->mode, provider->acceptsAnyValue, true};
    }
    return {};
}

} // namespace Aero::Markup
