#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/MetadataId.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>

#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Base {

struct Color;
struct Rect;
struct Size;

using TypeId = Base::MetaTypeId;
using MemberId = Base::MetaMemberId;

inline constexpr TypeId InvalidTypeId = Base::InvalidMetaTypeId;
inline constexpr MemberId InvalidMemberId = Base::InvalidMetaMemberId;

enum class ValueKind : std::uint8_t {
    Unset = 0U,
    None = Unset,
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Double,
    String,
    Object,
    Custom,
    DynamicResource
};

using ValueCopyCallback = Result<void> (*)(void* destination, const void* source, void* context) noexcept;
using ValueDestroyCallback = void (*)(void* value, void* context) noexcept;
using ValueEqualsCallback = bool (*)(const void* left, const void* right, void* context) noexcept;

struct ValueTypeRegistration {
    std::uint32_t size = 0U;
    std::uint32_t alignment = 0U;
    ValueCopyCallback copy = nullptr;
    ValueDestroyCallback destroy = nullptr;
    ValueEqualsCallback equals = nullptr;
    void* context = nullptr;
    bool inlineSafe = false;
};

class AERO_BASE_API ValueTypeSemantics : public Base::Object {
public:
    explicit ValueTypeSemantics(const ValueTypeRegistration& registration) noexcept : registration_(registration) {}
    const ValueTypeRegistration& Registration() const noexcept { return registration_; }
private:
    ValueTypeRegistration registration_;
};

class AERO_BASE_API Value {
public:
    static constexpr std::uint32_t InlineCapacity = 16U;

    Value() noexcept = default;
    Value(const Value& other) noexcept;
    Value(Value&& other) noexcept;
    Value& operator=(const Value& other) noexcept;
    Value& operator=(Value&& other) noexcept;
    ~Value();

    Value(bool value) noexcept;
    Value(std::int64_t value) noexcept;
    Value(int value) noexcept;
    Value(std::uint64_t value) noexcept;
    Value(unsigned int value) noexcept;
    Value(double value) noexcept;
    Value(float value) noexcept;
    Value(StringView value) noexcept;
    Value(const char* value) noexcept;
    Value(Ref<Base::Object> value) noexcept;
    template<class T, class = std::enable_if_t<std::is_base_of_v<Base::Object, T> && !std::is_same_v<T, Base::Object>>>
    Value(Ref<T> value) noexcept : Value(Ref<Base::Object>(std::move(value))) {}

    static Value Unset() noexcept;
    static Value FromBoolean(TypeId type, bool value) noexcept;
    static Value FromSignedInteger(TypeId type, std::int64_t value) noexcept;
    static Value FromUnsignedInteger(TypeId type, std::uint64_t value) noexcept;
    static Value FromDouble(TypeId type, double value) noexcept;
    static Result<Value> TryFromString(TypeId type, StringView value) noexcept;
    static Result<Value> FromDynamicResourceKey(StringView key) noexcept;
    static Value FromObject(TypeId type, Ref<Base::Object> value) noexcept;
    static Value NullObject(TypeId type) noexcept;

    static Result<Value> TryFromCustom(
        TypeId type,
        const void* source,
        const ValueTypeRegistration* semantics) noexcept;
    static Result<Value> TryFromCustom(
        TypeId type,
        const void* source,
        const ValueTypeRegistration& semantics) noexcept {
        return TryFromCustom(type, source, &semantics);
    }
    static Result<Value> TryFromCustom(
        TypeId type,
        const void* source,
        const Ref<ValueTypeSemantics>& semantics) noexcept;

    TypeId Type() const noexcept { return type_; }
    ValueKind Kind() const noexcept { return kind_; }
    bool IsUnset() const noexcept { return kind_ == ValueKind::Unset; }
    bool IsNullObject() const noexcept;
    bool IsInlineCustom() const noexcept { return kind_ == ValueKind::Custom && inlineCustom_; }
    bool IsShortString() const noexcept { return (kind_ == ValueKind::String || kind_ == ValueKind::DynamicResource) && isShortString_; }
    bool IsDynamicResource() const noexcept { return kind_ == ValueKind::DynamicResource; }

    bool AsBoolean() const noexcept;
    std::int64_t AsSignedInteger() const noexcept;
    std::uint64_t AsUnsignedInteger() const noexcept;
    double AsDouble() const noexcept;
    StringView AsString() const noexcept;
    StringView AsDynamicResourceKey() const noexcept;
    const Ref<Base::Object>& AsObject() const noexcept;
    const void* AsCustom() const noexcept;
    void* MutableCustom() noexcept;
    bool Equals(const Value& other) const noexcept;
    void Reset() noexcept;

private:
    TypeId type_ = InvalidTypeId;
    ValueKind kind_ = ValueKind::Unset;
    bool inlineCustom_ = false;
    bool isShortString_ = false;
    std::uint8_t stringLength_ = 0U;

    union Storage {
        alignas(8) unsigned char inlineData[InlineCapacity];
        Ref<Base::Object> storage;

        Storage() noexcept {
            std::memset(inlineData, 0, sizeof(inlineData));
        }
        ~Storage() noexcept {}
    } storage_{};

    bool HasActiveObject() const noexcept {
        return (kind_ == ValueKind::Object) ||
               ((kind_ == ValueKind::String || kind_ == ValueKind::DynamicResource) && !isShortString_) ||
               (kind_ == ValueKind::Custom && !inlineCustom_);
    }
    void DestroyStorage() noexcept;

