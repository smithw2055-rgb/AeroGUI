#include <Aero/Base/Value.hpp>
#include <Aero/Base/Assert.hpp>
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
    CustomValueStorage(void* value, Base::IAllocator& allocator, const ValueTypeRegistration* semantics) noexcept
        : value_(value), allocator_(&allocator), semantics_(semantics) {}
    ~CustomValueStorage() override {
        if (semantics_ != nullptr) {
            const ValueTypeRegistration& registration = *semantics_;
            if (registration.destroy != nullptr) registration.destroy(value_, registration.context);
            allocator_->Deallocate(value_, registration.size, registration.alignment, Base::MemoryTag::Ui);
        }
    }
    const void* Data() const noexcept { return value_; }
    void* MutableData() noexcept { return value_; }
private:
    void* value_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    const ValueTypeRegistration* semantics_ = nullptr;
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
Value Value::FromBoolean(TypeId type, bool value) noexcept {
    Value result; result.type_ = type; result.kind_ = ValueKind::Boolean;
    StoreScalar(result.storage_.inlineData, value); return result;
}
Value Value::FromSignedInteger(TypeId type, std::int64_t value) noexcept {
    Value result; result.type_ = type; result.kind_ = ValueKind::SignedInteger;
    StoreScalar(result.storage_.inlineData, value); return result;
}
Value Value::FromUnsignedInteger(TypeId type, std::uint64_t value) noexcept {
    Value result; result.type_ = type; result.kind_ = ValueKind::UnsignedInteger;
    StoreScalar(result.storage_.inlineData, value); return result;
}
Value Value::FromDouble(TypeId type, double value) noexcept {
    Value result; result.type_ = type; result.kind_ = ValueKind::Double;
    StoreScalar(result.storage_.inlineData, value); return result;
}

Base::Result<Value> Value::TryFromString(TypeId type, Base::StringView value) noexcept {
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::String;

    constexpr std::uint32_t MaxShortStringLength = InlineCapacity - 1U;
    if (value.SizeBytes() <= MaxShortStringLength) {
        result.isShortString_ = true;
        result.stringLength_ = static_cast<std::uint8_t>(value.SizeBytes());
        if (value.SizeBytes() > 0U && value.Data() != nullptr) {
            std::memcpy(result.storage_.inlineData, value.Data(), value.SizeBytes());
        }
        result.storage_.inlineData[value.SizeBytes()] = '\0';
        return result;
    }

    Base::IAllocator& selected = Base::GetDefaultAllocator();
    Base::String text(&selected);
    Base::Result<void> assigned = text.Assign(value);
    if (!assigned) return assigned.GetStatus();
    Base::Result<Base::Ref<StringValueStorage>> storage =
        Base::MakeRefWithAllocator<StringValueStorage>(selected, std::move(text));
    if (!storage) return storage.GetStatus();

    result.isShortString_ = false;
    new (&result.storage_.storage) Base::Ref<Base::Object>(std::move(storage).Value());
    return result;
}

Base::Result<Value> Value::FromDynamicResourceKey(Base::StringView key) noexcept {
    Value result;
    result.type_ = InvalidTypeId;
    result.kind_ = ValueKind::DynamicResource;

    constexpr std::uint32_t MaxShortStringLength = InlineCapacity - 1U;
    if (key.SizeBytes() <= MaxShortStringLength) {
        result.isShortString_ = true;
        result.stringLength_ = static_cast<std::uint8_t>(key.SizeBytes());
        if (key.SizeBytes() > 0U && key.Data() != nullptr) {
            std::memcpy(result.storage_.inlineData, key.Data(), key.SizeBytes());
        }
        result.storage_.inlineData[key.SizeBytes()] = '\0';
        return result;
    }

    Base::IAllocator& selected = Base::GetDefaultAllocator();
    Base::String text(&selected);
    Base::Result<void> assigned = text.Assign(key);
    if (!assigned) return assigned.GetStatus();
    Base::Result<Base::Ref<StringValueStorage>> storage =
        Base::MakeRefWithAllocator<StringValueStorage>(selected, std::move(text));
    if (!storage) return storage.GetStatus();

    result.isShortString_ = false;
    new (&result.storage_.storage) Base::Ref<Base::Object>(std::move(storage).Value());
    return result;
}

Value Value::FromObject(TypeId type, Base::Ref<Base::Object> value) noexcept {
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::Object;
    new (&result.storage_.storage) Base::Ref<Base::Object>(std::move(value));
    return result;
}

Value Value::NullObject(TypeId type) noexcept {
    return FromObject(type, {});
}

