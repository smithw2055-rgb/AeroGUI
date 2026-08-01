#include <Aero/Meta/RegistrationValues.hpp>

#include "ValueTable.hpp"

namespace Aero::Core {
namespace {

const ValueTable& Store(
    const void* value) noexcept {
    return *static_cast<
        const ValueTable*>(value);
}

ValueTable* MutableStore(
    void* value) noexcept {
    return static_cast<ValueTable*>(value);
}

} // namespace

Base::Result<Value> Detail::CreateRegistrationValue(
    void* registrationState,
    TypeId type,
    const void* source) noexcept {
    ValueTable* registrations =
        MutableStore(registrationState);
    if (registrations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Value registration state is unavailable");
    }
    RegistrationValues values(
        registrations, registrations);
    return values.TryCreateValue(type, source);
}

RegistrationValues Detail::MakeRegistrationValues(
    void* registrationState) noexcept {
    return RegistrationValues(
        registrationState, registrationState);
}

Base::Result<void>
RegistrationValues::TryRegisterValueSemantics(
    TypeId type,
    const ValueTypeRegistration& registration) const noexcept {
    ValueTable* registrations =
        MutableStore(mutableRegistrations_);
    if (registrations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata registration values are read-only");
    }
    return registrations->TryRegisterValueSemantics(
        type, registration);
}

Base::Result<void>
RegistrationValues::TryRegisterTextConverter(
    const TextValueConverterRegistration& registration) const noexcept {
    ValueTable* registrations =
        MutableStore(mutableRegistrations_);
    if (registrations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata registration values are read-only");
    }
    return registrations->TryRegisterTextConverter(registration);
}

Base::Result<Value> RegistrationValues::TryCreateValue(
    TypeId type,
    const void* source) const noexcept {
    const Base::Ref<ValueTypeSemantics>* semantics =
        FindValueSemantics(type);
    if (semantics == nullptr || !*semantics) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Value type semantics are not registered");
    }
    return Value::TryFromCustom(type, source, *semantics);
}

Base::Result<Value> RegistrationValues::TryConvertText(
    TypeId type,
    Base::StringView text) const noexcept {
    const TextValueConverterRegistration* converter =
        FindTextConverter(type);
    if (converter == nullptr || converter->convert == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Text value converter is not registered");
    }
    Base::Result<Value> converted = converter->convert(
        type, text, converter->context);
    if (!converted) return converted.GetStatus();
    if (converted.Value().IsUnset() ||
        converted.Value().Type() != type) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text converter returned an incompatible value");
    }
    return converted;
}

const Base::Ref<ValueTypeSemantics>*
RegistrationValues::FindValueSemantics(
    TypeId type) const noexcept {
    return registrations_ != nullptr
        ? Store(registrations_).FindValueSemantics(type)
        : nullptr;
}

const TextValueConverterRegistration*
RegistrationValues::FindTextConverter(
    TypeId type) const noexcept {
    return registrations_ != nullptr
        ? Store(registrations_).FindTextConverter(type)
        : nullptr;
}

bool RegistrationValues::IsFrozen() const noexcept {
    return registrations_ != nullptr &&
        Store(registrations_).IsFrozen();
}

const TypeRegistry& RegistrationValues::Types() const noexcept {
    return Store(registrations_).Types();
}

} // namespace Aero::Core
