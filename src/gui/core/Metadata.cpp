// Consolidated metadata implementation. Keep sections ordered by dependency.

#include <Aero/Value.hpp>
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Meta.hpp>
#include <Aero/DependencyProperty.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <utility>


// ===== MetadataAuthoring =====



namespace Aero::Meta {

MetadataAuthoringSession::MetadataAuthoringSession(
    Meta::Registration& context,
    const TypeRegistration& registration,
    TypeId expectedType) noexcept
    : context_(&context) {
    Base::Result<TypeId> result =
        context_->Types().RegisterType(registration);
    Record(result);
    if (!result) return;
    type_ = result.Value();
    if (type_ != expectedType) {
        Fail(Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Typed metadata descriptor does not match TypeOf<T>()"));
    }
}

MetadataAuthoringSession&
MetadataAuthoringSession::Implements(
    TypeId interfaceType,
    InterfaceCastThunk cast) noexcept {
    if (Ok()) {
        Record(context_->Types().RegisterInterface(
            type_, interfaceType, cast));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Factory(
    ObjectFactory factory) noexcept {
    if (Ok()) {
        Record(context_->Types().SetFactory(
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
            RegisterPropertyChangeNotification({
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
            RegisterCollectionChangeNotification({
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
    FrameworkPropertyMetadataOptions metadataFlags,
    DependencyPropertyFlags propertyFlags,
    ValidateValueCallback validate,
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
    registration.metadata.changed = changed;
    registration.metadata.defaultUpdateSourceTrigger =
        updateSourceTrigger;

    Base::Result<DependencyPropertyRegistrationResult>
        registered =
            context_->DependencyProperties().Register(
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
            OverrideMetadata(
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
    PropertyMetadata metadata,
    DependencyPropertyFlags flags,
    Base::StringView aliasName) noexcept {
    if (Ok()) {
        Record(context_->DependencyProperties().
            AddOwner(
                property,
                ownerType,
                metadata,
                flags,
                aliasName));
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
        static_cast<Aero::RegistrationState*>(
            context_->state_);
    RoutedEventTable* events =
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
        events->Register({
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
        context_->Types().RegisterProperty(
            type_, {name, valueType, flags});
    if (!member) return Fail(member.GetStatus());

    Record(context_->Types().SetContentMember(
        type_, member.Value()));
    if (Ok() && write != nullptr) {
        Record(context_->Types().SetContentAccessor({
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
        context_->Types().RegisterProperty(
            type_,
            {
                name,
                valueType,
                propertyFlags |
                    PropertyFlags::Structural |
                    PropertyFlags::Collection});
    if (!member) return Fail(member.GetStatus());

    Record(context_->Types().SetContentAccessor({
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
            context_->Types().RegisterProperty(
                type_, registration);
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Field(
    const FieldRegistration&) noexcept {
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Method(
    const MethodRegistration&) noexcept {
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::EventHandler(
    Base::StringView name,
    EventHandlerThunk thunk) noexcept {
    if (Ok()) {
        Base::Result<void> result =
            context_->Types().RegisterEventHandler(
                type_, name, thunk);
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::TemplatePart(
    Base::StringView name,
    TypeId partType) noexcept {
    if (Ok()) {
        Base::Result<void> result =
            context_->Types().RegisterTemplatePart(
                type_, name, partType);
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
            context_->Types().RegisterEnumValue(
                type_, {name, rawValue});
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Content(
    MemberId member) noexcept {
    if (Ok()) {
        Record(context_->Types().SetContentMember(
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
    Record(context_->Types().SetContentMember(
        type_, member));
    if (Ok()) {
        Record(context_->Types().SetContentAccessor({
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
            RegisterValueSemantics(
                type_, registration));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::TextConverter(
    TextValueConverterCallback converter) noexcept {
    if (Ok()) {
        Record(context_->Values().
            RegisterTextConverter({
                type_,
                converter,
                &context_->ValueRegistrations()}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Fail(
    Base::Status status) noexcept {
    AERO_ASSERT(status.IsOk());
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
        OwnContextRaw(
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
    AERO_ASSERT(static_cast<bool>(result));
    if (status_.IsOk() && !result) {
        status_ = result.GetStatus();
    }
}

} // namespace Aero::Meta

// ===== BehaviorTable =====



namespace Aero::Meta {

Base::Result<void*>
BehaviorTable::OwnContextRaw(
    std::size_t size,
    std::size_t alignment,
    void* source,
    void (*construct)(void*, void*) noexcept,
    void (*destroyValue)(void*) noexcept) noexcept {
    if (size == 0U ||
        alignment == 0U ||
        source == nullptr ||
        construct == nullptr ||
        destroyValue == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata behavior context descriptor is invalid");
    }

    Base::IAllocator& allocator =
        Base::GetDefaultAllocator();
    void* memory = allocator.Allocate({
        size,
        alignment,
        Base::MemoryTag::General});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Metadata behavior context allocation failed");
    }
    construct(memory, source);

    OwnedBehaviorData context;
    context.allocator = &allocator;
    context.value = memory;
    context.destroyValue = destroyValue;
    context.size = size;
    context.alignment = alignment;
    context.destroy =
        [](OwnedBehaviorData& owned) noexcept {
            owned.destroyValue(owned.value);
            owned.allocator->Deallocate(
                owned.value,
                owned.size,
                owned.alignment,
                Base::MemoryTag::General);
            owned = {};
        };
    ownedContexts_.PushBack(context);
    return memory;
}
namespace {

bool IsValidPropertyBehavior(
    const PropertyAccessorRegistration& registration) noexcept {
    switch (registration.access) {
    case PropertyAccessKind::External:
        return registration.get == nullptr && registration.set == nullptr &&
            registration.provider == InvalidPropertyProviderId &&
            registration.context == nullptr;
    case PropertyAccessKind::Ordinary:
        return (registration.get != nullptr || registration.set != nullptr) &&
            registration.provider == InvalidPropertyProviderId;
    case PropertyAccessKind::Provider:
        return registration.provider != InvalidPropertyProviderId;
    }
    return false;
}

bool HasPropertyFlagValue(
    PropertyFlags value,
    PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

} // namespace

BehaviorTable::~BehaviorTable()
    noexcept {
    for (std::uint32_t index = ownedContexts_.Size();
         index > 0U; --index) {
        OwnedBehaviorData& context =
            ownedContexts_[index - 1U];
        if (context.destroy != nullptr) {
            context.destroy(context);
        }
    }
}

void BehaviorTable::ReleaseLastContext(
    void* value) noexcept {
    if (ownedContexts_.Empty() ||
        ownedContexts_.Back().value != value) {
        return;
    }
    OwnedBehaviorData& context = ownedContexts_.Back();
    if (context.destroy != nullptr) {
        context.destroy(context);
    }
    ownedContexts_.PopBack();
}

Base::Result<void> BehaviorTable::AdoptOwnedContextsFrom(
    BehaviorTable& source) noexcept {
    if (&source == this || source.ownedContexts_.Empty()) return {};
    ownedContexts_.Reserve(
        ownedContexts_.Size() + source.ownedContexts_.Size());
    for (OwnedBehaviorData& context : source.ownedContexts_) {
        ownedContexts_.PushBack(context);
        context = {};
    }
    source.ownedContexts_.Clear();
    return {};
}

Base::Result<void> BehaviorTable::Freeze() noexcept {
    if (frozen_) return {};
    if (types_ == nullptr || !types_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TypeRegistry must be frozen before behavior registrations");
    }

    for (const TypeFactoryRegistration& factory : typeFactories_) {
        const TypeInfo* type = types_->FindType(factory.type);
        if (factory.type == InvalidTypeId || factory.factory == nullptr ||
            type == nullptr || type->Kind() != MetadataTypeKind::Object) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Type factory registration is invalid");
        }
    }
    for (const ContentAccessorRegistration& content : contentAccessors_) {
        const TypeInfo* type = types_->FindType(content.type);
        const PropertyInfo* member = types_->FindProperty(content.member);
        const bool collection = member != nullptr &&
            HasPropertyFlagValue(
                member->Flags(), PropertyFlags::Collection);
        if (type == nullptr || type->Kind() != MetadataTypeKind::Object ||
            member == nullptr ||
            member->OwnerType() != content.type ||
            !HasPropertyFlagValue(
                member->Flags(), PropertyFlags::Structural) ||
            collection != (content.kind == ContentKind::Collection) ||
            content.write == nullptr || content.clear == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Content accessor registration is invalid");
        }
    }
    for (const PropertyAccessorRegistration& accessor : propertyAccessors_) {
        if (accessor.member == InvalidMemberId ||
            types_->FindProperty(accessor.member) == nullptr ||
            !IsValidPropertyBehavior(accessor)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property accessor registration is invalid");
        }
    }

    for (const PropertyChangeNotificationRegistration& notification :
         propertyChangeNotifications_) {
        const TypeInfo* type = types_->FindType(notification.type);
        if (type == nullptr ||
            (type->Kind() != MetadataTypeKind::Object &&
             type->Kind() != MetadataTypeKind::Interface) ||
            notification.subscribe == nullptr ||
            notification.unsubscribe == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property-change notification registration is invalid");
        }
    }
    for (const CollectionChangeNotificationRegistration& notification :
         collectionChangeNotifications_) {
        const TypeInfo* type = types_->FindType(notification.type);
        if (type == nullptr ||
            (type->Kind() != MetadataTypeKind::Object &&
             type->Kind() != MetadataTypeKind::Interface) ||
            !HasTypeFlag(type->Flags(), TypeFlags::Collection) ||
            notification.subscribe == nullptr ||
            notification.unsubscribe == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Collection-change notification registration is invalid");
        }
    }


    frozen_ = true;
    return {};
}

const TypeFactoryRegistration*
BehaviorTable::FindTypeFactory(TypeId type) const noexcept {
    if (const std::uint32_t* idx = typeFactoriesIndex_.Find(type)) {
        if (*idx < typeFactories_.Size()) return &typeFactories_[*idx];
    }
    return nullptr;
}

const ContentAccessorRegistration*
BehaviorTable::FindContentAccessor(
    MemberId member) const noexcept {
    if (const std::uint32_t* idx = contentAccessorsIndex_.Find(member)) {
        if (*idx < contentAccessors_.Size()) return &contentAccessors_[*idx];
    }
    return nullptr;
}

const PropertyAccessorRegistration*
BehaviorTable::FindPropertyAccessor(
    MemberId member) const noexcept {
    if (const std::uint32_t* idx = propertyAccessorsIndex_.Find(member)) {
        if (*idx < propertyAccessors_.Size()) return &propertyAccessors_[*idx];
    }
    return nullptr;
}

const ValueMemberAccessorRegistration*
BehaviorTable::FindValueMemberAccessor(
    MemberId member) const noexcept {
    for (const ValueMemberAccessorRegistration& registration :
         valueMemberAccessors_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const MethodInvokerRegistration*
BehaviorTable::FindMethodInvoker(
    MemberId member) const noexcept {
    for (const MethodInvokerRegistration& registration : methodInvokers_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const PropertyChangeNotificationRegistration*
BehaviorTable::FindPropertyChangeNotification(
    TypeId type) const noexcept {
    if (const std::uint32_t* idx = propertyChangeIndex_.Find(type)) {
        if (*idx < propertyChangeNotifications_.Size()) {
            return &propertyChangeNotifications_[*idx];
        }
    }
    return nullptr;
}

const CollectionChangeNotificationRegistration*
BehaviorTable::FindCollectionChangeNotification(
    TypeId type) const noexcept {
    if (const std::uint32_t* idx = collectionChangeIndex_.Find(type)) {
        if (*idx < collectionChangeNotifications_.Size()) {
            return &collectionChangeNotifications_[*idx];
        }
    }
    return nullptr;
}

Base::Result<void> RegistrationTypes::ValidateRegistrationPair()
    const noexcept {
    if (types_ == nullptr || behaviors_ == nullptr ||
        &behaviors_->Types() != types_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata registration type and behavior stores do not match");
    }
    if (types_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata type registry is frozen");
    }
    if (behaviors_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata behavior registration store is frozen");
    }
    return {};
}

Base::Result<TypeId> RegistrationTypes::RegisterType(
    const TypeRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterType(*behaviors_, registration);
}

Base::Result<void> RegistrationTypes::RegisterInterface(
    TypeId ownerType,
    TypeId interfaceType,
    InterfaceCastThunk cast) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterInterface(ownerType, interfaceType, cast);
}

Base::Result<MemberId> RegistrationTypes::RegisterProperty(
    TypeId ownerType,
    const PropertyRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterProperty(*behaviors_, ownerType, registration);
}

Base::Result<MemberId> RegistrationTypes::RegisterField(
    TypeId,
    const FieldRegistration&) const noexcept {
    return MemberId{};
}

Base::Result<MemberId> RegistrationTypes::RegisterEnumValue(
    TypeId ownerType,
    const EnumValueRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterEnumValue(ownerType, registration);
}

Base::Result<MemberId> RegistrationTypes::RegisterEvent(
    TypeId ownerType,
    const EventRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterEvent(ownerType, registration);
}

Base::Result<MemberId> RegistrationTypes::RegisterMethod(
    TypeId,
    const MethodRegistration&) const noexcept {
    return MemberId{};
}

Base::Result<void> RegistrationTypes::RegisterEventHandler(
    TypeId ownerType,
    Base::StringView name,
    EventHandlerThunk thunk) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterEventHandler(ownerType, name, thunk);
}

Base::Result<void> RegistrationTypes::RegisterTemplatePart(
    TypeId ownerType,
    Base::StringView name,
    TypeId partType) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterTemplatePart(ownerType, name, partType);
}
Base::Result<void> RegistrationTypes::SetFactory(
    TypeId type,
    ObjectFactory factory) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->SetFactory(*behaviors_, type, factory);
}

Base::Result<void> RegistrationTypes::SetContentMember(
    TypeId type,
    MemberId member) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->SetContentMember(type, member);
}

Base::Result<void> RegistrationTypes::SetContentAccessor(
    const ContentAccessorRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    const TypeInfo* type = types_->FindType(registration.type);
    const PropertyInfo* member = types_->FindProperty(registration.member);
    const bool collection = member != nullptr &&
        HasPropertyFlagValue(member->Flags(), PropertyFlags::Collection);
    if (type == nullptr || type->Kind() != MetadataTypeKind::Object ||
        member == nullptr ||
        member->OwnerType() != registration.type ||
        !HasPropertyFlagValue(
            member->Flags(), PropertyFlags::Structural) ||
        collection != (registration.kind == ContentKind::Collection) ||
        registration.write == nullptr || registration.clear == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Content accessor registration is invalid");
    }
    if (behaviors_->FindContentAccessor(registration.member) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Content accessor is already registered");
    }
    const std::uint32_t contentIdx = behaviors_->contentAccessors_.Size();
    behaviors_->contentAccessors_.PushBack(registration);
    static_cast<void>(behaviors_->contentAccessorsIndex_.Insert(registration.member, contentIdx));
    return {};
}

Base::Result<void>
RegistrationTypes::RegisterPropertyChangeNotification(
    const PropertyChangeNotificationRegistration& registration)
    const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid;
    const TypeInfo* type = types_->FindType(registration.type);
    if (type == nullptr ||
        (type->Kind() != MetadataTypeKind::Object &&
         type->Kind() != MetadataTypeKind::Interface) ||
        registration.subscribe == nullptr ||
        registration.unsubscribe == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Property-change notification registration is invalid");
    }
    if (behaviors_->FindPropertyChangeNotification(
            registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Property-change notification is already registered");
    }
    const std::uint32_t propNotifIdx = behaviors_->propertyChangeNotifications_.Size();
    behaviors_->propertyChangeNotifications_.PushBack(
        registration);
    static_cast<void>(behaviors_->propertyChangeIndex_.Insert(registration.type, propNotifIdx));
    return {};
}

Base::Result<void>
RegistrationTypes::RegisterCollectionChangeNotification(
    const CollectionChangeNotificationRegistration& registration)
    const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid;
    const TypeInfo* type = types_->FindType(registration.type);
    if (type == nullptr ||
        (type->Kind() != MetadataTypeKind::Object &&
         type->Kind() != MetadataTypeKind::Interface) ||
        !HasTypeFlag(type->Flags(), TypeFlags::Collection) ||
        registration.subscribe == nullptr ||
        registration.unsubscribe == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Collection-change notification registration is invalid");
    }
    if (behaviors_->FindCollectionChangeNotification(
            registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Collection-change notification is already registered");
    }
    const std::uint32_t collNotifIdx = behaviors_->collectionChangeNotifications_.Size();
    behaviors_->collectionChangeNotifications_.PushBack(
        registration);
    static_cast<void>(behaviors_->collectionChangeIndex_.Insert(registration.type, collNotifIdx));
    return {};
}

} // namespace Aero::Meta

// ===== MetaTable =====

// Executable metadata behavior is private to Meta::Registry.


namespace Aero {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

template<class Less>
Base::Result<void> BuildOrder(
    std::uint32_t count,
    Base::Vector<std::uint32_t>& order,
    Less less) noexcept {
    order.Reserve(count);
    for (std::uint32_t index = 0U; index < count; ++index) {
        order.PushBack(index);
    }
    for (std::uint32_t index = 1U; index < count; ++index) {
        const std::uint32_t value = order[index];
        std::uint32_t cursor = index;
        while (cursor > 0U && less(value, order[cursor - 1U])) {
            order[cursor] = order[cursor - 1U];
            --cursor;
        }
        order[cursor] = value;
    }
    return {};
}

bool HasEventFlag(EventFlags value, EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool IsValueType(const TypeInfo& type) noexcept {
    return (static_cast<std::uint32_t>(type.Flags()) &
        static_cast<std::uint32_t>(TypeFlags::ValueType)) != 0U;
}

} // namespace

Base::Result<void> MetaTable::SetTypeFacet(
    TypeId type,
    MetadataFacetKind kind,
    std::uint32_t index) noexcept {
    if (type == InvalidTypeId || index == InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Type facet reference is invalid");
    }
    const std::uint32_t* draftPos = typeDraftIndex_.Find(type);
    FacetDraft* draft = nullptr;
    if (draftPos != nullptr && *draftPos < typeDrafts_.Size()) {
        draft = &typeDrafts_[*draftPos];
    } else {
        const std::uint32_t newPos = typeDrafts_.Size();
        FacetDraft* added = typeDrafts_.EmplaceBack();
        added->key = type;
        draft = added;
        typeDraftIndex_.Insert(type, newPos);
    }
    const std::uint8_t slot = static_cast<std::uint8_t>(kind);
    if (draft->facets[slot] != InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Type facet is already registered");
    }
    draft->facets[slot] = index;
    return {};
}

Base::Result<void> MetaTable::SetMemberFacet(
    MemberId member,
    MetadataFacetKind kind,
    std::uint32_t index) noexcept {
    if (member == InvalidMemberId || index == InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Member facet reference is invalid");
    }
    const std::uint32_t* draftPos = memberDraftIndex_.Find(member);
    FacetDraft* draft = nullptr;
    if (draftPos != nullptr && *draftPos < memberDrafts_.Size()) {
        draft = &memberDrafts_[*draftPos];
    } else {
        const std::uint32_t newPos = memberDrafts_.Size();
        FacetDraft* added = memberDrafts_.EmplaceBack();
        added->key = member;
        draft = added;
        memberDraftIndex_.Insert(member, newPos);
    }
    const std::uint8_t slot = static_cast<std::uint8_t>(kind);
    if (draft->facets[slot] != InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Member facet is already registered");
    }
    draft->facets[slot] = index;
    return {};
}

Base::Result<void> MetaTable::Build(
    const TypeRegistry& types,
    const BehaviorTable& behaviors,
    const DependencyPropertyRegistry& dependencyProperties,
    const RoutedEventTable& routedEvents) noexcept {
    if (sealed_) return InvalidState("MetaTable is already sealed");
    if (!types.IsFrozen() || !behaviors.IsFrozen() ||
        !dependencyProperties.IsFrozen() || !routedEvents.IsFrozen()) {
        return InvalidState(
            "Metadata sources must be sealed before building facets");
    }
    types_ = &types;

    for (const TypeInfo& type : types.Types()) {
        Base::Result<void> result;
        const TypeFactoryRegistration* factory =
            behaviors.FindTypeFactory(type.Id());
        if (factory != nullptr && factory->factory != nullptr) {
            const std::uint32_t index = factories_.Size();
            factories_.PushBack({type.Id(), factory->factory});
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::TypeFactory, index);
            if (!result) return result.GetStatus();
        }
        if (type.ContentMember() != InvalidMemberId) {
            const ContentAccessorRegistration* content =
                behaviors.FindContentAccessor(
                    type.ContentMember());
            const PropertyInfo* descriptor =
                types.FindProperty(type.ContentMember());
            if (descriptor == nullptr) {
                return InvalidState(
                    "Content metadata references a missing property");
            }
            const bool collection =
                (static_cast<std::uint32_t>(descriptor->Flags()) &
                 static_cast<std::uint32_t>(PropertyFlags::Collection)) != 0U;
            const ContentKind kind = content != nullptr
                ? content->kind
                : (collection ? ContentKind::Collection : ContentKind::Single);
            const std::uint32_t index = contents_.Size();
            contents_.PushBack({
                type.Id(), type.ContentMember(), kind,
                content != nullptr ? content->flags : ContentFlags::None,
                content != nullptr ? content->write : nullptr,
                content != nullptr ? content->clear : nullptr,
                content != nullptr ? content->context : nullptr});
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::Content, index);
            if (!result) return result.GetStatus();
            result = SetMemberFacet(
                type.ContentMember(), MetadataFacetKind::Content, index);
            if (!result) return result.GetStatus();
        }
        for (const PropertyInfo& property :
             type.Properties()) {
            if (property.Id() == type.ContentMember()) {
                continue;
            }
            const ContentAccessorRegistration* content =
                behaviors.FindContentAccessor(
                    property.Id());
            if (content == nullptr) continue;
            const std::uint32_t index = contents_.Size();
            contents_.PushBack({
                type.Id(),
                property.Id(),
                content->kind,
                content->flags,
                content->write,
                content->clear,
                content->context});
            result = SetMemberFacet(
                property.Id(), MetadataFacetKind::Content, index);
            if (!result) return result.GetStatus();
        }
        const PropertyChangeNotificationRegistration* notification =
            behaviors.FindPropertyChangeNotification(type.Id());
        if (notification != nullptr) {
            const std::uint32_t index =
                propertyChangeNotifications_.Size();
            propertyChangeNotifications_.PushBack({
                type.Id(),
                notification->subscribe,
                notification->unsubscribe,
                notification->context});
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::PropertyChangeNotification,
                index);
            if (!result) return result.GetStatus();
        }
        const CollectionChangeNotificationRegistration*
            collectionNotification =
                behaviors.FindCollectionChangeNotification(type.Id());
        if (collectionNotification != nullptr) {
            const std::uint32_t index =
                collectionChangeNotifications_.Size();
            collectionChangeNotifications_.PushBack({
                type.Id(),
                collectionNotification->subscribe,
                collectionNotification->unsubscribe,
                collectionNotification->context});
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::CollectionChangeNotification,
                index);
            if (!result) return result.GetStatus();
        }

        for (const PropertyInfo& property : type.Properties()) {
            const PropertyAccessorRegistration* accessor =
                behaviors.FindPropertyAccessor(property.Id());
            if (accessor != nullptr &&
                accessor->access != PropertyAccessKind::External) {
                const std::uint32_t index = propertyAccessors_.Size();
                propertyAccessors_.PushBack({
                    property.Id(), accessor->access, accessor->get,
                    accessor->set, accessor->provider, accessor->context});
                result = SetMemberFacet(
                    property.Id(), MetadataFacetKind::PropertyAccessor,
                    index);
                if (!result) return result.GetStatus();
            }

            const DependencyProperty* dependency = dependencyProperties.Find(
                DependencyPropertyHandle{property.Id()});
            if (dependency != nullptr) {
                const std::uint32_t index = dependencyProperties_.Size();
                dependencyProperties_.PushBack({
                    property.Id(), dependency->Handle().value,
                    dependency->RegisteredOwnerType(), dependency->ValueType(),
                    static_cast<std::uint32_t>(dependency->Flags()),
                    dependency->MetadataCount(), dependency});
                result = SetMemberFacet(
                    property.Id(), MetadataFacetKind::DependencyProperty,
                    index);
                if (!result) return result.GetStatus();
            }
        }

        for (const EventInfo& event : type.Events()) {
            if (!HasEventFlag(event.Flags(), EventFlags::Routed)) continue;
            const RoutedEventTable::Definition* definition =
                routedEvents.Find({event.Id()});
            if (definition == nullptr) {
                return InvalidState(
                    "Routed event metadata has no catalog definition");
            }
            const std::uint32_t index = routedEvents_.Size();
            routedEvents_.PushBack({
                event.Id(), event.OwnerType(), event.EventArgsType(),
                definition->strategy});
            result = SetMemberFacet(
                event.Id(), MetadataFacetKind::RoutedEvent, index);
            if (!result) return result.GetStatus();
        }
    }

    sealed_ = true;
    return {};
}

Base::Result<void> MetaTable::SealIndex() noexcept {
    typeRecords_.Clear();
    memberRecords_.Clear();
    typeIndex_.Clear();
    memberIndex_.Clear();

    typeRecords_.Reserve(typeDrafts_.Size());
    memberRecords_.Reserve(memberDrafts_.Size());
    typeIndex_.Reserve(typeDrafts_.Size());
    memberIndex_.Reserve(memberDrafts_.Size());
    for (const FacetDraft& draft : typeDrafts_) {
        TypeRecord record;
        record.id = static_cast<TypeId>(draft.key);
        for (std::uint8_t kind = 0U; kind < 11U; ++kind) {
            if (draft.facets[kind] != InvalidFacetIndex) {
                record.mask |= UINT64_C(1) << kind;
            }
        }
        record.factoryIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::TypeFactory)];
        record.valueSemanticsIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::ValueSemantics)];
        record.textConverterIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::TextConverter)];
        record.propertyChangeIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::PropertyChangeNotification)];
        record.collectionChangeIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::CollectionChangeNotification)];

        const std::uint32_t position = typeRecords_.Size();
        typeRecords_.PushBack(record);
        Base::HashMap<TypeId, std::uint32_t>::InsertResult inserted =
            typeIndex_.Insert(record.id, position);
        if (!inserted.inserted) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata type facet record is duplicated");
        }
    }

    for (const FacetDraft& draft : memberDrafts_) {
        MemberRecord record;
        record.id = static_cast<MemberId>(draft.key);
        for (std::uint8_t kind = 0U; kind < 11U; ++kind) {
            if (draft.facets[kind] != InvalidFacetIndex) {
                record.mask |= UINT64_C(1) << kind;
            }
        }
        record.propertyAccessorIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::PropertyAccessor)];
        record.contentIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::Content)];
        record.valueMemberAccessorIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::ValueMemberAccessor)];
        record.methodInvokerIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::MethodInvoker)];

        const std::uint32_t position = memberRecords_.Size();
        memberRecords_.PushBack(record);
        Base::HashMap<MemberId, std::uint32_t>::InsertResult inserted =
            memberIndex_.Insert(record.id, position);
        if (!inserted.inserted) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata member facet record is duplicated");
        }
    }

    typeDrafts_ = Base::Vector<FacetDraft>();
    memberDrafts_ = Base::Vector<FacetDraft>();
    typeDraftIndex_.Clear();
    memberDraftIndex_.Clear();
    return {};
}

MetadataFacetMask MetaTable::TypeFacets(TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    return position != nullptr && *position < typeRecords_.Size()
        ? typeRecords_[*position].mask
        : 0U;
}

MetadataFacetMask MetaTable::MemberFacets(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    return position != nullptr && *position < memberRecords_.Size()
        ? memberRecords_[*position].mask
        : 0U;
}

const TypeFactoryFacet* MetaTable::FindTypeFactory(
    TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].factoryIndex;
    return index < factories_.Size() ? &factories_[index] : nullptr;
}

const ContentFacet* MetaTable::FindContentByMember(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    if (position == nullptr || *position >= memberRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = memberRecords_[*position].contentIndex;
    return index < contents_.Size() ? &contents_[index] : nullptr;
}

const PropertyAccessorFacet* MetaTable::FindPropertyAccessor(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    if (position == nullptr || *position >= memberRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = memberRecords_[*position].propertyAccessorIndex;
    return index < propertyAccessors_.Size()
        ? &propertyAccessors_[index] : nullptr;
}

const ValueMemberAccessorFacet* MetaTable::FindValueMemberAccessor(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    if (position == nullptr || *position >= memberRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = memberRecords_[*position].valueMemberAccessorIndex;
    return index < valueMemberAccessors_.Size()
        ? &valueMemberAccessors_[index] : nullptr;
}

const MethodInvokerFacet* MetaTable::FindMethodInvoker(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    if (position == nullptr || *position >= memberRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = memberRecords_[*position].methodInvokerIndex;
    return index < methodInvokers_.Size()
        ? &methodInvokers_[index] : nullptr;
}

const PropertyChangeNotificationFacet*
MetaTable::FindPropertyChangeNotification(TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].propertyChangeIndex;
    return index < propertyChangeNotifications_.Size()
        ? &propertyChangeNotifications_[index] : nullptr;
}

const CollectionChangeNotificationFacet*
MetaTable::FindCollectionChangeNotification(TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].collectionChangeIndex;
    return index < collectionChangeNotifications_.Size()
        ? &collectionChangeNotifications_[index] : nullptr;
}

Base::Result<Base::HashCode> MetaTable::ComputeHash() const noexcept {
    if (!sealed_) return InvalidState("Facet hash requires a sealed store");
    Base::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.FACETS.V4";
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(MetadataFacetFormatVersion);

    Base::Vector<std::uint32_t> order;
    Base::Result<void> result = BuildOrder(factories_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return factories_[left].type < factories_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) builder.AddU64(factories_[index].type);

    order.Clear();
    result = BuildOrder(contents_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return contents_[left].type < contents_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const ContentFacet& facet = contents_[index];
        builder.AddU64(facet.type);
        builder.AddU64(facet.member);
        builder.AddByte(static_cast<std::uint8_t>(facet.kind));
        builder.AddByte(static_cast<std::uint8_t>(facet.flags));
        builder.AddByte(facet.write != nullptr ? 1U : 0U);
        builder.AddByte(facet.clear != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(propertyAccessors_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return propertyAccessors_[left].member <
                propertyAccessors_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const PropertyAccessorFacet& facet = propertyAccessors_[index];
        builder.AddU64(facet.member);
        builder.AddByte(static_cast<std::uint8_t>(facet.access));
        builder.AddU64(facet.provider);
        builder.AddByte(facet.get != nullptr ? 1U : 0U);
        builder.AddByte(facet.set != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(valueMemberAccessors_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return valueMemberAccessors_[left].member <
                valueMemberAccessors_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const ValueMemberAccessorFacet& facet = valueMemberAccessors_[index];
        builder.AddU64(facet.member);
        builder.AddByte(facet.get != nullptr ? 1U : 0U);
        builder.AddByte(facet.set != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(methodInvokers_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return methodInvokers_[left].member < methodInvokers_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        builder.AddU64(methodInvokers_[index].member);
    }

    order.Clear();
    result = BuildOrder(dependencyProperties_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return dependencyProperties_[left].member <
                dependencyProperties_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const DependencyPropertyFacet& facet = dependencyProperties_[index];
        builder.AddU64(facet.member);
        builder.AddU64(facet.canonicalMember);
        builder.AddU64(facet.registeredOwnerType);
        builder.AddU64(facet.valueType);
        builder.AddU32(facet.flags);
        builder.AddU32(facet.metadataCount);
    }

    order.Clear();
    result = BuildOrder(routedEvents_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return routedEvents_[left].member < routedEvents_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const RoutedEventFacet& facet = routedEvents_[index];
        builder.AddU64(facet.member);
        builder.AddU64(facet.ownerType);
        builder.AddU64(facet.eventArgsType);
    }

    order.Clear();
    result = BuildOrder(propertyChangeNotifications_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return propertyChangeNotifications_[left].type <
                propertyChangeNotifications_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const PropertyChangeNotificationFacet& facet =
            propertyChangeNotifications_[index];
        builder.AddU64(facet.type);
        builder.AddByte(facet.subscribe != nullptr ? 1U : 0U);
        builder.AddByte(facet.unsubscribe != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(collectionChangeNotifications_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return collectionChangeNotifications_[left].type <
                collectionChangeNotifications_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const CollectionChangeNotificationFacet& facet =
            collectionChangeNotifications_[index];
        builder.AddU64(facet.type);
        builder.AddByte(facet.subscribe != nullptr ? 1U : 0U);
        builder.AddByte(facet.unsubscribe != nullptr ? 1U : 0U);
    }
    return builder.Finish();
}

Base::Result<void> MetaTable::BuildValueFacets(
    const ValueTable& source,
    const TypeRegistry& types) noexcept {
    if (!sealed_ || valueFacetsSealed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            valueFacetsSealed_
                ? "Metadata value facets are already sealed"
                : "Core metadata facets must be built before value facets");
    }
    if (!source.IsFrozen() || !types.IsFrozen() ||
        types_ != &types) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata value facet sources are not sealed or do not match");
    }

    std::uint32_t valueTypeCount = 0U;
    for (const TypeInfo& type : types.Types()) {
        if (IsValueType(type)) ++valueTypeCount;
    }
    valueSemantics_.Reserve(valueTypeCount);
    textConverters_.Reserve(valueTypeCount);
    Base::Result<void> result;
    for (const TypeInfo& type : types.Types()) {
        if (IsValueType(type)) {
            const ValueTypeRegistration* semantics =
                source.FindValueSemantics(type.Id());
            if (semantics != nullptr) {
                ValueSemanticsFacet facet;
                facet.type = type.Id();
                facet.registration = *semantics;
                const std::uint32_t index =
                    valueSemantics_.Size();
                valueSemantics_.PushBack(
                    std::move(facet));
                result = SetTypeFacet(
                    type.Id(), MetadataFacetKind::ValueSemantics, index);
                if (!result) return result.GetStatus();
            }
        }

        const TextValueConverterRegistration* converter =
            source.FindTextConverter(type.Id());
        if (converter != nullptr && converter->convert != nullptr) {
            TextConverterFacet facet;
            facet.type = type.Id();
            facet.convert = converter->convert;
            facet.context = converter->context;
            const std::uint32_t index = textConverters_.Size();
            textConverters_.PushBack(facet);
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::TextConverter, index);
            if (!result) return result.GetStatus();
        }
    }

    result = SealIndex();
    if (!result) return result.GetStatus();
    valueFacetsSealed_ = true;
    return {};
}

const ValueSemanticsFacet* MetaTable::FindValueSemantics(
    TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].valueSemanticsIndex;
    return index < valueSemantics_.Size()
        ? &valueSemantics_[index] : nullptr;
}

const TextConverterFacet* MetaTable::FindTextConverter(
    TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].textConverterIndex;
    return index < textConverters_.Size()
        ? &textConverters_[index] : nullptr;
}

Base::Result<Base::HashCode> ComputeMetadataValueFacetHash(
    const MetaTable& facets,
    const TypeRegistry& descriptors) noexcept {
    if (!facets.IsSealed() || !facets.ValueFacetsSealed() ||
        !descriptors.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Value facet hash requires sealed descriptors and facets");
    }

    Base::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.VALUE.FACETS.V1";
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(MetadataFacetFormatVersion);

    std::uint32_t semanticsCount = 0U;
    std::uint32_t converterCount = 0U;
    for (const TypeInfo& type : descriptors.Types()) {
        if (facets.FindValueSemantics(type.Id()) != nullptr) ++semanticsCount;
        if (facets.FindTextConverter(type.Id()) != nullptr) ++converterCount;
    }
    builder.AddU32(semanticsCount);
    for (const TypeInfo& type : descriptors.Types()) {
        const ValueSemanticsFacet* facet =
            facets.FindValueSemantics(type.Id());
        if (facet == nullptr) continue;
        const ValueTypeRegistration& registration =
            facet->registration;
        builder.AddU64(type.Id());
        builder.AddU32(registration.size);
        builder.AddU32(registration.alignment);
        builder.AddByte(registration.copy != nullptr ? 1U : 0U);
        builder.AddByte(registration.destroy != nullptr ? 1U : 0U);
        builder.AddByte(registration.equals != nullptr ? 1U : 0U);
        builder.AddByte(registration.inlineSafe ? 1U : 0U);
    }

    builder.AddU32(converterCount);
    for (const TypeInfo& type : descriptors.Types()) {
        const TextConverterFacet* facet =
            facets.FindTextConverter(type.Id());
        if (facet == nullptr) continue;
        builder.AddU64(type.Id());
        builder.AddByte(facet->convert != nullptr ? 1U : 0U);
    }
    return builder.Finish();
}

} // namespace Aero

// ===== Registry =====





namespace Aero::Meta {

struct Registry::Storage {
    struct ModuleRecord {
        MetadataModuleId id = InvalidMetadataModuleId;
        std::uint32_t schemaVersion = 1U;
        MetadataModuleRegisterCallback registerModule = nullptr;
        MetadataModuleRegisterContextCallback registerModuleWithContext =
            nullptr;
        void* context = nullptr;
        Base::String name;
    };

    TypeRegistry types;
    BehaviorTable behaviorRegistrations;
    ValueTable valueRegistrations;
    DependencyPropertyRegistry dependencyProperties;
    RoutedEventTable routedEvents;
    ::Aero::MetaTable facets;
    Base::Vector<ModuleRecord> modules;
    Base::Vector<MetadataPropertyProviderRegistration> providers;
    bool sealed = false;
    bool ready = false;

    Storage() noexcept
        : types(),
          behaviorRegistrations(types),
          valueRegistrations(types),
          dependencyProperties(types, behaviorRegistrations),
          routedEvents(types, behaviorRegistrations),
          facets(),
          modules(),
          providers() {}
};

Registry::Registry() noexcept
    : storage_(new (std::nothrow) Storage()) {}

Registry::~Registry() noexcept {
    delete storage_;
    storage_ = nullptr;
}

Base::Status Registry::OutOfMemoryStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory,
        "Registry storage allocation failed");
}

bool Registry::IsValid() const noexcept {
    return storage_ != nullptr;
}

bool Registry::IsSealed() const noexcept {
    return storage_ != nullptr && storage_->sealed;
}

std::uint32_t Registry::ModuleCount() const noexcept {
    return storage_ != nullptr ? storage_->modules.Size() : 0U;
}

Base::Result<void> Registry::ValidateRegistration(
    const MetadataModuleRegistration& registration) noexcept {
    if (registration.id == InvalidMetadataModuleId ||
        registration.name.Empty() || registration.schemaVersion == 0U ||
        (registration.registerModule == nullptr &&
         registration.registerModuleWithContext == nullptr) ||
        (registration.registerModule != nullptr &&
         registration.registerModuleWithContext != nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata module registration is incomplete");
    }
    if (registration.id != MakeMetadataModuleId(registration.name)) {
        return Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Metadata module id does not match its stable name");
    }
    return {};
}

Base::Result<void> Registry::RegisterModule(
    const MetadataModuleRegistration& registration) noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry is sealed");
    }
    Base::Result<void> valid = ValidateRegistration(registration);
    if (!valid) return valid.GetStatus();
    for (const Storage::ModuleRecord& module : storage_->modules) {
        if (module.id == registration.id) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata module is already registered");
        }
    }

    // Modules are accumulated only while the registry is mutable. Applying
    // directly keeps behavior contexts owned by their registering table and
    // avoids rebuilding the value-semantic store once per module. Seal() then
    // freezes this single registry into the immutable runtime snapshot.
    ::Aero::RegistrationState contextState{
        &storage_->types,
        &storage_->behaviorRegistrations,
        &storage_->valueRegistrations,
        &storage_->dependencyProperties,
        &storage_->routedEvents};
    Registration context(&contextState);
    Base::Result<void> applied = registration.registerModule != nullptr
        ? registration.registerModule(context)
        : registration.registerModuleWithContext(
              context, registration.context);
    if (!applied) return applied.GetStatus();

    Storage::ModuleRecord record;
    record.id = registration.id;
    record.schemaVersion = registration.schemaVersion;
    Base::Result<void> assigned = record.name.Assign(registration.name);
    if (!assigned) return assigned.GetStatus();
    storage_->modules.PushBack(std::move(record));
    return {};
}

Base::Result<void> Registry::Seal() noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) return {};
    Base::Result<void> frozen = storage_->types.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->behaviorRegistrations.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->valueRegistrations.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->dependencyProperties.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->routedEvents.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->facets.Build(
        storage_->types,
        storage_->behaviorRegistrations,
        storage_->dependencyProperties,
        storage_->routedEvents);
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->facets.BuildValueFacets(
        storage_->valueRegistrations, storage_->types);
    if (!frozen) return frozen.GetStatus();
    storage_->behaviorRegistrations.ReleaseRegistrationTables();
    storage_->sealed = true;
    return {};
}

const TypeRegistry& Registry::Types() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->types;
}

DependencyPropertyRegistry& Registry::DependencyProperties() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

const DependencyPropertyRegistry& Registry::DependencyProperties() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

void* Registry::RoutedEventState() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return &storage_->routedEvents;
}

const ::Aero::MetaTable& Registry::RuntimeData() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->facets;
}

Base::Result<Base::HashCode> Registry::ComputeSchemaHash() const noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (!storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry schema hash requires a sealed domain");
    }

    Base::Result<Base::HashCode> descriptorHash =
        storage_->types.ComputeHash();
    if (!descriptorHash) return descriptorHash.GetStatus();
    Base::Result<Base::HashCode> facetHash = storage_->facets.ComputeHash();
    if (!facetHash) return facetHash.GetStatus();
    Base::Result<Base::HashCode> valueFacetHash =
        ::Aero::ComputeMetadataValueFacetHash(
            storage_->facets, storage_->types);
    if (!valueFacetHash) return valueFacetHash.GetStatus();

    Base::HashCode hash = Base::MixHash64(
        descriptorHash.Value() ^ facetHash.Value());
    hash = Base::MixHash64(hash ^ valueFacetHash.Value());
    for (const Storage::ModuleRecord& module : storage_->modules) {
        hash = Base::MixHash64(hash ^ module.id);
        hash = Base::MixHash64(
            hash ^ static_cast<Base::HashCode>(module.schemaVersion));
    }
    return hash;
}

} // namespace Aero::Meta

// Executable metadata operations share the Registry storage and lifetime.

namespace Aero::Meta {

Base::Result<void> Registry::RegisterPropertyProvider(
    const MetadataPropertyProviderRegistration& registration) noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->ready) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry is complete");
    }
    if (registration.id == InvalidPropertyProviderId ||
        registration.objectType == InvalidTypeId ||
        (registration.get == nullptr && registration.set == nullptr) ||
        !IsSealed() ||
        Types().FindType(registration.objectType) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property provider registration is invalid");
    }
    if (FindProvider(registration.id) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Metadata property provider is already registered");
    }
    storage_->providers.PushBack(registration);
    return {};
}

Base::Result<void> Registry::Complete() noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->ready) return {};
    if (!IsSealed() ||
        !Types().IsFrozen() ||
        !RuntimeData().IsSealed() ||
        !RuntimeData().ValueFacetsSealed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry and all typed behavior must be sealed before Registry completion");
    }
    storage_->ready = true;
    return {};
}


bool Registry::IsReady() const noexcept {
    return storage_ != nullptr && storage_->ready && storage_->sealed;
}

bool Registry::CanReadProperty(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    return accessor != nullptr &&
        (accessor->access == PropertyAccessKind::Provider ||
         (accessor->access == PropertyAccessKind::Ordinary &&
          accessor->get != nullptr));
}

bool Registry::CanWriteProperty(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    return accessor != nullptr &&
        (accessor->access == PropertyAccessKind::Provider ||
         (accessor->access == PropertyAccessKind::Ordinary &&
          accessor->set != nullptr));
}

bool Registry::CanReadValueMember(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    return accessor != nullptr && accessor->get != nullptr;
}

bool Registry::CanWriteValueMember(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    return accessor != nullptr && accessor->set != nullptr;
}

MemberId Registry::FindContentMember(
    TypeId type) const noexcept {
    return IsReady()
        ? Types().FindContentMember(type)
        : InvalidMemberId;
}

Base::Result<ContentInfo> Registry::GetContentInfo(
    MemberId member) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::ContentFacet* content =
        RuntimeData().FindContentByMember(member);
    if (content == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata content member was not found");
    }
    return ContentInfo{
        content->type,
        content->member,
        content->kind,
        content->flags,
        content->write != nullptr,
        content->clear != nullptr};
}

Base::Result<void> Registry::WriteContent(
    Base::Object& owner,
    MemberId member,
    const Base::Ref<Base::Object>& value) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::ContentFacet* content =
        RuntimeData().FindContentByMember(member);
    if (content == nullptr || content->write == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata content member is not writable");
    }
    const PropertyInfo* property =
        Types().FindProperty(member);
    const bool attached =
        property != nullptr &&
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::Attached);
    if (!attached &&
        !Types().IsAssignableFrom(
            content->type, owner.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata content owner type is incompatible");
    }
    const bool acceptsAnyValue =
        property != nullptr &&
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::AnyValue);
    if (!value || property == nullptr ||
        (!acceptsAnyValue &&
         !Types().IsAssignableFrom(
             property->ValueType(),
             value->RuntimeType()))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata content value type is incompatible");
    }
    content->write(owner, value, content->context);
    return {};
}

Base::Result<void> Registry::ClearContent(
    Base::Object& owner,
    MemberId member) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::ContentFacet* content =
        RuntimeData().FindContentByMember(member);
    if (content == nullptr || content->clear == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata content member cannot be cleared");
    }
    const PropertyInfo* property =
        Types().FindProperty(member);
    const bool attached =
        property != nullptr &&
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::Attached);
    if (!attached &&
        !Types().IsAssignableFrom(
            content->type, owner.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata content owner type is incompatible");
    }
    content->clear(owner, content->context);
    return {};
}

Base::Result<std::uint64_t>
Registry::SubscribePropertyChanged(
    Base::Object& object,
    MetadataPropertyChangedCallback callback,
    void* callbackContext) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::PropertyChangeNotificationFacet* notification =
        RuntimeData().FindPropertyChangeNotification(
            object.RuntimeType());
    if (notification == nullptr ||
        notification->subscribe == nullptr) {
        return UINT64_C(0);
    }
    return notification->subscribe(
        object,
        callback,
        callbackContext,
        notification->context);
}