Base::Result<Value> Value::TryFromCustom(
    TypeId type,
    const void* source,
    const ValueTypeRegistration* semantics) noexcept {
    if (type == InvalidTypeId || source == nullptr || semantics == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, "Custom Value construction is incomplete");
    }
    const ValueTypeRegistration& registration = *semantics;
    if (registration.size == 0U || registration.alignment == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, "Custom Value semantics are invalid");
    }
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::Custom;
    result.semantics_ = semantics;
    if (registration.inlineSafe &&
        registration.size <= InlineCapacity &&
        registration.alignment <= alignof(std::max_align_t)) {
        std::memcpy(result.storage_.inlineData, source, registration.size);
        result.inlineCustom_ = true;
        return result;
    }
    if (registration.copy == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, "Boxed custom Value requires a copy callback");
    }
    Base::IAllocator& selected = Base::GetDefaultAllocator();
    void* memory = selected.Allocate({
        registration.size,
        registration.alignment,
        Base::MemoryTag::Ui});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory, "Unable to allocate custom Value storage");
    }
    Base::Result<void> copied = registration.copy(
        memory, source, registration.context);
    if (!copied) {
        selected.Deallocate(
            memory, registration.size, registration.alignment, Base::MemoryTag::Ui);
        return copied.GetStatus();
    }
    Base::Result<Base::Ref<CustomValueStorage>> storage =
        Base::MakeRefWithAllocator<CustomValueStorage>(
            selected, memory, selected, semantics);
    if (!storage) {
        if (registration.destroy != nullptr) {
            registration.destroy(memory, registration.context);
        }
        selected.Deallocate(
            memory, registration.size, registration.alignment, Base::MemoryTag::Ui);
        return storage.GetStatus();
    }
    result.inlineCustom_ = false;
    new (&result.storage_.storage) Base::Ref<Base::Object>(std::move(storage).Value());
    return result;
}

Base::Result<Value> Value::TryFromCustom(
    TypeId type,
    const void* source,
    const Base::Ref<ValueTypeSemantics>& semantics) noexcept {
    if (!semantics) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, "Custom Value construction is incomplete");
    }
    return TryFromCustom(type, source, &semantics->Registration());
}

bool Value::AsBoolean() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::Boolean);
    return LoadScalar<bool>(storage_.inlineData);
}
std::int64_t Value::AsSignedInteger() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::SignedInteger);
    return LoadScalar<std::int64_t>(storage_.inlineData);
}
std::uint64_t Value::AsUnsignedInteger() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::UnsignedInteger);
    return LoadScalar<std::uint64_t>(storage_.inlineData);
}
double Value::AsDouble() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::Double);
    return LoadScalar<double>(storage_.inlineData);
}
Base::StringView Value::AsString() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::String);
    if (isShortString_) {
        return Base::StringView(
            reinterpret_cast<const char*>(storage_.inlineData),
            stringLength_);
    }
    AERO_ASSERT(storage_.storage);
    return static_cast<const StringValueStorage*>(storage_.storage.Get())->View();
}
Base::StringView Value::AsDynamicResourceKey() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::DynamicResource);
    if (isShortString_) {
        return Base::StringView(
            reinterpret_cast<const char*>(storage_.inlineData),
            stringLength_);
    }
    AERO_ASSERT(storage_.storage);
    return static_cast<const StringValueStorage*>(storage_.storage.Get())->View();
}
const Base::Ref<Base::Object>& Value::AsObject() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::Object);
    return storage_.storage;
}
const void* Value::AsCustom() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::Custom);
    return inlineCustom_
        ? static_cast<const void*>(storage_.inlineData)
        : static_cast<const CustomValueStorage*>(storage_.storage.Get())->Data();
}
void* Value::MutableCustom() noexcept {
    AERO_ASSERT(kind_ == ValueKind::Custom);
    return inlineCustom_
        ? static_cast<void*>(storage_.inlineData)
        : static_cast<CustomValueStorage*>(storage_.storage.Get())->MutableData();
}

bool Value::Equals(const Value& other) const noexcept {
    if (kind_ == ValueKind::DynamicResource && other.kind_ == ValueKind::DynamicResource) {
        return AsDynamicResourceKey() == other.AsDynamicResourceKey();
    }
    if (kind_ == ValueKind::String && other.kind_ == ValueKind::String) {
        return AsString() == other.AsString();
    }
    if (kind_ == ValueKind::Object && other.kind_ == ValueKind::Object) {
        return storage_.storage.Get() == other.storage_.storage.Get();
    }
    if (kind_ == ValueKind::Unset && other.kind_ == ValueKind::Unset) {
        return true;
    }
    if (type_ != other.type_ || kind_ != other.kind_) return false;
    switch (kind_) {
    case ValueKind::Unset: return true;
    case ValueKind::Boolean: return AsBoolean() == other.AsBoolean();
    case ValueKind::SignedInteger: return AsSignedInteger() == other.AsSignedInteger();
    case ValueKind::UnsignedInteger: return AsUnsignedInteger() == other.AsUnsignedInteger();
    case ValueKind::Double: {
        std::uint64_t left = 0U, right = 0U;
        const double leftValue = AsDouble(), rightValue = other.AsDouble();
        std::memcpy(&left, &leftValue, sizeof(left));
        std::memcpy(&right, &rightValue, sizeof(right));
        return left == right;
    }
    case ValueKind::String: return AsString() == other.AsString();
    case ValueKind::DynamicResource: return AsDynamicResourceKey() == other.AsDynamicResourceKey();
    case ValueKind::Object: return storage_.storage.Get() == other.storage_.storage.Get();
    case ValueKind::Custom: {
        if (!semantics_ || !other.semantics_) return false;
        const ValueTypeRegistration& registration = *semantics_;
        if (registration.equals != nullptr) {
            return registration.equals(AsCustom(), other.AsCustom(), registration.context);
        }
        return registration.inlineSafe &&
            std::memcmp(AsCustom(), other.AsCustom(), registration.size) == 0;
    }
    }
    return false;
}

} // namespace Aero::Base
