#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/TypeRegistry.hpp>

namespace Aero::Core {

class DependencyPropertyRegistry;
class MetadataRuntime;

using PropertyProviderGetCallback = Base::Result<Value> (*)(
    const Base::Object& object,
    const PropertyInfo& property,
    void* context) noexcept;
using PropertyProviderSetCallback = Base::Result<void> (*)(
    Base::Object& object,
    const PropertyInfo& property,
    const Value& value,
    void* context) noexcept;

struct PropertyProviderRegistration final {
    PropertyProviderId id = InvalidPropertyProviderId;
    TypeId objectType = InvalidTypeId;
    PropertyProviderGetCallback get = nullptr;
    PropertyProviderSetCallback set = nullptr;
    void* context = nullptr;
};

class AERO_API MemberAccessor final {
public:
    explicit MemberAccessor(TypeRegistry& types) noexcept;

    Base::Result<void> UseRuntime(MetadataRuntime& runtime) noexcept;
    Base::Result<void> TryRegisterProvider(
        const PropertyProviderRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;

    Base::Result<Value> GetProperty(
        const Base::Object& object,
        const PropertyInfo& property) const noexcept;
    Base::Result<void> SetProperty(
        Base::Object& object,
        const PropertyInfo& property,
        const Value& value) const noexcept;
    Base::Result<Value> InvokeMethod(
        Base::Object& object,
        const MethodInfo& method,
        Base::Span<const Value> arguments) const noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    bool UsesRuntime() const noexcept { return runtime_ != nullptr; }
    TypeRegistry& Types() const noexcept { return *types_; }

private:
    TypeRegistry* types_ = nullptr;
    MetadataRuntime* runtime_ = nullptr;
    Base::Vector<PropertyProviderRegistration> providers_;
    bool frozen_ = false;

    const PropertyProviderRegistration* FindProvider(
        PropertyProviderId id) const noexcept;
    Base::Result<void> ValidateTarget(
        const Base::Object& object,
        TypeId ownerType) const noexcept;
};

AERO_API Base::Result<void>
TryRegisterDependencyPropertyProvider(
    MemberAccessor& accessor,
    DependencyPropertyRegistry& properties,
    TypeId dependencyObjectType) noexcept;

} // namespace Aero::Core
