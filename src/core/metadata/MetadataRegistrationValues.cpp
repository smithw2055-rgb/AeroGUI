#include <Aero/Core/Metadata/MetadataRegistrationValues.hpp>

#include <Aero/Core/Metadata/MetadataValueRegistrationStore.hpp>

namespace Aero::Core {
namespace {

const MetadataValueRegistrationStore& Store(
    const void* value) noexcept {
    return *static_cast<
        const MetadataValueRegistrationStore*>(value);
}

MetadataValueRegistrationStore* MutableStore(
    void* value) noexcept {
    return static_cast<MetadataValueRegistrationStore*>(value);
}

} // namespace

Base::Result<Value> Detail::CreateRegistrationValue(
    void* registrationState,
    TypeId type,
    const void* source) noexcept {
    MetadataValueRegistrationStore* registrations =
        MutableStore(registrationState);
    if (registrations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Value registration state is unavailable");
    }
    MetadataRegistrationValues values(
        registrations, registrations);
    return values.TryCreateValue(type, source);
}

MetadataRegistrationValues Detail::MakeRegistrationValues(
    void* registrationState) noexcept {
    return MetadataRegistrationValues(
        registrationState, registrationState);
}

Base::Result<void>
MetadataRegistrationValues::TryRegisterValueSemantics(
    TypeId type,
    const ValueTypeRegistration& registration) const noexcept {
    MetadataValueRegistrationStore* registrations =
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
MetadataRegistrationValues::TryRegisterTextConverter(
    const TextValueConverterRegistration& registration) const noexcept {
    MetadataValueRegistrationStore* registrations =
        MutableStore(mutableRegistrations_);
    if (registrations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata registration values are read-only");
    }
    return registrations->TryRegisterTextConverter(registration);
}

Base::Result<Value> MetadataRegistrationValues::TryCreateValue(
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

Base::Result<Value> MetadataRegistrationValues::TryConvertText(
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
MetadataRegistrationValues::FindValueSemantics(
    TypeId type) const noexcept {
    return registrations_ != nullptr
        ? Store(registrations_).FindValueSemantics(type)
        : nullptr;
}

const TextValueConverterRegistration*
MetadataRegistrationValues::FindTextConverter(
    TypeId type) const noexcept {
    return registrations_ != nullptr
        ? Store(registrations_).FindTextConverter(type)
        : nullptr;
}

bool MetadataRegistrationValues::IsFrozen() const noexcept {
    return registrations_ != nullptr &&
        Store(registrations_).IsFrozen();
}

const TypeRegistry& MetadataRegistrationValues::Types() const noexcept {
    return Store(registrations_).Types();
}

} // namespace Aero::Core