bool Registry::UnsubscribePropertyChanged(
    Base::Object& object,
    std::uint64_t subscription) const noexcept {
    if (!IsReady() || subscription == 0U) return false;
    const ::Aero::PropertyChangeNotificationFacet* notification =
        RuntimeData().FindPropertyChangeNotification(
            object.RuntimeType());
    if (notification == nullptr ||
        notification->unsubscribe == nullptr) {
        return false;
    }
    return notification->unsubscribe(
        object,
        subscription,
        notification->context);
}

void* Registry::TryCastToInterface(
    Base::Object& object,
    TypeId interfaceType) const noexcept {
    if (!IsReady()) {
        return nullptr;
    }
    return Types().TryCastToInterface(object, interfaceType);
}

Base::Result<Base::Ref<Base::Object>>
Registry::CreateObject(TypeId type) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    const ::Aero::TypeFactoryFacet* factory =
        RuntimeData().FindTypeFactory(type);
    if (descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata type descriptor was not found");
    }
    if (descriptor->Kind() != MetadataTypeKind::Object ||
        HasTypeFlag(descriptor->Flags(), TypeFlags::Abstract) ||
        factory == nullptr ||
        factory->factory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata type has no constructible factory");
    }
    Base::Ref<Base::Object> created = factory->factory();
    if (!created) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Metadata factory returned a null object");
    }
    if (created->RuntimeType() != type) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata factory returned the wrong runtime type");
    }
    return created;
}

