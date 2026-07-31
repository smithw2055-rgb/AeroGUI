#include "SchemaInternal.hpp"

#include <Aero/Rendering.hpp>

// Query surface is public; execution operations are reached by source-side
// friends and SchemaAccess.
#include <cstdio>
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
        ? Core::AeroNamespaceUri()
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
    // HierarchicalDataTemplate shares the ordinary data-template factory;
    // hierarchy-specific item expansion is applied by TreeViewItem later.
    return value == Base::StringView("HierarchicalDataTemplate")
        ? Base::StringView("DataTemplate")
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
          ownerName == Base::StringView("Element"));
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
        runtime_->Types().FindType(
            IsSystemNamespace(xamlNamespace) &&
                (localName == Base::StringView("String") ||
                 localName == Base::StringView("Double"))
                ? Core::AeroNamespaceUri()
                : CanonicalXamlNamespace(xamlNamespace),
            CanonicalXamlTypeName(localName));
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
            CanonicalXamlNamespace(name.NamespaceUri()) !=
                target->XamlNamespace()) {
            return RuntimeMemberNotFound();
        }
        Base::Result<ResolvedMember> resolved =
            ResolvePropertyOrEventRuntime(
            targetType, targetType, localName, syntax, false);
        if (resolved || localName != Base::StringView("ToolTip")) {
            return resolved;
        }

        // WPF exposes ToolTip as a FrameworkElement property even though the
        // storage and display policy live in ToolTipService. Retain that XAML
        // surface while keeping the existing shared service implementation.
        const Core::TypeInfo* service =
            runtime_->Types().FindType(
                Core::AeroNamespaceUri(), "ToolTipService");
        if (service == nullptr) return resolved.GetStatus();
        return ResolvePropertyOrEventRuntime(
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
    if (IsAeroExtensionsFacade(
            ownerNamespace, ownerName)) {
        // Keep the runtime schema in lockstep with the compiled manifest:
        // Element is a real extension owner, but the legacy Gallery alias
        // Element.BlendingMode targets UIElement.BlendMode.
        if (ownerName == Base::StringView("Element") &&
            memberName == Base::StringView("BlendingMode")) {
            return ResolvePropertyOrEventRuntime(
                targetType,
                targetType,
                Base::StringView("BlendMode"),
                syntax,
                false);
        }
        const Core::TypeInfo* aeroOwner = runtime_->Types().FindType(
            Core::AeroNamespaceUri(),
            CanonicalXamlTypeName(ownerName));
        if (aeroOwner != nullptr) {
            return ResolvePropertyOrEventRuntime(
                targetType, aeroOwner->Id(), memberName, syntax, true);
        }
        return ResolvePropertyOrEventRuntime(
            targetType,
            targetType,
            memberName,
            syntax,
            false);
    }
    const Core::TypeInfo* owner =
        runtime_->Types().FindType(
            CanonicalXamlNamespace(ownerNamespace),
            CanonicalXamlTypeName(ownerName));
    if (owner == nullptr) return RuntimeMemberNotFound();
    if (memberName == Base::StringView("ContextMenu")) {
        const Core::TypeInfo* service = runtime_->Types().FindType(
            Core::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return ResolvePropertyOrEventRuntime(
                targetType, service->Id(), memberName, syntax, true);
        }
    }
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
    if (property != nullptr &&
        syntax == MemberSyntax::Attribute &&
        HasPropertyFlag(
            property->Flags(),
            Core::PropertyFlags::Collection)) {
        Base::String textAlias;
        Base::Result<void> aliasStatus =
            textAlias.TryAssign(memberName);
        if (aliasStatus) {
            aliasStatus = textAlias.TryAppend("Text");
        }
        if (!aliasStatus) return aliasStatus.GetStatus();

        const Core::PropertyInfo* alias =
            descriptors.FindProperty(
                ownerType, textAlias.View(), true);
        if (alias != nullptr &&
            !HasPropertyFlag(
                alias->Flags(),
                Core::PropertyFlags::Collection)) {
            property = alias;
        }
    }
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

    Core::Value convertedValue = value;
    // FrameworkElement keeps the renderer-facing font family as text, while
    // WPF resources conventionally expose a FontFamily object. Coerce that
    // resource at the markup boundary without changing the authored XAML.
    if (member.valueType == Core::TypeOf<Base::String>() &&
        value.Kind() == Core::ValueKind::Object &&
        !value.IsNullObject() && value.AsObject() &&
        value.Type() == Media::FontFamily::StaticTypeId()) {
        Base::Result<Core::Value> encoded = Core::Value::TryFromString(
            member.valueType,
            static_cast<Media::FontFamily*>(
                value.AsObject().Get())->Source());
        if (!encoded) return encoded.GetStatus();
        convertedValue = std::move(encoded).Value();
    }
    const bool metadataAcceptsAnyValue =
        (static_cast<std::uint32_t>(
             member.propertyFlags) &
         static_cast<std::uint32_t>(
             Core::PropertyFlags::AnyValue)) != 0U;
    // Core::Value is the metadata representation of WPF's object-valued
    // member and must retain the concrete type supplied by markup (enums,
    // scalars, objects, or null), even when an older descriptor omitted the
    // redundant AnyValue flag.
    const bool acceptsAnyValue = metadataAcceptsAnyValue ||
        member.valueType == Core::TypeOf<Core::Value>();
    if (!acceptsAnyValue) {
        bool compatible = convertedValue.Type() == member.valueType;
        if (convertedValue.Kind() == Core::ValueKind::Object &&
            convertedValue.AsObject()) {
            compatible = runtime_->Types().IsDerivedFrom(
                convertedValue.Type(), member.valueType);
        }
        if (!compatible) {
            const Core::TypeInfo* owner =
                runtime_->Types().FindType(member.ownerType);
            const Core::TypeInfo* expected =
                runtime_->Types().FindType(member.valueType);
            const Core::TypeInfo* actual =
                runtime_->Types().FindType(convertedValue.Type());
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
        return runtime_->SetProperty(
            object, member.id, convertedValue);
    }
    if (runtimeContentWritable) {
        if (convertedValue.Kind() != Core::ValueKind::Object ||
            convertedValue.IsNullObject() || !convertedValue.AsObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML content member requires a non-null object value");
        }
        return runtime_->WriteContent(
            object, member.id, convertedValue.AsObject());
    }
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "XAML runtime member is not writable");
}

MemberWritePolicy Schema::ResolveMemberWritePolicy(
    const ResolvedMember& member) const noexcept {
    if (runtime_ == nullptr || !runtime_->IsFrozen()) return {};
    if (runtime_->CanWriteProperty(member.id)) {
        Base::Result<Core::ContentInfo> content =
            runtime_->GetContentInfo(member.id);
        const bool acceptsAnyValue =
            (static_cast<std::uint32_t>(
                 member.propertyFlags) &
             static_cast<std::uint32_t>(
                 Core::PropertyFlags::AnyValue)) != 0U ||
            member.valueType == Core::TypeOf<Core::Value>();
        return {
            content && content.Value().writable &&
                    content.Value().kind ==
                        Core::ContentKind::Collection
                ? MemberWriteMode::Collection
                : MemberWriteMode::SetOnce,
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
