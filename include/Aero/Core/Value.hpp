#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/MetadataId.hpp>

#include <cstddef>
#include <cstdint>

namespace Aero::Core {

enum class ValueKind : std::uint8_t {
    Unset = 0U,
    None = Unset,
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Double,
    String,
    Object,
    Custom
};

using ValueCopyCallback = Base::Result<void> (*)(
    void* destination, const void* source, void* context) noexcept;
using ValueDestroyCallback = void (*)(void* value, void* context) noexcept;
using ValueEqualsCallback = bool (*)(
    const void* left, const void* right, void* context) noexcept;

struct ValueTypeRegistration final {
    std::uint32_t size = 0U;
    std::uint32_t alignment = 0U;
    ValueCopyCallback copy = nullptr;
    ValueDestroyCallback destroy = nullptr;
    ValueEqualsCallback equals = nullptr;
    void* context = nullptr;
    bool inlineSafe = false;
};

class AERO_API ValueTypeSemantics final : public Base::Object {
public:
    explicit ValueTypeSemantics(
        const ValueTypeRegistration& registration) noexcept
        : registration_(registration) {}

    const ValueTypeRegistration& Registration() const noexcept {
        return registration_;
    }

private:
    ValueTypeRegistration registration_;
};

class AERO_API Value final {
public:
    static constexpr std::uint32_t InlineCapacity = 32U;

    Value() noexcept = default;
    explicit Value(Base::IAllocator*) noexcept {}
    Value(const Value&) noexcept = default;
    Value(Value&&) noexcept = default;
    Value& operator=(const Value&) noexcept = default;
    Value& operator=(Value&&) noexcept = default;
    ~Value() = default;

    static Value Unset() noexcept;
    static Value FromBoolean(
        TypeId type, bool value, Base::IAllocator* allocator = nullptr) noexcept;
    static Value FromSignedInteger(
        TypeId type, std::int64_t value,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Value FromUnsignedInteger(
        TypeId type, std::uint64_t value,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Value FromDouble(
        TypeId type, double value, Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<Value> TryFromString(
        TypeId type, Base::StringView value,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Value FromObject(
        TypeId type, Base::Ref<Base::Object> value,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Value NullObject(
        TypeId type, Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<Value> TryFromCustom(
        TypeId type, const void* source,
        const Base::Ref<ValueTypeSemantics>& semantics,
        Base::IAllocator* allocator = nullptr) noexcept;

    TypeId Type() const noexcept { return type_; }
    ValueKind Kind() const noexcept { return kind_; }
    bool IsUnset() const noexcept {
        return kind_ == ValueKind::Unset;
    }
    bool IsNullObject() const noexcept {
        return kind_ == ValueKind::Object && !storage_;
    }
    bool IsInlineCustom() const noexcept {
        return kind_ == ValueKind::Custom && inlineCustom_;
    }

    bool AsBoolean() const noexcept;
    std::int64_t AsSignedInteger() const noexcept;
    std::uint64_t AsUnsignedInteger() const noexcept;
    double AsDouble() const noexcept;
    Base::StringView AsString() const noexcept;
    const Base::Ref<Base::Object>& AsObject() const noexcept;
    const void* AsCustom() const noexcept;
    bool Equals(const Value& other) const noexcept;

private:
    alignas(std::max_align_t) unsigned char inlineData_[InlineCapacity]{};
    TypeId type_ = InvalidTypeId;
    ValueKind kind_ = ValueKind::Unset;
    bool inlineCustom_ = false;
    Base::Ref<Base::Object> storage_;
    Base::Ref<ValueTypeSemantics> semantics_;
};

inline bool operator==(
    const Value& left, const Value& right) noexcept {
    return left.Equals(right);
}

inline bool operator!=(
    const Value& left, const Value& right) noexcept {
    return !(left == right);
}

using TextValueConverterCallback = Base::Result<Value> (*)(
    TypeId targetType, Base::StringView text,
    Base::IAllocator& allocator, void* context) noexcept;

struct TextValueConverterRegistration final {
    TypeId type = InvalidTypeId;
    TextValueConverterCallback convert = nullptr;
    void* context = nullptr;
};

} // namespace Aero::Core
