// Consolidated implementation. Keep sections ordered by dependency.

// ===== Value =====

#include <Aero/Value.hpp>

#include <Aero/Base/String.hpp>

#include <cstring>
#include <utility>

namespace Aero::Base {
namespace {

class StringValueStorage : public Base::Object {
public:
    explicit StringValueStorage(Base::String&& value) noexcept : value_(std::move(value)) {}
    Base::StringView View() const noexcept { return value_.View(); }
private:
    Base::String value_;
};

class CustomValueStorage : public Base::Object {
public:
    CustomValueStorage(void* value, Base::IAllocator& allocator, const Base::Ref<ValueTypeSemantics>& semantics) noexcept
        : value_(value), allocator_(&allocator), semantics_(semantics) {}
    ~CustomValueStorage() override {
        const ValueTypeRegistration& registration = semantics_->Registration();
        if (registration.destroy != nullptr) registration.destroy(value_, registration.context);
        allocator_->Deallocate(value_, registration.size, registration.alignment, Base::MemoryTag::Ui);
    }
    const void* Data() const noexcept { return value_; }
    void* MutableData() noexcept { return value_; }
private:
    void* value_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Ref<ValueTypeSemantics> semantics_;
};

template<class T> void StoreScalar(unsigned char* destination, T value) noexcept {
    static_assert(sizeof(T) <= Value::InlineCapacity, "scalar is too large");
    std::memcpy(destination, &value, sizeof(T));
}
template<class T> T LoadScalar(const unsigned char* source) noexcept {
    T value{}; std::memcpy(&value, source, sizeof(T)); return value;
}

} // namespace

Value Value::Unset() noexcept { return {}; }
Value Value::FromBoolean(TypeId type, bool value) noexcept { Value result; result.type_ = type; result.kind_ = ValueKind::Boolean; StoreScalar(result.inlineData_, value); return result; }
Value Value::FromSignedInteger(TypeId type, std::int64_t value) noexcept { Value result; result.type_ = type; result.kind_ = ValueKind::SignedInteger; StoreScalar(result.inlineData_, value); return result; }
Value Value::FromUnsignedInteger(TypeId type, std::uint64_t value) noexcept { Value result; result.type_ = type; result.kind_ = ValueKind::UnsignedInteger; StoreScalar(result.inlineData_, value); return result; }
Value Value::FromDouble(TypeId type, double value) noexcept { Value result; result.type_ = type; result.kind_ = ValueKind::Double; StoreScalar(result.inlineData_, value); return result; }

Base::Result<Value> Value::TryFromString(TypeId type, Base::StringView value) noexcept {
    Base::IAllocator& selected = Base::GetDefaultAllocator();
    Base::String text(&selected);
    Base::Result<void> assigned = text.Assign(value);
    if (!assigned) return assigned.GetStatus();
    Base::Result<Base::Ref<StringValueStorage>> storage = Base::MakeRefWithAllocator<StringValueStorage>(selected, std::move(text));
    if (!storage) return storage.GetStatus();
    Value result; result.type_ = type; result.kind_ = ValueKind::String; result.storage_ = std::move(storage).Value(); return result;
}

Value Value::FromObject(TypeId type, Base::Ref<Base::Object> value) noexcept { Value result; result.type_ = type; result.kind_ = ValueKind::Object; result.storage_ = std::move(value); return result; }
Value Value::NullObject(TypeId type) noexcept { return FromObject(type, {}); }

Base::Result<Value> Value::TryFromCustom(TypeId type, const void* source, const Base::Ref<ValueTypeSemantics>& semantics) noexcept {
    if (type == InvalidTypeId || source == nullptr || !semantics) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Custom Value construction is incomplete");
    const ValueTypeRegistration& registration = semantics->Registration();
    if (registration.size == 0U || registration.alignment == 0U) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Custom Value semantics are invalid");
    Value result; result.type_ = type; result.kind_ = ValueKind::Custom; result.semantics_ = semantics;
    if (registration.inlineSafe && registration.size <= InlineCapacity && registration.alignment <= alignof(std::max_align_t)) {
        std::memcpy(result.inlineData_, source, registration.size); result.inlineCustom_ = true; return result;
    }
    if (registration.copy == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Boxed custom Value requires a copy callback");
    Base::IAllocator& selected = Base::GetDefaultAllocator();
    void* memory = selected.Allocate({registration.size, registration.alignment, Base::MemoryTag::Ui});
    if (memory == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Unable to allocate custom Value storage");
    Base::Result<void> copied = registration.copy(memory, source, registration.context);
    if (!copied) { selected.Deallocate(memory, registration.size, registration.alignment, Base::MemoryTag::Ui); return copied.GetStatus(); }
    Base::Result<Base::Ref<CustomValueStorage>> storage = Base::MakeRefWithAllocator<CustomValueStorage>(selected, memory, selected, semantics);
    if (!storage) {
        if (registration.destroy != nullptr) registration.destroy(memory, registration.context);
        selected.Deallocate(memory, registration.size, registration.alignment, Base::MemoryTag::Ui);
        return storage.GetStatus();
    }
    result.storage_ = std::move(storage).Value(); return result;
}

bool Value::AsBoolean() const noexcept { AERO_ASSERT(kind_ == ValueKind::Boolean); return LoadScalar<bool>(inlineData_); }
std::int64_t Value::AsSignedInteger() const noexcept { AERO_ASSERT(kind_ == ValueKind::SignedInteger); return LoadScalar<std::int64_t>(inlineData_); }
std::uint64_t Value::AsUnsignedInteger() const noexcept { AERO_ASSERT(kind_ == ValueKind::UnsignedInteger); return LoadScalar<std::uint64_t>(inlineData_); }
double Value::AsDouble() const noexcept { AERO_ASSERT(kind_ == ValueKind::Double); return LoadScalar<double>(inlineData_); }
Base::StringView Value::AsString() const noexcept { AERO_ASSERT(kind_ == ValueKind::String && storage_); return static_cast<const StringValueStorage*>(storage_.Get())->View(); }
const Base::Ref<Base::Object>& Value::AsObject() const noexcept { AERO_ASSERT(kind_ == ValueKind::Object); return storage_; }
const void* Value::AsCustom() const noexcept { AERO_ASSERT(kind_ == ValueKind::Custom); return inlineCustom_ ? static_cast<const void*>(inlineData_) : static_cast<const CustomValueStorage*>(storage_.Get())->Data(); }
void* Value::MutableCustom() noexcept { AERO_ASSERT(kind_ == ValueKind::Custom); return inlineCustom_ ? static_cast<void*>(inlineData_) : static_cast<CustomValueStorage*>(storage_.Get())->MutableData(); }

bool Value::Equals(const Value& other) const noexcept {
    if (type_ != other.type_ || kind_ != other.kind_) return false;
    switch (kind_) {
    case ValueKind::Unset: return true;
    case ValueKind::Boolean: return AsBoolean() == other.AsBoolean();
    case ValueKind::SignedInteger: return AsSignedInteger() == other.AsSignedInteger();
    case ValueKind::UnsignedInteger: return AsUnsignedInteger() == other.AsUnsignedInteger();
    case ValueKind::Double: {
        std::uint64_t left = 0U, right = 0U; const double leftValue = AsDouble(), rightValue = other.AsDouble();
        std::memcpy(&left, &leftValue, sizeof(left)); std::memcpy(&right, &rightValue, sizeof(right)); return left == right;
    }
    case ValueKind::String: return AsString() == other.AsString();
    case ValueKind::Object: return storage_.Get() == other.storage_.Get();
    case ValueKind::Custom: {
        if (!semantics_ || !other.semantics_) return false;
        const ValueTypeRegistration& registration = semantics_->Registration();
        if (registration.equals != nullptr) return registration.equals(AsCustom(), other.AsCustom(), registration.context);
        return registration.inlineSafe && std::memcmp(AsCustom(), other.AsCustom(), registration.size) == 0;
    }
    }
    return false;
}

} // namespace Aero::Base


// ===== ValueConversion =====

#include <Aero/Value.hpp>

#include <cctype>

namespace Aero::Base::Detail::ValueConversion {

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

} // namespace Aero::Base::Detail::ValueConversion


// ===== RegistrationValues =====

#include <Aero/Meta.hpp>

#include "MetadataInternal.hpp"

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

#include "MetadataInternal.hpp"

#include <Aero/Base/Allocator.hpp>

#include <cstddef>
#include <utility>

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
