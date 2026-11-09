#include <Aero/Base/Value.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/String.hpp>

#include <cstring>
#include <utility>

namespace Aero::Base {
namespace {

// Intrusive single-allocation boxed string storage:
// [CustomAllocationHeader][StringValueStorage][char payload[length + 1]]
// Eliminates the double heap allocation (Base::String buffer + StringValueStorage object)
// when boxing strings longer than Value::InlineCapacity - 1 (15 bytes).
class StringValueStorage final : public Base::Object {
public:
    static Base::Result<Base::Ref<StringValueStorage>> Create(
        Base::IAllocator& allocator,
        Base::StringView value) noexcept {
        constexpr std::size_t kHeaderOffset =
            (sizeof(CustomAllocationHeader) + alignof(StringValueStorage) - 1U) &
            ~(alignof(StringValueStorage) - 1U);
        constexpr std::size_t kPayloadOffset = kHeaderOffset + sizeof(StringValueStorage);
        const std::size_t length = value.SizeBytes();
        const std::size_t totalSize = kPayloadOffset + length + 1U;
        constexpr std::size_t kAllocAlign =
            alignof(StringValueStorage) > alignof(CustomAllocationHeader)
                ? alignof(StringValueStorage)
                : alignof(CustomAllocationHeader);

        void* rawMemory = allocator.Allocate({
            totalSize,
            kAllocAlign,
            Base::MemoryTag::String});
        if (rawMemory == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
        }

        char* payload = static_cast<char*>(rawMemory) + kPayloadOffset;
        if (length > 0U && value.Data() != nullptr) {
            std::memcpy(payload, value.Data(), length);
        }
        payload[length] = '\0';

        void* objectMemory = static_cast<char*>(rawMemory) + kHeaderOffset;
        auto* storage = new (objectMemory) StringValueStorage(
            static_cast<std::uint32_t>(length));

        auto destroy = [](Base::Object* base, Base::IAllocator* objectAllocator) noexcept {
            auto* typed = static_cast<StringValueStorage*>(base);
            void* block = reinterpret_cast<char*>(typed) - kHeaderOffset;
            const std::size_t blockSize = kPayloadOffset + typed->length_ + 1U;
            typed->~StringValueStorage();
            objectAllocator->Deallocate(
                block,
                blockSize,
                kAllocAlign,
                Base::MemoryTag::String);
        };

        storage->AttachManagedLifetime(allocator, destroy);
        return Base::Ref<StringValueStorage>::AdoptConstructed(storage);
    }

    Base::StringView View() const noexcept {
        return Base::StringView(reinterpret_cast<const char*>(this + 1), length_);
    }

private:
    explicit StringValueStorage(std::uint32_t length) noexcept
        : length_(length) {}

    std::uint32_t length_ = 0U;
};

// Intrusive single-allocation boxed custom value storage:
// [CustomAllocationHeader][CustomValueStorage][aligned custom payload]
// Eliminates the double heap allocation (payload buffer + CustomValueStorage wrapper)
// and keeps the custom value payload in the same cache line as the Object header.
class CustomValueStorage final : public BoxedValue {
public:
    static Base::Result<Base::Ref<CustomValueStorage>> Create(
        Base::IAllocator& allocator,
        const void* source,
        const ValueTypeRegistration* semantics) noexcept {
        const ValueTypeRegistration& registration = *semantics;
        const std::size_t regAlign = registration.alignment > 0U
            ? static_cast<std::size_t>(registration.alignment)
            : alignof(std::max_align_t);
        const std::size_t baseAlign =
            alignof(CustomValueStorage) > alignof(CustomAllocationHeader)
                ? alignof(CustomValueStorage)
                : alignof(CustomAllocationHeader);
        const std::size_t allocAlign = regAlign > baseAlign ? regAlign : baseAlign;

        constexpr std::size_t kHeaderOffset =
            (sizeof(CustomAllocationHeader) + alignof(CustomValueStorage) - 1U) &
            ~(alignof(CustomValueStorage) - 1U);
        const std::size_t payloadOffset =
            (kHeaderOffset + sizeof(CustomValueStorage) + regAlign - 1U) &
            ~(regAlign - 1U);
        const std::size_t totalSize = payloadOffset + registration.size;

        void* rawMemory = allocator.Allocate({
            totalSize,
            allocAlign,
            Base::MemoryTag::Ui});
        if (rawMemory == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Unable to allocate custom Value storage");
        }

        void* payloadMemory = static_cast<char*>(rawMemory) + payloadOffset;
        Base::Result<void> copied = registration.copy(
            payloadMemory,
            source,
            registration.context);
        if (!copied) {
            allocator.Deallocate(rawMemory, totalSize, allocAlign, Base::MemoryTag::Ui);
            return copied.GetStatus();
        }

        void* objectMemory = static_cast<char*>(rawMemory) + kHeaderOffset;
        auto* storage = new (objectMemory) CustomValueStorage(
            payloadMemory,
            semantics,
            totalSize,
            allocAlign);

        auto destroy = [](Base::Object* base, Base::IAllocator* objectAllocator) noexcept {
            auto* typed = static_cast<CustomValueStorage*>(base);
            void* block = reinterpret_cast<char*>(typed) - kHeaderOffset;
            const std::size_t blockSize = typed->totalSize_;
            const std::size_t blockAlign = typed->allocAlign_;
            typed->~CustomValueStorage();
            objectAllocator->Deallocate(
                block,
                blockSize,
                blockAlign,
                Base::MemoryTag::Ui);
        };

        storage->AttachManagedLifetime(allocator, destroy);
        return Base::Ref<CustomValueStorage>::AdoptConstructed(storage);
    }

