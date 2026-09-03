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
    Base::Result<void> retained =
        ownedContexts_.PushBack(context);
    if (!retained) {
        context.destroy(context);
        return retained.GetStatus();
    }
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
    Base::Result<void> reserved = ownedContexts_.Reserve(
        ownedContexts_.Size() + source.ownedContexts_.Size());
    if (!reserved) return reserved.GetStatus();
    for (OwnedBehaviorData& context : source.ownedContexts_) {
        Base::Result<void> added = ownedContexts_.PushBack(context);
        AERO_ASSERT(added);
        if (!added) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Reserved metadata context adoption unexpectedly failed");
        }
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
    for (const TypeFactoryRegistration& registration : typeFactories_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const ContentAccessorRegistration*
BehaviorTable::FindContentAccessor(
    MemberId member) const noexcept {
    for (const ContentAccessorRegistration& registration : contentAccessors_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const PropertyAccessorRegistration*
BehaviorTable::FindPropertyAccessor(
    MemberId member) const noexcept {
    for (const PropertyAccessorRegistration& registration : propertyAccessors_) {
        if (registration.member == member) return &registration;
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
    for (const PropertyChangeNotificationRegistration& registration :
         propertyChangeNotifications_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const CollectionChangeNotificationRegistration*
BehaviorTable::FindCollectionChangeNotification(
    TypeId type) const noexcept {
    for (const CollectionChangeNotificationRegistration& registration :
         collectionChangeNotifications_) {
        if (registration.type == type) return &registration;
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
    return behaviors_->contentAccessors_.PushBack(registration);
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
    return behaviors_->propertyChangeNotifications_.PushBack(
        registration);
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
    return behaviors_->collectionChangeNotifications_.PushBack(
        registration);
}

} // namespace Aero::Meta