Base::Result<Value> Registry::TryCreateValue(
    TypeId type,
    const void* source) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    const ::Aero::ValueSemanticsFacet* behavior =
        RuntimeData().FindValueSemantics(type);
    if (descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata value type descriptor was not found");
    }
    if (!HasTypeFlag(descriptor->Flags(), TypeFlags::ValueType) ||
        behavior == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata type has no value semantics");
    }
    return Value::TryFromCustom(
        type, source, &behavior->registration);
}

Base::Result<Value> Registry::TryConvertText(
    TypeId type,
    Base::StringView text) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    if (descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata text target descriptor was not found");
    }
    if (descriptor->Kind() == MetadataTypeKind::Enum) {
        return ConvertEnumText(*descriptor, text);
    }
    const ::Aero::TextConverterFacet* converter =
        RuntimeData().FindTextConverter(type);
    for (const TypeInfo* cur = descriptor;
         converter == nullptr && cur != nullptr && cur->BaseType() != InvalidTypeId;
         cur = Types().FindType(cur->BaseType())) {
        converter = RuntimeData().FindTextConverter(cur->BaseType());
    }
    if (converter == nullptr ||
        converter->convert == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata type has no text converter");
    }
    Base::Result<Value> converted =
        converter->convert(type, text, converter->context);
    if (!converted && converter->type != type) {
        converted = converter->convert(converter->type, text, converter->context);
    }
    if (!converted) return converted.GetStatus();
    if (converted.Value().IsUnset() ||
        (converted.Value().Type() != type &&
         !Types().IsAssignableFrom(type, converted.Value().Type()))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata text converter returned an incompatible value");
    }
    return converted;
}



