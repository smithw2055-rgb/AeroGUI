#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Metadata/Value.hpp>

namespace Aero::Core {

class MetadataContext;
class MetadataRegistrationValues;
#if !defined(AERO_SDK_SURFACE_ONLY)
class TypeRegistry;
#endif
class ValueTypeSemantics;
struct TextValueConverterRegistration;
struct ValueTypeRegistration;

namespace Detail {
AERO_API Base::Result<Value> CreateRegistrationValue(
    void* registrationState,
    TypeId type,
    const void* source) noexcept;
AERO_API MetadataRegistrationValues MakeRegistrationValues(
    void* registrationState) noexcept;
}

// Opaque callback-scoped value registration view used by ValueCodec. The
// backing registration store remains a Core implementation detail.
class AERO_API MetadataRegistrationValues final {
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
#if !defined(AERO_SDK_SURFACE_ONLY)
    const TypeRegistry& Types() const noexcept;
#endif

private:
    friend class MetadataContext;
    friend Base::Result<Value> Detail::CreateRegistrationValue(
        void* registrationState,
        TypeId type,
        const void* source) noexcept;
    friend MetadataRegistrationValues
    Detail::MakeRegistrationValues(
        void* registrationState) noexcept;

    MetadataRegistrationValues(
        const void* registrations,
        void* mutableRegistrations) noexcept
        : registrations_(registrations),
          mutableRegistrations_(mutableRegistrations) {}

    const void* registrations_ = nullptr;
    void* mutableRegistrations_ = nullptr;
};

} // namespace Aero::Core
