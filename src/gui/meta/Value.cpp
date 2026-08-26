// Consolidated implementation. Value lives in AeroBase (src/base/Value.cpp).
// Codecs, TypeId helpers and registration tables stay in Meta / AeroGui.

#include <Aero/Value.hpp>
#include <Aero/Base/String.hpp>
#include "gui/meta/ValueConversion.hpp"

// ===== ValueConversion =====

#include <cctype>

namespace Aero::Base::ValueConversion {

Base::StringView Trim(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1U]))) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

bool EqualsAsciiInsensitive(
    Base::StringView left,
    Base::StringView right) noexcept {
    if (left.SizeBytes() != right.SizeBytes()) return false;
    for (std::uint32_t index = 0U;
         index < left.SizeBytes(); ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index])) !=
            std::tolower(static_cast<unsigned char>(right[index]))) {
            return false;
        }
    }
    return true;
}

Base::Result<double> ParseDouble(
    Base::StringView text) noexcept {
    Base::String buffer;
    Base::Result<void> assigned =
        buffer.Assign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    const double value =
        std::strtod(buffer.CStr(), &end);
    if (end == buffer.CStr() || *end != '\0' ||
        !std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Text is not a finite number");
    }
    return value;
}

Base::Result<bool> ConvertBoolean(
    Base::StringView text) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "true")) {
        return true;
    }
    if (EqualsAsciiInsensitive(value, "false")) {
        return false;
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Boolean text must be true or false");
}

Base::Result<::Aero::Nullable<bool>> ConvertNullableBoolean(
    Base::StringView text) noexcept {
    const Base::StringView value = Trim(text);
    if (value.Empty() || EqualsAsciiInsensitive(value, "null")) {
        return ::Aero::Nullable<bool>{};
    }
    Base::Result<bool> converted = ConvertBoolean(value);
    if (!converted) return converted.GetStatus();
    return ::Aero::Nullable<bool>{converted.Value()};
}

Base::Result<double> ConvertDouble(
    Base::StringView text) noexcept {
    return ParseDouble(text);
}

Base::Result<Base::String> ConvertString(
    Base::StringView text) noexcept {
    Base::String value;
    Base::Result<void> assigned =
        value.Assign(text);
    if (!assigned) return assigned.GetStatus();
    return value;
}

Base::Result<Base::ResourceUri> ConvertResourceUri(
    Base::StringView text) noexcept {
    return Base::ResourceUri::Parse(Trim(text));
}

} // namespace Aero::Base::ValueConversion

// ===== RegistrationValues =====

#include <Aero/Meta.hpp>

#include "gui/meta/MetadataState.hpp"
#include "gui/media/AnimationEngine.hpp"

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

} // namespace Aero::Meta

// ===== ValueTable =====

#include <Aero/Base/Allocator.hpp>

#include <cstddef>

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
    Base::Result<Base::Ref<ValueTypeSemantics>> created =
        Base::MakeRef<ValueTypeSemantics>(registration);
    if (!created) return created.GetStatus();
    return valueSemantics_.PushBack({type, std::move(created).Value()});
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
    return textConverters_.PushBack(registration);
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

const Base::Ref<ValueTypeSemantics>*
ValueTable::FindValueSemantics(TypeId type) const noexcept {
    for (const ValueSemanticsEntry& entry : valueSemantics_) {
        if (entry.type == type) return &entry.semantics;
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
