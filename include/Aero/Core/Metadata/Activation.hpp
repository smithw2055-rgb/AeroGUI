#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataDescriptors.hpp>

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

// Host activation is a runtime facet rather than static schema metadata. It is
// deliberately excluded from MetadataDomain schema hashes because callbacks and
// host contexts differ between applications even when the XAML schema matches.
struct ActivationFacet final {
    TypeId type = InvalidTypeId;
    ObjectActivateCallback activate = nullptr;
    void* context = nullptr;
};

// Source compatibility for registration sites. The registry immediately stores
// the supplied value as an ActivationFacet.
using ObjectActivationProviderRegistration = ActivationFacet;

// Shared activation-facet registry for XAML, templates, compiled markup and
// direct host construction. The sealed descriptor store is the authoritative
// type/inheritance source used to validate activation facets.
class AERO_API ActivationProviderRegistry final {
public:
    explicit ActivationProviderRegistry(
        const MetadataDescriptorStore& descriptors) noexcept
        : descriptors_(&descriptors),
          facets_() {}

    ActivationProviderRegistry(const ActivationProviderRegistry&) = delete;
    ActivationProviderRegistry& operator=(
        const ActivationProviderRegistry&) = delete;

    Base::Result<void> TryRegister(
        const ObjectActivationProviderRegistration& registration) noexcept {
        if (frozen_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Activation facet registry is frozen");
        }
        TypeFlags flags = TypeFlags::None;
        if (!TryGetTypeFlags(registration.type, flags) ||
            registration.activate == nullptr ||
            HasTypeFlag(flags, TypeFlags::ValueType)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Activation facet registration is invalid");
        }
        if (FindExact(registration.type) != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Activation facet is already registered");
        }
        return facets_.TryPushBack(registration);
    }

    Base::Result<void> Freeze() noexcept {
        if (frozen_) {
            return {};
        }
        if (!descriptors_->IsSealed()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Metadata descriptors must be sealed before activation facets");
        }
        frozen_ = true;
        return {};
    }

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t ProviderCount() const noexcept {
        return facets_.Size();
    }
    const MetadataDescriptorStore& Descriptors() const noexcept {
        return *descriptors_;
    }

    const ActivationFacet* Find(TypeId requestedType) const noexcept {
        TypeId current = requestedType;
        for (std::uint32_t depth = 0U;
             current != InvalidTypeId && depth <= TypeCount();
             ++depth) {
            const ActivationFacet* facet = FindExact(current);
            if (facet != nullptr) {
                return facet;
            }
            current = BaseTypeOf(current);
        }
        return nullptr;
    }

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        TypeId requestedType,
        const ObjectActivationContext& activation) const noexcept {
        if (!frozen_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Activation facet registry is not frozen");
        }
        if (!activation.IsCompatible()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Object activation context is incompatible");
        }
        TypeFlags flags = TypeFlags::None;
        if (!TryGetTypeFlags(requestedType, flags)) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Requested activation type is not registered");
        }

        const ActivationFacet* facet = Find(requestedType);
        if (facet == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "No activation facet applies to the requested type");
        }

        Base::Result<Base::Ref<Base::Object>> created = facet->activate(
            requestedType,
            activation,
            facet->context);
        if (!created) {
            return created.GetStatus();
        }
        if (!created.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Activation facet returned a null object");
        }

        const TypeId runtimeType = created.Value()->RuntimeType();
        if (runtimeType != InvalidTypeId && runtimeType != requestedType) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Activation facet returned an object with the wrong runtime type");
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

    bool TryGetTypeFlags(TypeId type, TypeFlags& flags) const noexcept {
        const MetadataTypeDescriptor* descriptor =
            descriptors_->FindType(type);
        if (descriptor == nullptr) return false;
        flags = descriptor->Flags();
        return true;
    }

    std::uint32_t TypeCount() const noexcept {
        return descriptors_->TypeCount();
    }

    TypeId BaseTypeOf(TypeId type) const noexcept {
        const MetadataTypeDescriptor* descriptor =
            descriptors_->FindType(type);
        return descriptor != nullptr
            ? descriptor->BaseType()
            : InvalidTypeId;
    }

    const ActivationFacet* FindExact(TypeId type) const noexcept {
        for (const ActivationFacet& facet : facets_) {
            if (facet.type == type) {
                return &facet;
            }
        }
        return nullptr;
    }

    const MetadataDescriptorStore* descriptors_ = nullptr;
    Base::Vector<ActivationFacet> facets_;
    bool frozen_ = false;
};

} // namespace Aero::Core