    ~CustomValueStorage() override {
        if (semantics_ != nullptr && semantics_->destroy != nullptr) {
            semantics_->destroy(value_, semantics_->context);
        }
    }

    const void* Data() const noexcept override { return value_; }
    void* MutableData() noexcept override { return value_; }
    bool RawEquals(const void* otherData) const noexcept override {
        if (otherData == nullptr || value_ == nullptr) {
            return false;
        }
        if (semantics_ != nullptr && semantics_->equals != nullptr) {
            return semantics_->equals(value_, otherData, semantics_->context);
        }
        if (semantics_ != nullptr) {
            return std::memcmp(value_, otherData, semantics_->size) == 0;
        }
        return false;
    }
    const ValueTypeRegistration* Semantics() const noexcept { return semantics_; }

private:
    CustomValueStorage(
        void* value,
        const ValueTypeRegistration* semantics,
        std::size_t totalSize,
        std::size_t allocAlign) noexcept
        : value_(value),
          semantics_(semantics),
          totalSize_(totalSize),
          allocAlign_(allocAlign) {}

    void* value_ = nullptr;
    const ValueTypeRegistration* semantics_ = nullptr;
    std::size_t totalSize_ = 0U;
    std::size_t allocAlign_ = alignof(CustomValueStorage);
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
    Base::Result<Base::Ref<StringValueStorage>> storage =
        StringValueStorage::Create(selected, value);
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
    Base::Result<Base::Ref<StringValueStorage>> storage =
        StringValueStorage::Create(selected, key);
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

Value Value::FromBoxedObject(TypeId type, Base::Ref<Base::Object> boxed) noexcept {
    Value result;
    result.type_ = type;
    result.kind_ = ValueKind::Custom;
    result.inlineCustom_ = false;
    new (&result.storage_.storage) Base::Ref<Base::Object>(std::move(boxed));
    return result;
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
    if (registration.inlineSafe &&
        registration.destroy == nullptr &&
        registration.size <= InlineCapacity &&
        registration.alignment <= alignof(std::max_align_t)) {
        std::memset(result.storage_.inlineData, 0, InlineCapacity);
        std::memcpy(result.storage_.inlineData, source, registration.size);
        result.inlineCustom_ = true;
        return result;
    }
    if (registration.copy == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, "Boxed custom Value requires a copy callback");
    }
    Base::IAllocator& selected = Base::GetDefaultAllocator();
    Base::Result<Base::Ref<CustomValueStorage>> storage =
        CustomValueStorage::Create(selected, source, semantics);
    if (!storage) {
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
double Value::ToDouble(double fallback) const noexcept {
    switch (kind_) {
    case ValueKind::Double:
        return AsDouble();
    case ValueKind::SignedInteger:
        return static_cast<double>(AsSignedInteger());
    case ValueKind::UnsignedInteger:
        return static_cast<double>(AsUnsignedInteger());
    case ValueKind::Boolean:
        return AsBoolean() ? 1.0 : 0.0;
    case ValueKind::String: {
        const Base::StringView text = AsString();
        if (text.Empty() || text.SizeBytes() >= 63U) return fallback;
        char buffer[64];
        std::memcpy(buffer, text.Data(), text.SizeBytes());
        buffer[text.SizeBytes()] = '\0';
        char* end = nullptr;
        const double parsed = std::strtod(buffer, &end);
        return (end != buffer && std::isfinite(parsed)) ? parsed : fallback;
    }
    default:
        return fallback;
    }
}
float Value::ToFloat(float fallback) const noexcept {
    return static_cast<float>(ToDouble(static_cast<double>(fallback)));
}
std::int32_t Value::ToInt32(std::int32_t fallback) const noexcept {
    switch (kind_) {
    case ValueKind::SignedInteger:
        return static_cast<std::int32_t>(AsSignedInteger());
    case ValueKind::UnsignedInteger:
        return static_cast<std::int32_t>(AsUnsignedInteger());
    case ValueKind::Double:
        return static_cast<std::int32_t>(AsDouble());
    case ValueKind::Boolean:
        return AsBoolean() ? 1 : 0;
    case ValueKind::String: {
        const Base::StringView text = AsString();
        if (text.Empty() || text.SizeBytes() >= 63U) return fallback;
        char buffer[64];
        std::memcpy(buffer, text.Data(), text.SizeBytes());
        buffer[text.SizeBytes()] = '\0';
        char* end = nullptr;
        const long long parsed = std::strtoll(buffer, &end, 10);
        return end != buffer ? static_cast<std::int32_t>(parsed) : fallback;
    }
    default:
        return fallback;
    }
}
bool Value::ToBool(bool fallback) const noexcept {
    switch (kind_) {
    case ValueKind::Boolean:
        return AsBoolean();
    case ValueKind::SignedInteger:
        return AsSignedInteger() != 0;
    case ValueKind::UnsignedInteger:
        return AsUnsignedInteger() != 0U;
    case ValueKind::Double:
        return AsDouble() != 0.0;
    case ValueKind::String: {
        const Base::StringView text = AsString();
        if (text == Base::StringView("True") || text == Base::StringView("true") ||
            text == Base::StringView("1")) {
            return true;
        }
        if (text == Base::StringView("False") || text == Base::StringView("false") ||
            text == Base::StringView("0")) {
            return false;
        }
        return fallback;
    }
    default:
        return fallback;
    }
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
const void* Value::AsBoxedCustom() const noexcept {
    AERO_ASSERT(kind_ == ValueKind::Custom);
    return inlineCustom_
        ? static_cast<const void*>(storage_.inlineData)
        : static_cast<const BoxedValue*>(storage_.storage.Get())->Data();
}
void* Value::MutableBoxedCustom() noexcept {
    AERO_ASSERT(kind_ == ValueKind::Custom);
    return inlineCustom_
        ? static_cast<void*>(storage_.inlineData)
        : static_cast<BoxedValue*>(storage_.storage.Get())->MutableData();
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
        if (inlineCustom_ && other.inlineCustom_) {
            return std::memcmp(storage_.inlineData, other.storage_.inlineData, InlineCapacity) == 0;
        }
        if (!inlineCustom_ && !other.inlineCustom_) {
            const auto* leftStorage = static_cast<const BoxedValue*>(storage_.storage.Get());
            const auto* rightStorage = static_cast<const BoxedValue*>(other.storage_.storage.Get());
            if (leftStorage == rightStorage) return true;
            if (leftStorage == nullptr || rightStorage == nullptr) return false;
            return leftStorage->RawEquals(rightStorage->Data());
        }
        const auto* boxedStorage = static_cast<const BoxedValue*>(
            inlineCustom_ ? other.storage_.storage.Get() : storage_.storage.Get());
        const void* inlineData = inlineCustom_ ? storage_.inlineData : other.storage_.inlineData;
        return boxedStorage != nullptr && boxedStorage->RawEquals(inlineData);
    }
    }
    return false;
}

} // namespace Aero::Base
