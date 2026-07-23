#include <Aero/Core/TypeRegistry.hpp>

#include <utility>

namespace Aero::Core {
namespace {

Base::Status BehaviorMaterializationStatus(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

bool HasPropertyBehavior(const PropertyInfo& property) noexcept {
    return property.Access() != PropertyAccessKind::External ||
        property.Getter() != nullptr || property.Setter() != nullptr ||
        property.Provider() != InvalidPropertyProviderId ||
        property.Context() != nullptr;
}

} // namespace

Base::Result<void> TypeRegistry::MaterializeBehaviorRecords() noexcept {
    if (behaviorRecordsMaterialized_) return {};
    if (!frozen_) {
        return BehaviorMaterializationStatus(
            "TypeRegistry must be frozen before behavior materialization");
    }

    std::uint32_t factoryCount = 0U;
    std::uint32_t accessorCount = 0U;
    std::uint32_t invokerCount = 0U;
    for (const TypeInfo& type : types_) {
        if (type.factory_ != nullptr) ++factoryCount;
        for (const PropertyInfo& property : type.properties_) {
            if (HasPropertyBehavior(property)) ++accessorCount;
        }
        for (const MethodInfo& method : type.methods_) {
            if (method.invoke_ != nullptr) ++invokerCount;
        }
    }

    Base::Vector<TypeFactoryRegistration> factories;
    Base::Vector<PropertyAccessorRegistration> accessors;
    Base::Vector<MethodInvokerRegistration> invokers;
    Base::Result<void> result = factories.TryReserve(factoryCount);
    if (!result) return result.GetStatus();
    result = accessors.TryReserve(accessorCount);
    if (!result) return result.GetStatus();
    result = invokers.TryReserve(invokerCount);
    if (!result) return result.GetStatus();

    for (const TypeInfo& type : types_) {
        if (type.factory_ != nullptr) {
            result = factories.TryPushBack({type.id_, type.factory_});
            if (!result) return result.GetStatus();
        }
        for (const PropertyInfo& property : type.properties_) {
            if (!HasPropertyBehavior(property)) continue;
            result = accessors.TryPushBack({
                property.id_, property.access_, property.get_, property.set_,
                property.provider_, property.context_});
            if (!result) return result.GetStatus();
        }
        for (const MethodInfo& method : type.methods_) {
            if (method.invoke_ == nullptr) continue;
            result = invokers.TryPushBack(
                {method.id_, method.invoke_, method.context_});
            if (!result) return result.GetStatus();
        }
    }

    typeFactories_ = std::move(factories);
    propertyAccessors_ = std::move(accessors);
    methodInvokers_ = std::move(invokers);

    // From this point the records are the only active behavior storage. The
    // staging fields remain only to preserve the registration ABI until the
    // next cleanup slice physically removes them.
    for (TypeInfo& type : types_) {
        type.registry_ = this;
        type.factory_ = nullptr;
        for (PropertyInfo& property : type.properties_) {
            property.registry_ = this;
            property.access_ = PropertyAccessKind::External;
            property.get_ = nullptr;
            property.set_ = nullptr;
            property.provider_ = InvalidPropertyProviderId;
            property.context_ = nullptr;
        }
        for (MethodInfo& method : type.methods_) {
            method.registry_ = this;
            method.invoke_ = nullptr;
            method.context_ = nullptr;
        }
    }

    behaviorRecordsMaterialized_ = true;
    return {};
}

const TypeFactoryRegistration* TypeRegistry::FindTypeFactory(
    TypeId type) const noexcept {
    for (const TypeFactoryRegistration& registration : typeFactories_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const PropertyAccessorRegistration* TypeRegistry::FindPropertyAccessor(
    MemberId member) const noexcept {
    for (const PropertyAccessorRegistration& registration : propertyAccessors_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const MethodInvokerRegistration* TypeRegistry::FindMethodInvoker(
    MemberId member) const noexcept {
    for (const MethodInvokerRegistration& registration : methodInvokers_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

ObjectFactory TypeInfo::Factory() const noexcept {
    if (registry_ == nullptr) return factory_;
    const TypeFactoryRegistration* registration =
        registry_->FindTypeFactory(id_);
    return registration != nullptr ? registration->factory : nullptr;
}

PropertyAccessKind PropertyInfo::Access() const noexcept {
    if (registry_ == nullptr) return access_;
    const PropertyAccessorRegistration* registration =
        registry_->FindPropertyAccessor(id_);
    return registration != nullptr
        ? registration->access : PropertyAccessKind::External;
}

PropertyGetCallback PropertyInfo::Getter() const noexcept {
    if (registry_ == nullptr) return get_;
    const PropertyAccessorRegistration* registration =
        registry_->FindPropertyAccessor(id_);
    return registration != nullptr ? registration->get : nullptr;
}

PropertySetCallback PropertyInfo::Setter() const noexcept {
    if (registry_ == nullptr) return set_;
    const PropertyAccessorRegistration* registration =
        registry_->FindPropertyAccessor(id_);
    return registration != nullptr ? registration->set : nullptr;
}

PropertyProviderId PropertyInfo::Provider() const noexcept {
    if (registry_ == nullptr) return provider_;
    const PropertyAccessorRegistration* registration =
        registry_->FindPropertyAccessor(id_);
    return registration != nullptr
        ? registration->provider : InvalidPropertyProviderId;
}

void* PropertyInfo::Context() const noexcept {
    if (registry_ == nullptr) return context_;
    const PropertyAccessorRegistration* registration =
        registry_->FindPropertyAccessor(id_);
    return registration != nullptr ? registration->context : nullptr;
}

MethodInvokeCallback MethodInfo::Invoker() const noexcept {
    if (registry_ == nullptr) return invoke_;
    const MethodInvokerRegistration* registration =
        registry_->FindMethodInvoker(id_);
    return registration != nullptr ? registration->invoke : nullptr;
}

void* MethodInfo::Context() const noexcept {
    if (registry_ == nullptr) return context_;
    const MethodInvokerRegistration* registration =
        registry_->FindMethodInvoker(id_);
    return registration != nullptr ? registration->context : nullptr;
}

} // namespace Aero::Core
