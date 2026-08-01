#include <Aero/Meta/Describe.hpp>

#include "MetadataContextState.hpp"
#include "RoutedEventCatalog.hpp"

namespace Aero::Core::Detail {

MetadataAuthoringSession::MetadataAuthoringSession(
    MetadataContext& context,
    const TypeRegistration& registration,
    TypeId expectedType) noexcept
    : context_(&context) {
    Base::Result<TypeId> result =
        context_->Types().TryRegisterType(registration);
    if (!result) {
        status_ = result.GetStatus();
        return;
    }
    type_ = result.Value();
    if (type_ != expectedType) {
        status_ = Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Typed metadata descriptor does not match TypeOf<T>()");
    }
}

MetadataAuthoringSession&
MetadataAuthoringSession::Implements(
    TypeId interfaceType) noexcept {
    if (Ok()) {
        Record(context_->Types().TryRegisterInterface(
            type_, interfaceType));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Factory(
    ObjectFactory factory) noexcept {
    if (Ok()) {
        Record(context_->Types().TrySetFactory(
            type_, factory));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::PropertyChangeNotifications(
    PropertyChangeSubscribeCallback subscribe,
    PropertyChangeUnsubscribeCallback unsubscribe,
    void* callbackContext) noexcept {
    if (Ok()) {
        Record(context_->Types().
            TryRegisterPropertyChangeNotification({
                type_,
                subscribe,
                unsubscribe,
                callbackContext}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::CollectionChangeNotifications(
    CollectionChangeSubscribeCallback subscribe,
    CollectionChangeUnsubscribeCallback unsubscribe,
    void* callbackContext) noexcept {
    if (Ok()) {
        Record(context_->Types().
            TryRegisterCollectionChangeNotification({
                type_,
                subscribe,
                unsubscribe,
                callbackContext}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::DependencyProperty(
    DependencyPropertyHandle declaredHandle,
    Base::StringView name,
    TypeId valueType,
    Value defaultValue,
    PropertyMetadataFlags metadataFlags,
    DependencyPropertyFlags propertyFlags,
    ValidateValueCallback validate,
    CoerceValueCallback coerce,
    PropertyChangedCallback changed,
    UpdateSourceTrigger updateSourceTrigger) noexcept {
    if (!Ok()) return *this;
    if (name.Empty() ||
        declaredHandle !=
            MakeDependencyPropertyHandle(type_, name)) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Typed dependency property handle does not match owner and name"));
    }

    DependencyPropertyRegistration registration;
    registration.name = name;
    registration.ownerType = type_;
    registration.valueType = valueType;
    registration.flags = propertyFlags;
    registration.metadata.defaultValue =
        std::move(defaultValue);
    registration.metadata.flags = metadataFlags;
    registration.metadata.validate = validate;
    registration.metadata.coerce = coerce;
    registration.metadata.changed = changed;
    registration.metadata.defaultUpdateSourceTrigger =
        updateSourceTrigger;

    Base::Result<DependencyPropertyRegistrationResult>
        registered =
            context_->DependencyProperties().TryRegister(
                registration);
    if (!registered) {
        return Fail(registered.GetStatus());
    }
    if (registered.Value().property != declaredHandle) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Dependency property registry returned a different handle"));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Override(
    DependencyPropertyHandle property,
    TypeId ownerType,
    PropertyMetadata metadata) noexcept {
    if (Ok()) {
        Record(context_->DependencyProperties().
            TryOverrideMetadata(
                property,
                ownerType,
                std::move(metadata)));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::AddOwner(
    DependencyPropertyHandle property,
    TypeId ownerType,
    PropertyMetadata metadata) noexcept {
    if (Ok()) {
        Record(context_->DependencyProperties().
            TryAddOwner(
                property,
                ownerType,
                metadata));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::RoutedEvent(
    RoutedEventHandle declaredHandle,
    Base::StringView name,
    TypeId eventArgsType,
    RoutingStrategy strategy) noexcept {
    if (!Ok()) return *this;
    auto* state =
        static_cast<MetadataContextState*>(
            context_->state_);
    RoutedEventCatalog* events =
        state != nullptr ? state->events : nullptr;
    if (events == nullptr) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Routed event metadata requires a registry"));
    }
    if (name.Empty() ||
        declaredHandle !=
            MakeRoutedEventHandle(type_, name)) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Typed routed event handle does not match owner and name"));
    }
    Base::Result<RoutedEventHandle> registered =
        events->TryRegister({
            name,
            type_,
            eventArgsType,
            strategy});
    if (!registered) {
        return Fail(registered.GetStatus());
    }
    if (registered.Value() != declaredHandle) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Routed event registry returned a different handle"));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Content(
    Base::StringView name,
    TypeId valueType,
    ContentKind kind,
    ContentWriteCallback write,
    ContentClearCallback clear,
    ContentFlags contentFlags,
    void* contentContext) noexcept {
    if (!Ok()) return *this;
    if (valueType == InvalidTypeId) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Content property value type is invalid"));
    }
    if ((write == nullptr) != (clear == nullptr) ||
        (write == nullptr &&
         contentFlags != ContentFlags::None)) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Content access requires matching write and clear callbacks"));
    }

    PropertyFlags flags = PropertyFlags::Structural;
    if (kind == ContentKind::Collection) {
        flags = flags | PropertyFlags::Collection;
    }
    Base::Result<MemberId> member =
        context_->Types().TryRegisterProperty(
            type_, {name, valueType, flags});
    if (!member) return Fail(member.GetStatus());

    Record(context_->Types().TrySetContentMember(
        type_, member.Value()));
    if (Ok() && write != nullptr) {
        Record(context_->Types().TrySetContentAccessor({
            type_,
            member.Value(),
            kind,
            contentFlags,
            write,
            clear,
            contentContext}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Collection(
    Base::StringView name,
    TypeId valueType,
    ContentWriteCallback write,
    ContentClearCallback clear,
    PropertyFlags propertyFlags,
    ContentFlags contentFlags,
    void* contentContext) noexcept {
    if (!Ok()) return *this;
    if (valueType == InvalidTypeId ||
        write == nullptr ||
        clear == nullptr) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Collection property requires a value type and access callbacks"));
    }
    Base::Result<MemberId> member =
        context_->Types().TryRegisterProperty(
            type_,
            {
                name,
                valueType,
                propertyFlags |
                    PropertyFlags::Structural |
                    PropertyFlags::Collection});
    if (!member) return Fail(member.GetStatus());

    Record(context_->Types().TrySetContentAccessor({
        type_,
        member.Value(),
        ContentKind::Collection,
        contentFlags,
        write,
        clear,
        contentContext}));
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Property(
    const PropertyRegistration& registration) noexcept {
    if (Ok()) {
        Base::Result<MemberId> result =
            context_->Types().TryRegisterProperty(
                type_, registration);
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Field(
    const FieldRegistration& registration) noexcept {
    if (Ok()) {
        Base::Result<MemberId> result =
            context_->Types().TryRegisterField(
                type_, registration);
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::EnumValueRaw(
    Base::StringView name,
    std::uint64_t rawValue) noexcept {
    if (Ok()) {
        Base::Result<MemberId> result =
            context_->Types().TryRegisterEnumValue(
                type_, {name, rawValue});
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Content(
    MemberId member) noexcept {
    if (Ok()) {
        Record(context_->Types().TrySetContentMember(
            type_, member));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::ContentAccessor(
    MemberId member,
    ContentKind kind,
    ContentWriteCallback write,
    ContentClearCallback clear,
    ContentFlags contentFlags,
    void* contentContext) noexcept {
    if (!Ok()) return *this;
    if (member == InvalidMemberId ||
        write == nullptr ||
        clear == nullptr) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Content accessor requires a member and matching callbacks"));
    }
    Record(context_->Types().TrySetContentMember(
        type_, member));
    if (Ok()) {
        Record(context_->Types().TrySetContentAccessor({
            type_,
            member,
            kind,
            contentFlags,
            write,
            clear,
            contentContext}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::ValueSemantics(
    const ValueTypeRegistration& registration) noexcept {
    if (Ok()) {
        Record(context_->Values().
            TryRegisterValueSemantics(
                type_, registration));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::TextConverter(
    TextValueConverterCallback converter) noexcept {
    if (Ok()) {
        Record(context_->Values().
            TryRegisterTextConverter({
                type_,
                converter,
                &context_->ValueRegistrations()}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Fail(
    Base::Status status) noexcept {
    if (status_.IsOk() && !status.IsOk()) {
        status_ = status;
    }
    return *this;
}

Base::Result<void*>
MetadataAuthoringSession::OwnBehaviorContextRaw(
    std::size_t size,
    std::size_t alignment,
    void* source,
    void (*construct)(void*, void*) noexcept,
    void (*destroyValue)(void*) noexcept) noexcept {
    return context_->Types().Behaviors().
        TryOwnContextRaw(
            size,
            alignment,
            source,
            construct,
            destroyValue);
}

void MetadataAuthoringSession::ReleaseBehaviorContext(
    void* value) noexcept {
    context_->Types().Behaviors().
        ReleaseLastContext(value);
}

Base::Result<void>
MetadataAuthoringSession::Finish() const noexcept {
    return status_.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(status_);
}

void MetadataAuthoringSession::Record(
    Base::Result<void> result) noexcept {
    if (status_.IsOk() && !result) {
        status_ = result.GetStatus();
    }
}

} // namespace Aero::Core::Detail