    friend class ValueAccess;
};

static_assert(sizeof(Value) == 32U, "Value size must be exactly 32 bytes");
static_assert(alignof(Value) == 8U, "Value alignment must be 8 bytes");

inline void Value::DestroyStorage() noexcept {
    if (HasActiveObject()) {
        storage_.storage.~Ref();
    }
}

inline Value::~Value() {
    DestroyStorage();
}

inline Value::Value(const Value& other) noexcept
    : type_(other.type_),
      kind_(other.kind_),
      inlineCustom_(other.inlineCustom_),
      isShortString_(other.isShortString_),
      stringLength_(other.stringLength_) {
    if (other.HasActiveObject()) {
        new (&storage_.storage) Ref<Base::Object>(other.storage_.storage);
    } else {
        std::memcpy(storage_.inlineData, other.storage_.inlineData, InlineCapacity);
    }
}

inline Value::Value(Value&& other) noexcept
    : type_(other.type_),
      kind_(other.kind_),
      inlineCustom_(other.inlineCustom_),
      isShortString_(other.isShortString_),
      stringLength_(other.stringLength_) {
    if (other.HasActiveObject()) {
        new (&storage_.storage) Ref<Base::Object>(std::move(other.storage_.storage));
    } else {
        std::memcpy(storage_.inlineData, other.storage_.inlineData, InlineCapacity);
    }
    other.kind_ = ValueKind::Unset;
    other.type_ = InvalidTypeId;
    other.inlineCustom_ = false;
    other.isShortString_ = false;
    other.stringLength_ = 0U;
}

inline Value& Value::operator=(const Value& other) noexcept {
    if (this == &other) return *this;
    DestroyStorage();
    type_ = other.type_;
    kind_ = other.kind_;
    inlineCustom_ = other.inlineCustom_;
    isShortString_ = other.isShortString_;
    stringLength_ = other.stringLength_;
    if (other.HasActiveObject()) {
        new (&storage_.storage) Ref<Base::Object>(other.storage_.storage);
    } else {
        std::memcpy(storage_.inlineData, other.storage_.inlineData, InlineCapacity);
    }
    return *this;
}

inline Value& Value::operator=(Value&& other) noexcept {
    if (this == &other) return *this;
    DestroyStorage();
    type_ = other.type_;
    kind_ = other.kind_;
    inlineCustom_ = other.inlineCustom_;
    isShortString_ = other.isShortString_;
    stringLength_ = other.stringLength_;
    if (other.HasActiveObject()) {
        new (&storage_.storage) Ref<Base::Object>(std::move(other.storage_.storage));
    } else {
        std::memcpy(storage_.inlineData, other.storage_.inlineData, InlineCapacity);
    }
    other.kind_ = ValueKind::Unset;
    other.type_ = InvalidTypeId;
    other.inlineCustom_ = false;
    other.isShortString_ = false;
    other.stringLength_ = 0U;
    return *this;
}

inline bool Value::IsNullObject() const noexcept {
    return kind_ == ValueKind::Object && !storage_.storage;
}

inline void Value::Reset() noexcept {
    DestroyStorage();
    type_ = InvalidTypeId;
    kind_ = ValueKind::Unset;
    inlineCustom_ = false;
    isShortString_ = false;
    stringLength_ = 0U;
    std::memset(storage_.inlineData, 0, InlineCapacity);
}

inline bool operator==(const Value& left, const Value& right) noexcept { return left.Equals(right); }
inline bool operator!=(const Value& left, const Value& right) noexcept { return !(left == right); }

using TextValueConverterCallback = Result<Value> (*)(TypeId targetType, StringView text, void* context) noexcept;

struct TextValueConverterRegistration {
    TypeId type = InvalidTypeId;
    TextValueConverterCallback convert = nullptr;
    void* context = nullptr;
};

inline Value::Value(bool value) noexcept : Value(FromBoolean(MakeMetaTypeId("Boolean"), value)) {}
inline Value::Value(std::int64_t value) noexcept : Value(FromSignedInteger(MakeMetaTypeId("Int64"), value)) {}
inline Value::Value(int value) noexcept : Value(FromSignedInteger(MakeMetaTypeId("Int32"), value)) {}
inline Value::Value(std::uint64_t value) noexcept : Value(FromUnsignedInteger(MakeMetaTypeId("UInt64"), value)) {}
inline Value::Value(unsigned int value) noexcept : Value(FromUnsignedInteger(MakeMetaTypeId("UInt32"), value)) {}
inline Value::Value(double value) noexcept : Value(FromDouble(MakeMetaTypeId("Double"), value)) {}
inline Value::Value(float value) noexcept : Value(FromDouble(MakeMetaTypeId("Double"), static_cast<double>(value))) {}
inline Value::Value(StringView value) noexcept {
    auto res = TryFromString(MakeMetaTypeId("String"), value);
    if (res) *this = std::move(res).Value();
}
inline Value::Value(const char* value) noexcept
    : Value(value ? StringView(value, static_cast<std::uint32_t>(std::strlen(value))) : StringView()) {}
inline Value::Value(Ref<Base::Object> value) noexcept {
    TypeId type = value ? value->RuntimeType() : MakeMetaTypeId("Object");
    *this = FromObject(type, std::move(value));
}

} // namespace Aero::Base

namespace Aero {

using Base::Value;
using Base::ValueKind;

} // namespace Aero
