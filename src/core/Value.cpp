#include <Aero/Core/Value.hpp>

#include <Aero/Base/String.hpp>

#include <cstring>
#include <utility>

namespace Aero::Core {
namespace {

class StringValueStorage final : public Base::Object {
public:
    explicit StringValueStorage(Base::String&& value) noexcept
        : value_(std::move(value)) {}
    AERO_NODISCARD Base::StringView View() const noexcept { return value_.View(); }
private:
    Base::String value_;
};

class CustomValueStorage final : public Base::Object {
public:
    CustomValueStorage(void* value, Base::IAllocator& allocator,
        const Base::Ref<ValueTypeSemantics>& semantics) noexcept
        : value_(value), allocator_(&allocator), semantics_(semantics) {}
    ~CustomValueStorage() override {
        const ValueTypeRegistration& registration = semantics_->Registration();
        if (registration.destroy != nullptr) {
            registration.destroy(value_, registration.context);
        }
        allocator_->Deallocate(value_, registration.size, registration.alignment,
            Base::MemoryTag::Presentation);
    }
    AERO_NODISCARD const void* Data() const noexcept { return value_; }
private:
    void* value_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Ref<ValueTypeSemantics> semantics_;
};

template<class T>
void StoreScalar(unsigned char* destination, T value) noexcept {
    static_assert(sizeof(T) <= Value::InlineCapacity, "scalar is too large");
    std::memcpy(destination, &value, sizeof(T));
}

template<class T>
T LoadScalar(const unsigned char* source) noexcept {
    T value{};
    std::memcpy(&value, source, sizeof(T));
    return value;
}

} // namespace

Value Value::Unset() noexcept { return {}; }

Value Value::FromBoolean(TypeId type, bool value, Base::IAllocator*) noexcept {
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::Boolean;
    StoreScalar(result.inlineData_, value);
    return result;
}

Value Value::FromSignedInteger(
    TypeId type, std::int64_t value, Base::IAllocator*) noexcept {
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::SignedInteger;
    StoreScalar(result.inlineData_, value);
    return result;
}

Value Value::FromUnsignedInteger(
    TypeId type, std::uint64_t value, Base::IAllocator*) noexcept {
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::UnsignedInteger;
    StoreScalar(result.inlineData_, value);
    return result;
}

Value Value::FromDouble(TypeId type, double value, Base::IAllocator*) noexcept {
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::Double;
    StoreScalar(result.inlineData_, value);
    return result;
}

Base::Result<Value> Value::TryFromString(
    TypeId type, Base::StringView value, Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Base::String text(&selected);
    Base::Result<void> assigned = text.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    Base::Result<Base::Ref<StringValueStorage>> storage =
        Base::MakeRefWithAllocator<StringValueStorage>(selected, std::move(text));
    if (!storage) return storage.GetStatus();
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::String;
    result.storage_ = std::move(storage).Value();
    return result;
}

Value Value::FromObject(
    TypeId type, Base::Ref<Base::Object> value, Base::IAllocator*) noexcept {
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::Object;
    result.storage_ = std::move(value);
    return result;
}

Value Value::NullObject(TypeId type, Base::IAllocator*) noexcept {
    return FromObject(type, {});
}

Base::Result<Value> Value::TryFromCustom(
    TypeId type, const void* source,
    const Base::Ref<ValueTypeSemantics>& semantics,
    Base::IAllocator* allocator) noexcept {
    if (type == InvalidTypeId || source == nullptr || !semantics) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Custom Value construction is incomplete");
    }
    const ValueTypeRegistration& registration = semantics->Registration();
    if (registration.size == 0U || registration.alignment == 0U) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Custom Value semantics are invalid");
    }
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::Custom;
    result.semantics_ = semantics;
    if (registration.inlineSafe && registration.size <= InlineCapacity &&
        registration.alignment <= alignof(std::max_align_t)) {
        std::memcpy(result.inlineData_, source, registration.size);
        result.inlineCustom_ = true;
        return result;
    }
    if (registration.copy == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Boxed custom Value requires a copy callback");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    void* memory = selected.Allocate({registration.size, registration.alignment,
        Base::MemoryTag::Presentation});
    if (memory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::OutOfMemory,
            "Unable to allocate custom Value storage");
    }
    Base::Result<void> copied = registration.copy(
        memory, source, registration.context);
    if (!copied) {
        selected.Deallocate(memory, registration.size, registration.alignment,
            Base::MemoryTag::Presentation);
        return copied.GetStatus();
    }
    Base::Result<Base::Ref<CustomValueStorage>> storage =
        Base::MakeRefWithAllocator<CustomValueStorage>(
            selected, memory, selected, semantics);
    if (!storage) {
        if (registration.destroy != nullptr) {
            registration.destroy(memory, registration.context);
        }
        selected.Deallocate(memory, registration.size, registration.alignment,
            Base::MemoryTag::Presentation);
        return storage.GetStatus();
    }
    result.storage_ = std::move(storage).Value();
    return result;
}

bool Value::AsBoolean() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::Boolean);
    return LoadScalar<bool>(inlineData_);
}
std::int64_t Value::AsSignedInteger() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::SignedInteger);
    return LoadScalar<std::int64_t>(inlineData_);
}
std::uint64_t Value::AsUnsignedInteger() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::UnsignedInteger);
    return LoadScalar<std::uint64_t>(inlineData_);
}
double Value::AsDouble() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::Double);
    return LoadScalar<double>(inlineData_);
}
Base::StringView Value::AsString() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::String && storage_);
    return static_cast<const StringValueStorage*>(storage_.Get())->View();
}
const Base::Ref<Base::Object>& Value::AsObject() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::Object);
    return storage_;
}
const void* Value::AsCustom() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::Custom);
    return inlineCustom_ ? static_cast<const void*>(inlineData_)
        : static_cast<const CustomValueStorage*>(storage_.Get())->Data();
}

bool Value::Equals(const Value& other) const noexcept {
    if (type_ != other.type_ || kind_ != other.kind_) return false;
    switch (kind_) {
    case ValueKind::Unset: return true;
    case ValueKind::Boolean: return AsBoolean() == other.AsBoolean();
    case ValueKind::SignedInteger:
        return AsSignedInteger() == other.AsSignedInteger();
    case ValueKind::UnsignedInteger:
        return AsUnsignedInteger() == other.AsUnsignedInteger();
    case ValueKind::Double: {
        std::uint64_t left = 0U;
        std::uint64_t right = 0U;
        const double leftValue = AsDouble();
        const double rightValue = other.AsDouble();
        std::memcpy(&left, &leftValue, sizeof(left));
        std::memcpy(&right, &rightValue, sizeof(right));
        return left == right;
    }
    case ValueKind::String: return AsString() == other.AsString();
    case ValueKind::Object: return storage_.Get() == other.storage_.Get();
    case ValueKind::Custom: {
        if (!semantics_ || !other.semantics_) return false;
        const ValueTypeRegistration& registration = semantics_->Registration();
        if (registration.equals != nullptr) {
            return registration.equals(
                AsCustom(), other.AsCustom(), registration.context);
        }
        return registration.inlineSafe &&
            std::memcmp(AsCustom(), other.AsCustom(), registration.size) == 0;
    }
    }
    return false;
}

} // namespace Aero::Core
