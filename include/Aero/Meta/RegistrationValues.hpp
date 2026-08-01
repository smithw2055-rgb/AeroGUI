#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Meta/Value.hpp>

namespace Aero::Core {

class MetaRegistration;
class RegistrationValues;
class TypeRegistry;
class ValueTypeSemantics;
struct TextValueConverterRegistration;
struct ValueTypeRegistration;

namespace Detail {
AERO_API Base::Result<Value> CreateRegistrationValue(
    void* registrationState,
    TypeId type,
    const void* source) noexcept;
AERO_API RegistrationValues MakeRegistrationValues(
    void* registrationState) noexcept;
}

// Opaque callback-scoped value registration view used by ValueCodec. The
// backing registration store remains a Core implementation detail.
class AERO_API RegistrationValues final {
public:
    Base::Result<void> TryRegisterValueSemantics(
        TypeId type,
        const ValueTypeRegistration& registration) const noexcept;
    Base::Result<void> TryRegisterTextConverter(
        const TextValueConverterRegistration& registration) const noexcept;
    Base::Result<Value> TryCreateValue(
        TypeId type,
        const void* source) const noexcept;
    Base::Result<Value> TryConvertText(
        TypeId type,
        Base::StringView text) const noexcept;

    const Base::Ref<ValueTypeSemantics>* FindValueSemantics(
        TypeId type) const noexcept;
    const TextValueConverterRegistration* FindTextConverter(
        TypeId type) const noexcept;
    bool IsFrozen() const noexcept;
    const TypeRegistry& Types() const noexcept;

private:
    friend class MetaRegistration;
    friend Base::Result<Value> Detail::CreateRegistrationValue(
        void* registrationState,
        TypeId type,
        const void* source) noexcept;
    friend RegistrationValues
    Detail::MakeRegistrationValues(
        void* registrationState) noexcept;

    RegistrationValues(
        const void* registrations,
        void* mutableRegistrations) noexcept
        : registrations_(registrations),
          mutableRegistrations_(mutableRegistrations) {}

    const void* registrations_ = nullptr;
    void* mutableRegistrations_ = nullptr;
};

} // namespace Aero::Core
