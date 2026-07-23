#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/TypeRegistry.hpp>

#include <cstdint>

namespace Aero::Core {

class DependencyPropertyRegistry;
class Dispatcher;

inline constexpr std::uint32_t ObjectActivationAbiVersion = 2U;

struct ObjectActivationContext final {
    std::uint32_t structSize = 0U;
    std::uint32_t abiVersion = ObjectActivationAbiVersion;
    Dispatcher* dispatcher = nullptr;
    DependencyPropertyRegistry* dependencyProperties = nullptr;
    void* applicationServices = nullptr;
    void* hostContext = nullptr;

    static ObjectActivationContext Create() noexcept {
        ObjectActivationContext context;
        context.structSize = static_cast<std::uint32_t>(
            sizeof(ObjectActivationContext));
        return context;
    }

    bool IsCompatible() const noexcept {
        return structSize >= static_cast<std::uint32_t>(
                   sizeof(ObjectActivationContext)) &&
            abiVersion == ObjectActivationAbiVersion;
    }
};

using ObjectActivateCallback = Base::Result<Base::Ref<Base::Object>> (*)(
    TypeId requestedType,
    const ObjectActivationContext& activation,
    void* context) noexcept;

struct ObjectActivationProviderRegistration final {
    TypeId type = InvalidTypeId;
    ObjectActivateCallback activate = nullptr;
    void* context = nullptr;
};

// Shared activation registry for XAML, templates, compiled markup and direct
// host construction. A provider registered for a base metadata type may create
// any requested derived type and receives the exact requested TypeId.
class AERO_API ActivationProviderRegistry final {
public:
    explicit ActivationProviderRegistry(TypeRegistry& types) noexcept
        : types_(&types), providers_() {}

    ActivationProviderRegistry(const ActivationProviderRegistry&) = delete;
    ActivationProviderRegistry& operator=(
        const ActivationProviderRegistry&) = delete;

    Base::Result<void> TryRegister(
        const ObjectActivationProviderRegistration& registration) noexcept {
        if (frozen_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Activation provider registry is frozen");
        }
        const TypeInfo* type = types_->FindType(registration.type);
        if (type == nullptr || registration.activate == nullptr ||
            HasTypeFlag(type->Flags(), TypeFlags::ValueType)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Activation provider registration is invalid");
        }
        if (FindExact(registration.type) != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Activation provider is already registered");
        }
        return providers_.TryPushBack(registration);
    }

    Base::Result<void> Freeze() noexcept {
        if (frozen_) {
            return {};
        }
        if (!types_->IsFrozen()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "TypeRegistry must be frozen before activation providers");
        }
        frozen_ = true;
        return {};
    }

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t ProviderCount() const noexcept {
        return providers_.Size();
    }
    TypeRegistry& Types() const noexcept { return *types_; }

    const ObjectActivationProviderRegistration* Find(
        TypeId requestedType) const noexcept {
        TypeId current = requestedType;
        for (std::uint32_t depth = 0U;
             current != InvalidTypeId && depth <= types_->TypeCount();
             ++depth) {
            const ObjectActivationProviderRegistration* provider =
                FindExact(current);
            if (provider != nullptr) {
                return provider;
            }
            const TypeInfo* type = types_->FindType(current);
            if (type == nullptr) {
                return nullptr;
            }
            current = type->BaseType();
        }
        return nullptr;
    }

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        TypeId requestedType,
        const ObjectActivationContext& activation) const noexcept {
        if (!frozen_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Activation provider registry is not frozen");
        }
        if (!activation.IsCompatible()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Object activation context is incompatible");
        }
        if (types_->FindType(requestedType) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Requested activation type is not registered");
        }

        const ObjectActivationProviderRegistration* provider =
            Find(requestedType);
        if (provider == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "No activation provider applies to the requested type");
        }

        Base::Result<Base::Ref<Base::Object>> created = provider->activate(
            requestedType,
            activation,
            provider->context);
        if (!created) {
            return created.GetStatus();
        }
        if (!created.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Activation provider returned a null object");
        }

        // Presentation and custom-control objects report a runtime metadata
        // type and are checked strictly. Some established XAML helper objects,
        // such as Style and Setter construction records, intentionally derive
        // directly from Base::Object and let the explicit provider own their
        // metadata identity. Preserve that compatibility only when the object
        // reports no runtime type at all.
        const TypeId runtimeType = created.Value()->RuntimeType();
        if (runtimeType != InvalidTypeId && runtimeType != requestedType) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Activation provider returned an object with the wrong runtime type");
        }
        return created;
    }

private:
    static constexpr bool HasTypeFlag(
        TypeFlags value,
        TypeFlags flag) noexcept {
        return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
    }

    const ObjectActivationProviderRegistration* FindExact(
        TypeId type) const noexcept {
        for (const ObjectActivationProviderRegistration& provider : providers_) {
            if (provider.type == type) {
                return &provider;
            }
        }
        return nullptr;
    }

    TypeRegistry* types_ = nullptr;
    Base::Vector<ObjectActivationProviderRegistration> providers_;
    bool frozen_ = false;
};

} // namespace Aero::Core
