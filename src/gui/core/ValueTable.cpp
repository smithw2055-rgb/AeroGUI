#include "gui/core/TypeRegistryCore.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Meta.hpp>

#include <cstddef>
#include <utility>

namespace Aero::Meta {
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

Base::Result<Value> CreateRegistrationValue(
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

RegistrationValues MakeRegistrationValues(
    void* registrationState) noexcept {
    return RegistrationValues(
        registrationState, registrationState);
}

Base::Result<void>
RegistrationValues::RegisterValueSemantics(
    TypeId type,
    const ValueTypeRegistration& registration) const noexcept {
    ValueTable* registrations =
        MutableStore(mutableRegistrations_);
    if (registrations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata registration values are read-only");
    }
    return registrations->RegisterValueSemantics(
        type, registration);
}

Base::Result<void>
RegistrationValues::RegisterTextConverter(
    const TextValueConverterRegistration& registration) const noexcept {
    ValueTable* registrations =
        MutableStore(mutableRegistrations_);
    if (registrations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata registration values are read-only");
    }
    return registrations->RegisterTextConverter(registration);
}

Base::Result<Value> RegistrationValues::TryCreateValue(
    TypeId type,
    const void* source) const noexcept {
    const ValueTypeRegistration* semantics =
        FindValueSemantics(type);
    if (semantics == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Value type semantics are not registered");
    }
    return Value::TryFromCustom(type, source, semantics);
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

const ValueTypeRegistration*
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

} // namespace Aero::Meta

namespace Aero::Meta {
namespace {

Base::Status FrozenStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Metadata value registration store is frozen");
}

bool IsValueType(const TypeInfo& type) noexcept {
    return (static_cast<std::uint32_t>(type.Flags()) &
        static_cast<std::uint32_t>(TypeFlags::ValueType)) != 0U;
}

} // namespace

Base::Result<void> ValueTable::RegisterValueSemantics(
    TypeId type,
    const ValueTypeRegistration& registration) noexcept {
    if (frozen_) return FrozenStatus();
    const TypeInfo* info = types_ != nullptr ? types_->FindType(type) : nullptr;
    if (info == nullptr || !IsValueType(*info) ||
        registration.size == 0U || registration.alignment == 0U ||
        !Base::IsValidAlignment(registration.alignment) ||
        registration.equals == nullptr ||
        (registration.inlineSafe &&
            (registration.size > Value::InlineCapacity ||
             registration.alignment > alignof(std::max_align_t))) ||
        (!registration.inlineSafe && registration.copy == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Value type semantics are invalid");
    }
    if (FindValueSemantics(type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Value type semantics are already registered");
    }
    auto* entry = new (std::nothrow) ValueSemanticsEntry{type, registration};
    if (entry == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Value type semantics allocation failed");
    }
    valueSemantics_.PushBack(entry);
    return {};
}

Base::Result<void> ValueTable::RegisterTextConverter(
    const TextValueConverterRegistration& registration) noexcept {
    if (frozen_) return FrozenStatus();
    if (registration.type == InvalidTypeId || registration.convert == nullptr ||
        types_ == nullptr || types_->FindType(registration.type) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text value converter registration is invalid");
    }
    if (FindTextConverter(registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Text value converter is already registered");
    }
    textConverters_.PushBack(registration);
    return {};
}

Base::Result<void> ValueTable::Freeze() noexcept {
    if (frozen_) return {};
    if (types_ == nullptr || !types_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TypeRegistry must be frozen before value registrations");
    }
    frozen_ = true;
    return {};
}

const ValueTypeRegistration*
ValueTable::FindValueSemantics(TypeId type) const noexcept {
    for (const ValueSemanticsEntry* entry : valueSemantics_) {
        if (entry != nullptr && entry->type == type) return &entry->registration;
    }
    return nullptr;
}

const TextValueConverterRegistration*
ValueTable::FindTextConverter(TypeId type) const noexcept {
    for (const TextValueConverterRegistration& entry : textConverters_) {
        if (entry.type == type) return &entry;
    }
    return nullptr;
}

} // namespace Aero::Meta
