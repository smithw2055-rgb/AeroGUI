#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/ObjectTree.hpp>

#include <utility>

namespace Aero::Core {
namespace {

Base::StringView PropertyName(Base::StringView declarationName) noexcept {
    constexpr Base::StringView suffix("Property");
    if (declarationName.SizeBytes() <= suffix.SizeBytes()) {
        return declarationName;
    }
    const std::uint32_t offset =
        declarationName.SizeBytes() - suffix.SizeBytes();
    for (std::uint32_t index = 0U; index < suffix.SizeBytes(); ++index) {
        if (declarationName[offset + index] != suffix[index]) {
            return declarationName;
        }
    }
    return declarationName.Substr(0U, offset);
}

Base::StringView EventName(Base::StringView declarationName) noexcept {
    constexpr Base::StringView suffix("Event");
    if (declarationName.SizeBytes() <= suffix.SizeBytes()) {
        return declarationName;
    }
    const std::uint32_t offset =
        declarationName.SizeBytes() - suffix.SizeBytes();
    for (std::uint32_t index = 0U; index < suffix.SizeBytes(); ++index) {
        if (declarationName[offset + index] != suffix[index]) {
            return declarationName;
        }
    }
    return declarationName.Substr(0U, offset);
}

} // namespace

MetaRegistrationBuilder::MetaRegistrationBuilder(
    MetaRegistrationContext& context,
    TypeId ownerType,
    Base::StringView xamlNamespace,
    Base::StringView name,
    TypeId baseType,
    TypeFlags flags) noexcept
    : context_(&context),
      ownerType_(ownerType),
      xamlNamespace_(xamlNamespace),
      name_(name),
      baseType_(baseType),
      flags_(flags) {}

Base::Result<void> MetaRegistrationBuilder::Begin() noexcept {
    if (begun_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata registration builder was already started");
    }
    begun_ = true;
    Base::Result<TypeId> registered = context_->Types().TryRegisterType(
        {xamlNamespace_, name_, baseType_, flags_, nullptr});
    if (!registered) {
        status_ = registered.GetStatus();
        return status_;
    }
    if (registered.Value() != ownerType_) {
        status_ = Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Declared metadata TypeId does not match registry TypeId");
        return status_;
    }
    return {};
}

Base::Result<void> MetaRegistrationBuilder::Finish() const noexcept {
    if (!begun_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata registration builder was not started");
    }
    return status_.IsOk() ? Base::Result<void>()
                          : Base::Result<void>(status_);
}

void MetaRegistrationBuilder::Record(Base::Result<void> result) noexcept {
    if (status_.IsOk() && !result) {
        status_ = result.GetStatus();
    }
}

void MetaRegistrationBuilder::RegisterDependencyProperty(
    DependencyPropertyHandle declaredHandle,
    Base::StringView declarationName,
    TypeId valueType,
    Value defaultValue,
    PropertyMetadataFlags metadataFlags,
    DependencyPropertyFlags propertyFlags,
    ValidateValueCallback validate,
    CoerceValueCallback coerce) noexcept {
    if (!status_.IsOk()) return;
    const Base::StringView name = PropertyName(declarationName);
    if (name.Empty() || name == declarationName) {
        status_ = Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property member must end with Property");
        return;
    }
    const DependencyPropertyHandle expected{
        MakeMemberId(ownerType_, MemberKind::Property, name)};
    if (declaredHandle != expected) {
        status_ = Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Declared dependency property handle does not match owner and name");
        return;
    }
    DependencyPropertyRegistration registration;
    registration.name = name;
    registration.ownerType = ownerType_;
    registration.valueType = valueType;
    registration.flags = propertyFlags;
    registration.metadata.defaultValue = std::move(defaultValue);
    registration.metadata.flags = metadataFlags;
    registration.metadata.validate = validate;
    registration.metadata.coerce = coerce;
    Base::Result<DependencyPropertyRegistrationResult> result =
        context_->DependencyProperties().TryRegister(registration);
    if (!result) {
        status_ = result.GetStatus();
    } else if (result.Value().property != declaredHandle) {
        status_ = Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Dependency property registry returned a different handle");
    }
}

void MetaRegistrationBuilder::DependencyProperty(
    DependencyPropertyHandle declaredHandle,
    Base::StringView declarationName,
    TypeId valueType,
    Value defaultValue,
    PropertyMetadataFlags metadataFlags,
    ValidateValueCallback validate,
    CoerceValueCallback coerce) noexcept {
    RegisterDependencyProperty(declaredHandle, declarationName, valueType,
        std::move(defaultValue), metadataFlags, DependencyPropertyFlags::None,
        validate, coerce);
}

void MetaRegistrationBuilder::AttachedDependencyProperty(
    DependencyPropertyHandle declaredHandle,
    Base::StringView declarationName,
    TypeId valueType,
    Value defaultValue,
    PropertyMetadataFlags metadataFlags,
    ValidateValueCallback validate,
    CoerceValueCallback coerce) noexcept {
    RegisterDependencyProperty(declaredHandle, declarationName, valueType,
        std::move(defaultValue), metadataFlags,
        DependencyPropertyFlags::Attached, validate, coerce);
}

void MetaRegistrationBuilder::Property(
    const PropertyRegistration& registration) noexcept {
    if (!status_.IsOk()) return;
    Base::Result<MemberId> result =
        context_->Types().TryRegisterProperty(ownerType_, registration);
    if (!result) status_ = result.GetStatus();
}

void MetaRegistrationBuilder::Method(
    const MethodRegistration& registration) noexcept {
    if (!status_.IsOk()) return;
    Base::Result<MemberId> result =
        context_->Types().TryRegisterMethod(ownerType_, registration);
    if (!result) status_ = result.GetStatus();
}

void MetaRegistrationBuilder::Event(
    const EventRegistration& registration) noexcept {
    if (!status_.IsOk()) return;
    Base::Result<MemberId> result =
        context_->Types().TryRegisterEvent(ownerType_, registration);
    if (!result) status_ = result.GetStatus();
}

void MetaRegistrationBuilder::RoutedEvent(
    RoutedEventHandle declaredHandle,
    Base::StringView declarationName,
    TypeId eventArgsType,
    RoutingStrategy strategy) noexcept {
    if (!status_.IsOk()) return;
    if (context_->RoutedEvents() == nullptr) {
        status_ = Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Routed event metadata requires a RoutedEventRegistry");
        return;
    }
    const Base::StringView name = EventName(declarationName);
    if (name.Empty() || name == declarationName) {
        status_ = Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Routed event member must end with Event");
        return;
    }
    const RoutedEventHandle expected = MakeRoutedEventHandle(ownerType_, name);
    if (declaredHandle != expected) {
        status_ = Base::Status::Failure(Base::ErrorCode::IdCollision,
            "Declared routed event handle does not match owner and name");
        return;
    }
    Base::Result<RoutedEventHandle> result = context_->RoutedEvents()->TryRegister(
        {name, ownerType_, eventArgsType, strategy});
    if (!result) {
        status_ = result.GetStatus();
    } else if (result.Value() != declaredHandle) {
        status_ = Base::Status::Failure(Base::ErrorCode::IdCollision,
            "Routed event registry returned a different handle");
    }
}

void MetaRegistrationBuilder::Content(
    Base::StringView name,
    ContentKind kind) noexcept {
    if (!status_.IsOk()) return;
    PropertyFlags flags = PropertyFlags::Structural;
    if (kind == ContentKind::Collection) {
        flags = flags | PropertyFlags::Collection;
    }
    Base::Result<MemberId> member = context_->Types().TryRegisterProperty(
        ownerType_, {name, BuiltinTypes::UIElement, flags});
    if (!member) {
        status_ = member.GetStatus();
        return;
    }
    Record(context_->Types().TrySetContentMember(ownerType_, member.Value()));
}

void MetaRegistrationBuilder::Factory(ObjectFactory factory) noexcept {
    if (!status_.IsOk()) return;
    Record(context_->Types().TrySetFactory(ownerType_, factory));
}

void MetaRegistrationBuilder::Fail(Base::Status status) noexcept {
    if (status_.IsOk() && !status.IsOk()) {
        status_ = status;
    }
}

} // namespace Aero::Core