Base::Result<Value> Registry::GetProperty(
    const Base::Object& object,
    MemberId member) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const PropertyInfo* property =
        Types().FindProperty(member);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata property descriptor was not found");
    }
    Base::Result<void> target =
        ValidatePropertyTarget(object, *property);
    if (!target) return target.GetStatus();
    if (HasPropertyFlag(
            property->Flags(), PropertyFlags::WriteOnly)) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Write-only metadata property cannot be read");
    }
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    if (accessor == nullptr) return UnsupportedProperty();

    Base::Result<Value> value = UnsupportedProperty();
    if (accessor->access == PropertyAccessKind::Ordinary) {
        if (accessor->get == nullptr) {
            return UnsupportedProperty();
        }
        value = accessor->get(object, accessor->context);
    } else if (accessor->access ==
               PropertyAccessKind::Provider) {
        if (accessor->provider ==
            DependencyPropertyProviderId) {
            value = GetDependencyProperty(
                object, *property);
        } else {
            const MetadataPropertyProviderRegistration* provider =
                FindProvider(accessor->provider);
            if (provider == nullptr ||
                provider->get == nullptr ||
                !Types().IsAssignableFrom(
                    provider->objectType,
                    object.RuntimeType())) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Readable metadata property provider was not found");
            }
            value = provider->get(
                object, *property, provider->context);
        }
    }
    if (!value) return value.GetStatus();
    const bool objectAssignment =
        value.Value().Kind() == ValueKind::Object &&
        Types().IsAssignableFrom(
            property->ValueType(), value.Value().Type());
    if (value.Value().IsUnset() ||
        (!HasPropertyFlag(
             property->Flags(),
             PropertyFlags::AnyValue) &&
         value.Value().Type() != property->ValueType() &&
         !objectAssignment)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata getter returned an incompatible value");
    }
    if (objectAssignment && value.Value().Type() != property->ValueType()) {
        return Value::FromObject(
            property->ValueType(),
            value.Value().AsObject());
    }
    return value;
}

