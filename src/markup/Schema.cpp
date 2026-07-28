#include "SchemaInternal.hpp"

// Query surface is public; execution operations are reached by source-side
// friends and SchemaAccess.
#include <new>

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

Schema::Schema(
    Core::MetadataDomain& domain,
    Core::MetadataRuntime& runtime,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      domain_(&domain),
      runtime_(&runtime) {
    AERO_ASSERT(domain.IsSealed());
    AERO_ASSERT(&runtime.Domain() == &domain);
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

Base::Result<const Core::TypeInfo*> Schema::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }

    const Core::TypeInfo* descriptor =
        runtime_->Types().FindType(xamlNamespace, localName);
    if (descriptor == nullptr) return RuntimeTypeNotFound();
    return descriptor;
}

Base::Result<ResolvedMember> Schema::ResolveMember(
    Core::TypeId targetType,
    const QualifiedName& name,
    MemberSyntax syntax) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }

    const Core::TypeInfo* target =
        runtime_->Types().FindType(targetType);
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
    const Core::TypeInfo* owner =
        runtime_->Types().FindType(ownerNamespace, ownerName);
    if (owner == nullptr) return RuntimeMemberNotFound();
    return ResolvePropertyOrEventRuntime(
        targetType, owner->Id(), memberName, syntax, true);
}

Base::Result<ResolvedMember>
Schema::ResolvePropertyOrEventRuntime(
    Core::TypeId targetType,
    Core::TypeId ownerType,
    Base::StringView memberName,
    MemberSyntax syntax,
    bool ownerWasExplicit) const noexcept {
    const Core::TypeRegistry& descriptors = runtime_->Types();
    const Core::PropertyInfo* property =
        descriptors.FindProperty(ownerType, memberName, true);
    if (property != nullptr) {
        const bool attached = HasPropertyFlag(
            property->Flags(), Core::PropertyFlags::Attached);
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
        resolved.kind = Core::MemberKind::Property;
        resolved.ownerType = property->OwnerType();
        resolved.valueType = property->ValueType();
        resolved.propertyFlags = property->Flags();
        resolved.attached = attached;
        return resolved;
    }

    const Core::EventInfo* event =
        descriptors.FindEvent(ownerType, memberName, true);
    if (event != nullptr) {
        const bool attached = HasEventFlag(
            event->Flags(), Core::EventFlags::Attached);
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
        resolved.kind = Core::MemberKind::Event;
        resolved.ownerType = event->OwnerType();
        resolved.valueType = event->EventArgsType();
        resolved.eventFlags = event->Flags();
        resolved.attached = attached;
        return resolved;
    }

    return RuntimeMemberNotFound();
}

Base::Result<ResolvedMember> Schema::ResolveContentMember(
    Core::TypeId targetType) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }

    const Core::MemberId contentMember =
        runtime_->FindContentMember(targetType);
    if (contentMember == Core::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML runtime type has no content facet");
    }
    const Core::PropertyInfo* property =
        runtime_->Types().FindProperty(contentMember);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Content facet references a missing property descriptor");
    }

    ResolvedMember resolved;
    resolved.id = property->Id();
    resolved.kind = Core::MemberKind::Property;
    resolved.ownerType = property->OwnerType();
    resolved.valueType = property->ValueType();
    resolved.propertyFlags = property->Flags();
    resolved.attached = HasPropertyFlag(
        property->Flags(), Core::PropertyFlags::Attached);
    return resolved;
}

Base::Result<Base::Ref<Base::Object>> Schema::CreateObject(
    Core::TypeId type) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }
    return runtime_->CreateObject(type);
}

Base::Result<Core::DependencyObject*>
Schema::ResolvePropertyTarget(
    Base::Object& object) const noexcept {
    if (!frozen_ || runtime_ == nullptr || !runtime_->IsFrozen()) {
        return RuntimeSchemaNotReady();
    }
    Core::DependencyObject* target = nullptr;
    if (runtime_->Types().IsAssignableFrom(
            Core::TypeOf<Core::DependencyObject>(),
            object.RuntimeType())) {
        target = static_cast<Core::DependencyObject*>(&object);
    } else {
        const XamlPropertyTargetFacet* facet =
            impl_->facets.FindPropertyTarget(
                object.RuntimeType(), runtime_->Types());
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
        &static_cast<const Core::MetadataDomain&>(
            *domain_).DependencyProperties()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML target belongs to a different metadata domain");
    }
    return target;
}

Base::Result<void> Schema::SetMember(
    Base::Object& object,
    Core::TypeId objectType,
    const ResolvedMember& member,
    const Core::Value& value) const noexcept {
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
        !runtime_->Types().IsDerivedFrom(objectType, member.ownerType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML runtime member owner is incompatible with the target object");
    }

    const bool runtimeWritable =
        runtime_->CanWriteProperty(member.id);
    Base::Result<Core::ContentInfo> content =
        !runtimeWritable
        ? runtime_->GetContentInfo(member.id)
        : Base::Result<Core::ContentInfo>(
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

    const bool metadataAcceptsAnyValue =
        (static_cast<std::uint32_t>(
             member.propertyFlags) &
         static_cast<std::uint32_t>(
             Core::PropertyFlags::AnyValue)) != 0U;
    const bool acceptsAnyValue = metadataAcceptsAnyValue;
    if (!acceptsAnyValue) {
        bool compatible = value.Type() == member.valueType;
        if (value.Kind() == Core::ValueKind::Object && value.AsObject()) {
            compatible = runtime_->Types().IsDerivedFrom(
                value.Type(), member.valueType);
        }
        if (!compatible) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML runtime value type does not match the member descriptor");
        }
    }

    if (runtimeWritable) {
        return runtime_->SetProperty(object, member.id, value);
    }
    if (runtimeContentWritable) {
        if (value.Kind() != Core::ValueKind::Object ||
            value.IsNullObject() || !value.AsObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML content member requires a non-null object value");
        }
        return runtime_->WriteContent(
            object, member.id, value.AsObject());
    }
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "XAML runtime member is not writable");
}

MemberWritePolicy Schema::ResolveMemberWritePolicy(
    const ResolvedMember& member) const noexcept {
    if (runtime_ == nullptr || !runtime_->IsFrozen()) return {};
    if (runtime_->CanWriteProperty(member.id)) {
        const bool acceptsAnyValue =
            (static_cast<std::uint32_t>(
                 member.propertyFlags) &
             static_cast<std::uint32_t>(
                 Core::PropertyFlags::AnyValue)) != 0U;
        return {
            MemberWriteMode::SetOnce,
            acceptsAnyValue,
            true};
    }
    Base::Result<Core::ContentInfo> content =
        runtime_->GetContentInfo(member.id);
    if (content && content.Value().writable) {
        return {
            content.Value().kind == Core::ContentKind::Collection
                ? MemberWriteMode::Collection
                : MemberWriteMode::SetOnce,
            false,
            true};
    }
    return {};
}

} // namespace Aero::Markup
