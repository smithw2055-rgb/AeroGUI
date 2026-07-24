#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>

#include <cstdint>

namespace Aero::Core {
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

} // namespace

Base::Result<void> MetadataBehaviorRegistrationStore::Freeze() noexcept {
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
    for (const PropertyAccessorRegistration& accessor : propertyAccessors_) {
        if (accessor.member == InvalidMemberId ||
            types_->FindProperty(accessor.member) == nullptr ||
            !IsValidPropertyBehavior(accessor)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property accessor registration is invalid");
        }
    }
    for (const ValueMemberAccessorRegistration& accessor :
         valueMemberAccessors_) {
        const FieldInfo* field = types_->FindField(accessor.member);
        if (accessor.member == InvalidMemberId || field == nullptr ||
            accessor.get == nullptr ||
            (!HasFieldFlag(field->Flags(), FieldFlags::ReadOnly) &&
             accessor.set == nullptr)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Value member accessor registration is invalid");
        }
    }
    for (const MethodInvokerRegistration& invoker : methodInvokers_) {
        if (invoker.member == InvalidMemberId || invoker.invoke == nullptr ||
            types_->FindMethod(invoker.member) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Method invoker registration is invalid");
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
    for (const TypeInfo& type : types_->Types()) {
        for (const FieldInfo& field : type.Fields()) {
            const ValueMemberAccessorRegistration* accessor =
                FindValueMemberAccessor(field.Id());
            if (accessor == nullptr || accessor->get == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Registered value field has no accessor behavior");
            }
        }
        for (const MethodInfo& method : type.Methods()) {
            const MethodInvokerRegistration* invoker =
                FindMethodInvoker(method.Id());
            if (invoker == nullptr || invoker->invoke == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Registered method has no invoker behavior");
            }
        }
    }

    frozen_ = true;
    return {};
}

const TypeFactoryRegistration*
MetadataBehaviorRegistrationStore::FindTypeFactory(TypeId type) const noexcept {
    for (const TypeFactoryRegistration& registration : typeFactories_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const PropertyAccessorRegistration*
MetadataBehaviorRegistrationStore::FindPropertyAccessor(
    MemberId member) const noexcept {
    for (const PropertyAccessorRegistration& registration : propertyAccessors_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const ValueMemberAccessorRegistration*
MetadataBehaviorRegistrationStore::FindValueMemberAccessor(
    MemberId member) const noexcept {
    for (const ValueMemberAccessorRegistration& registration :
         valueMemberAccessors_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const MethodInvokerRegistration*
MetadataBehaviorRegistrationStore::FindMethodInvoker(
    MemberId member) const noexcept {
    for (const MethodInvokerRegistration& registration : methodInvokers_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const PropertyChangeNotificationRegistration*
MetadataBehaviorRegistrationStore::FindPropertyChangeNotification(
    TypeId type) const noexcept {
    for (const PropertyChangeNotificationRegistration& registration :
         propertyChangeNotifications_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const CollectionChangeNotificationRegistration*
MetadataBehaviorRegistrationStore::FindCollectionChangeNotification(
    TypeId type) const noexcept {
    for (const CollectionChangeNotificationRegistration& registration :
         collectionChangeNotifications_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

Base::Result<void> MetadataRegistrationTypes::ValidateRegistrationPair()
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

Base::Result<TypeId> MetadataRegistrationTypes::TryRegisterType(
    const TypeRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->TryRegisterType(*behaviors_, registration);
}

Base::Result<void> MetadataRegistrationTypes::TryRegisterInterface(
    TypeId ownerType,
    TypeId interfaceType) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->TryRegisterInterface(ownerType, interfaceType);
}

Base::Result<MemberId> MetadataRegistrationTypes::TryRegisterProperty(
    TypeId ownerType,
    const PropertyRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->TryRegisterProperty(*behaviors_, ownerType, registration);
}

Base::Result<MemberId> MetadataRegistrationTypes::TryRegisterField(
    TypeId ownerType,
    const FieldRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->TryRegisterField(*behaviors_, ownerType, registration);
}

Base::Result<MemberId> MetadataRegistrationTypes::TryRegisterEnumValue(
    TypeId ownerType,
    const EnumValueRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->TryRegisterEnumValue(ownerType, registration);
}

Base::Result<MemberId> MetadataRegistrationTypes::TryRegisterEvent(
    TypeId ownerType,
    const EventRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->TryRegisterEvent(ownerType, registration);
}

Base::Result<MemberId> MetadataRegistrationTypes::TryRegisterMethod(
    TypeId ownerType,
    const MethodRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->TryRegisterMethod(*behaviors_, ownerType, registration);
}

Base::Result<void> MetadataRegistrationTypes::TrySetFactory(
    TypeId type,
    ObjectFactory factory) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->TrySetFactory(*behaviors_, type, factory);
}

Base::Result<void> MetadataRegistrationTypes::TrySetContentMember(
    TypeId type,
    MemberId member) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->TrySetContentMember(type, member);
}

Base::Result<void>
MetadataRegistrationTypes::TryRegisterPropertyChangeNotification(
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
    return behaviors_->propertyChangeNotifications_.TryPushBack(
        registration);
}

Base::Result<void>
MetadataRegistrationTypes::TryRegisterCollectionChangeNotification(
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
    return behaviors_->collectionChangeNotifications_.TryPushBack(
        registration);
}

} // namespace Aero::Core