Base::Result<void> Registry::SetProperty(
    Base::Object& object,
    MemberId member,
    const Value& value) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const PropertyInfo* property =
        Types().FindProperty(member);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata property descriptor was not found");
    }
    Base::Result<void> target =
        ValidatePropertyTarget(object, *property);
    if (!target) return target.GetStatus();
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property value is unset");
    }
    if (HasPropertyFlag(
            property->Flags(), PropertyFlags::ReadOnly)) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Read-only metadata property cannot be written");
    }
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    if (accessor == nullptr) return UnsupportedProperty();

    const bool acceptsAnyValue =
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::AnyValue);
    const bool objectAssignment =
        value.Kind() == ValueKind::Object &&
        Types().IsAssignableFrom(
            property->ValueType(), value.Type());
    if (!acceptsAnyValue &&
        value.Type() != property->ValueType() &&
        !objectAssignment) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property value type does not match the descriptor");
    }
    if (!acceptsAnyValue &&
        !IsRegisteredEnumValue(
            property->ValueType(), value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Metadata enum property value is not registered");
    }
    if (accessor->access ==
        PropertyAccessKind::Ordinary) {
        if (accessor->set == nullptr) {
            return UnsupportedProperty();
        }
        accessor->set(object, value, accessor->context);
        return {};
    }
    if (accessor->access ==
        PropertyAccessKind::Provider) {
        if (accessor->provider ==
            DependencyPropertyProviderId) {
            return SetDependencyProperty(
                object, *property, value);
        }
        const MetadataPropertyProviderRegistration* provider =
            FindProvider(accessor->provider);
        if (provider == nullptr ||
            provider->set == nullptr ||
            !Types().IsAssignableFrom(
                provider->objectType,
                object.RuntimeType())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Writable metadata property provider was not found");
        }
        return provider->set(
            object, *property, value, provider->context);
    }
    return UnsupportedProperty();
}

