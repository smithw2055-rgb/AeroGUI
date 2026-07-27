#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>

#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Core {

namespace Detail {
class MetadataFacetStore;
}
class MetadataRegistrationTypes;
template<class T>
class TypeDescription;

// Mutable registration storage for executable type/member behavior.
//
// TypeRegistry owns callback-free structural metadata only. Registration code
// enters through MetadataRegistrationTypes, which commits structural records to
// TypeRegistry and executable records to this store as one registration step.
class AERO_API MetadataBehaviorRegistrationStore final {
public:
    explicit MetadataBehaviorRegistrationStore(TypeRegistry& types) noexcept
        : types_(&types) {}
    ~MetadataBehaviorRegistrationStore() noexcept;

    MetadataBehaviorRegistrationStore(
        const MetadataBehaviorRegistrationStore&) = delete;
    MetadataBehaviorRegistrationStore& operator=(
        const MetadataBehaviorRegistrationStore&) = delete;
    MetadataBehaviorRegistrationStore(
        MetadataBehaviorRegistrationStore&&) = delete;
    MetadataBehaviorRegistrationStore& operator=(
        MetadataBehaviorRegistrationStore&&) = delete;

    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    const TypeRegistry& Types() const noexcept { return *types_; }

private:
    friend class Detail::MetadataFacetStore;
    friend class MetadataRegistrationTypes;
    friend class TypeRegistry;

    struct OwnedBehaviorContext final {
        Base::IAllocator* allocator = nullptr;
        void* value = nullptr;
        void (*destroy)(OwnedBehaviorContext&) noexcept = nullptr;
    };

    template<class TContext>
    Base::Result<std::decay_t<TContext>*> TryOwnContext(
        TContext&& value) noexcept {
        using Stored = std::decay_t<TContext>;
        Base::IAllocator& allocator = Base::GetDefaultAllocator();
        void* memory = allocator.Allocate({
            sizeof(Stored),
            alignof(Stored),
            Base::MemoryTag::General});
        if (memory == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Metadata behavior context allocation failed");
        }
        auto* stored = new (memory) Stored(
            std::forward<TContext>(value));
        OwnedBehaviorContext context;
        context.allocator = &allocator;
        context.value = stored;
        context.destroy = [](OwnedBehaviorContext& owned) noexcept {
            auto* typed = static_cast<Stored*>(owned.value);
            typed->~Stored();
            owned.allocator->Deallocate(
                typed,
                sizeof(Stored),
                alignof(Stored),
                Base::MemoryTag::General);
            owned = {};
        };
        Base::Result<void> retained =
            ownedContexts_.TryPushBack(context);
        if (!retained) {
            context.destroy(context);
            return retained.GetStatus();
        }
        return stored;
    }
    void ReleaseLastContext(void* value) noexcept;

    const TypeFactoryRegistration* FindTypeFactory(
        TypeId type) const noexcept;
    const ContentAccessorRegistration* FindContentAccessor(
        MemberId member) const noexcept;
    const PropertyAccessorRegistration* FindPropertyAccessor(
        MemberId member) const noexcept;
    const ValueMemberAccessorRegistration* FindValueMemberAccessor(
        MemberId member) const noexcept;
    const MethodInvokerRegistration* FindMethodInvoker(
        MemberId member) const noexcept;
    const PropertyChangeNotificationRegistration*
    FindPropertyChangeNotification(TypeId type) const noexcept;
    const CollectionChangeNotificationRegistration*
    FindCollectionChangeNotification(TypeId type) const noexcept;

    TypeRegistry* types_ = nullptr;
    Base::Vector<TypeFactoryRegistration> typeFactories_;
    Base::Vector<ContentAccessorRegistration> contentAccessors_;
    Base::Vector<PropertyAccessorRegistration> propertyAccessors_;
    Base::Vector<ValueMemberAccessorRegistration> valueMemberAccessors_;
    Base::Vector<MethodInvokerRegistration> methodInvokers_;
    Base::Vector<PropertyChangeNotificationRegistration>
        propertyChangeNotifications_;
    Base::Vector<CollectionChangeNotificationRegistration>
        collectionChangeNotifications_;
    Base::Vector<OwnedBehaviorContext> ownedContexts_;
    bool frozen_ = false;
};

// Explicit mutable view for structural and executable type registration.
// Read-only consumers continue to use TypeRegistry directly.
class MetadataRegistrationTypes final {
public:
    MetadataRegistrationTypes(
        TypeRegistry& types,
        MetadataBehaviorRegistrationStore& behaviors) noexcept
        : types_(&types), behaviors_(&behaviors) {}

    Base::Result<TypeId> TryRegisterType(
        const TypeRegistration& registration) const noexcept;
    Base::Result<void> TryRegisterInterface(
        TypeId ownerType,
        TypeId interfaceType) const noexcept;
    Base::Result<MemberId> TryRegisterProperty(
        TypeId ownerType,
        const PropertyRegistration& registration) const noexcept;
    Base::Result<MemberId> TryRegisterField(
        TypeId ownerType,
        const FieldRegistration& registration) const noexcept;
    Base::Result<MemberId> TryRegisterEnumValue(
        TypeId ownerType,
        const EnumValueRegistration& registration) const noexcept;
    Base::Result<MemberId> TryRegisterEvent(
        TypeId ownerType,
        const EventRegistration& registration) const noexcept;
    Base::Result<MemberId> TryRegisterMethod(
        TypeId ownerType,
        const MethodRegistration& registration) const noexcept;
    Base::Result<void> TrySetFactory(
        TypeId type,
        ObjectFactory factory) const noexcept;
    Base::Result<void> TrySetContentMember(
        TypeId type,
        MemberId member) const noexcept;
    Base::Result<void> TrySetContentAccessor(
        const ContentAccessorRegistration& registration) const noexcept;
    Base::Result<void> TryRegisterPropertyChangeNotification(
        const PropertyChangeNotificationRegistration& registration)
        const noexcept;
    Base::Result<void> TryRegisterCollectionChangeNotification(
        const CollectionChangeNotificationRegistration& registration)
        const noexcept;

    TypeRegistry& Registry() const noexcept { return *types_; }
    MetadataBehaviorRegistrationStore& Behaviors() const noexcept {
        return *behaviors_;
    }

private:
    template<class>
    friend class TypeDescription;

    template<class TContext>
    Base::Result<std::decay_t<TContext>*> TryOwnBehaviorContext(
        TContext&& value) const noexcept {
        return behaviors_->TryOwnContext(
            std::forward<TContext>(value));
    }
    void ReleaseLastBehaviorContext(void* value) const noexcept {
        behaviors_->ReleaseLastContext(value);
    }

    Base::Result<void> ValidateRegistrationPair() const noexcept;

    TypeRegistry* types_ = nullptr;
    MetadataBehaviorRegistrationStore* behaviors_ = nullptr;
};

} // namespace Aero::Core