EventHandlerThunk Registry::FindEventHandler(
    TypeId ownerType,
    Base::StringView name,
    bool includeBaseTypes) const noexcept {
    return Types().FindEventHandler(ownerType, name, includeBaseTypes);
}

TypeId Registry::FindTemplatePart(
    TypeId ownerType,
    Base::StringView name,
    bool includeBaseTypes) const noexcept {
    return Types().FindTemplatePart(ownerType, name, includeBaseTypes);
}

EventHandlerThunk Registry::FindEventHandlerThunk(
    MemberId member) const noexcept {
    if (!storage_->ready) return nullptr;
    const ::Aero::MethodInvokerFacet* invoker =
        RuntimeData().FindMethodInvoker(member);
    return invoker != nullptr ? reinterpret_cast<EventHandlerThunk>(invoker->context) : nullptr;
}

bool Registry::HasPropertyFlag(
    PropertyFlags value,
    PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status Registry::MetadataNotReady() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Registry is not complete");
}

Base::Status Registry::UnsupportedProperty() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Metadata property has no usable accessor");
}

bool Registry::IsRegisteredEnumValue(
    TypeId type,
    const Value& value) const noexcept {
    const TypeInfo* info = Types().FindType(type);
    if (info == nullptr ||
        info->Kind() != MetadataTypeKind::Enum) {
        return true;
    }
    if (HasTypeFlag(
            info->Flags(), TypeFlags::SignedEnum)) {
        return value.Kind() ==
                ValueKind::SignedInteger &&
            Types().IsEnumValue(
                type,
                static_cast<std::uint64_t>(
                    value.AsSignedInteger()));
    }
    return value.Kind() ==
            ValueKind::UnsignedInteger &&
        Types().IsEnumValue(
            type, value.AsUnsignedInteger());
}

Base::Result<Value> Registry::ConvertEnumText(
    const TypeInfo& type,
    Base::StringView input) noexcept {
    Base::StringView remaining =
        ::Aero::Base::ValueConversion::Trim(input);
    if (remaining.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Enum text is empty");
    }
    std::uint64_t raw = 0U;
    std::uint32_t tokenCount = 0U;
    while (!remaining.Empty()) {
        std::uint32_t split =
            remaining.SizeBytes();
        for (std::uint32_t index = 0U;
             index < remaining.SizeBytes(); ++index) {
            if (remaining[index] == ',' ||
                remaining[index] == '|') {
                split = index;
                break;
            }
        }
        const Base::StringView token =
            ::Aero::Base::ValueConversion::Trim(
                remaining.Substr(0U, split));
        if (token.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Enum text contains an empty value");
        }
        const EnumValueInfo* match = nullptr;
        for (const EnumValueInfo& candidate :
             type.EnumValues()) {
            if (candidate.Name().SizeBytes() == token.SizeBytes() &&
                ::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
                    token, candidate.Name())) {
                match = &candidate;
                break;
            }
        }
        if (match == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Enum text value is not registered");
        }
        ++tokenCount;
        if (!type.IsFlagsEnum() &&
            tokenCount > 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Non-flags enum accepts exactly one value");
        }
        raw = type.IsFlagsEnum()
            ? (raw | match->RawValue())
            : match->RawValue();
        if (split == remaining.SizeBytes()) break;
        remaining = ::Aero::Base::ValueConversion::Trim(
            remaining.Substr(split + 1U));
    }
    return HasTypeFlag(
               type.Flags(), TypeFlags::SignedEnum)
        ? Value::FromSignedInteger(
              type.Id(),
              static_cast<std::int64_t>(raw))
        : Value::FromUnsignedInteger(type.Id(), raw);
}

Base::Result<void> Registry::ValidatePropertyTarget(
    const Base::Object& object,
    const PropertyInfo& property) const noexcept {
    if (HasPropertyFlag(
            property.Flags(), PropertyFlags::Attached)) {
        if (object.RuntimeType() != InvalidTypeId &&
            Types().FindType(
                object.RuntimeType()) != nullptr) {
            return {};
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Attached metadata property target has no type information");
    }
    if (Types().IsAssignableFrom(
            property.OwnerType(),
            object.RuntimeType())) {
        return {};
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Object type is incompatible with the metadata property");
}

Base::Result<Value> Registry::GetDependencyProperty(
    const Base::Object& object,
    const PropertyInfo& property) const noexcept {
    const DependencyPropertyHandle handle{property.Id()};
    const DependencyPropertyRegistry& registry =
        DependencyProperties();
    if (!Types().IsAssignableFrom(
            TypeOf<DependencyObject>(),
            object.RuntimeType()) ||
        registry.Find(handle) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property metadata target is invalid");
    }
    const auto& dependencyObject =
        static_cast<const DependencyObject&>(object);
    return dependencyObject.GetValue(handle);
}

Base::Result<void> Registry::SetDependencyProperty(
    Base::Object& object,
    const PropertyInfo& property,
    const Value& value) const noexcept {
    const DependencyPropertyHandle handle{property.Id()};
    const DependencyPropertyRegistry& registry =
        DependencyProperties();
    if (!Types().IsAssignableFrom(
            TypeOf<DependencyObject>(),
            object.RuntimeType()) ||
        registry.Find(handle) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property metadata target is invalid");
    }
    auto& dependencyObject =
        static_cast<DependencyObject&>(object);
    dependencyObject.SetValue(handle, value);
    return {};
}

const MetadataPropertyProviderRegistration*
Registry::FindProvider(
    PropertyProviderId id) const noexcept {
    for (const MetadataPropertyProviderRegistration& provider :
         storage_->providers) {
        if (provider.id == id) return &provider;
    }
    return nullptr;
}

} // namespace Aero::Meta

// ===== Registration =====

namespace Aero::Meta {

namespace {

::Aero::RegistrationState& State(void* value) noexcept {
    return *static_cast<::Aero::RegistrationState*>(value);
}

} // namespace

bool Registration::ContainsType(TypeId type) const noexcept {
    const ::Aero::RegistrationState& state =
        *static_cast<const ::Aero::RegistrationState*>(state_);
    return state.types != nullptr &&
        state.types->FindType(type) != nullptr;
}

RegistrationTypes Registration::Types() noexcept {
    ::Aero::RegistrationState& state = State(state_);
    return RegistrationTypes(
        *state.types, *state.behaviors);
}

RegistrationValues Registration::Values() noexcept {
    return RegistrationValues(
        State(state_).values,
        State(state_).values);
}

RegistrationValues Registration::Values() const noexcept {
    return RegistrationValues(
        State(state_).values,
        nullptr);
}

ValueTable&
Registration::ValueRegistrations() noexcept {
    return *State(state_).values;
}

DependencyPropertyRegistry&
Registration::DependencyProperties() noexcept {
    return *State(state_).properties;
}

} // namespace Aero::Meta

#include "gui/core/Describe.hpp"
#include <Aero/ResourceProvider.hpp>
#include <Aero/PropertyPath.hpp>

namespace Aero {

AERO_DESCRIBE(ResourceProvider) {
    auto type = Register<ResourceProvider>(context, TypeFlags::Abstract);
    (void)type.Result();
}

AERO_DESCRIBE(PropertyPath) {
    auto type = Register<PropertyPath>(context);
    (void)type.Result();
}

} // namespace Aero


