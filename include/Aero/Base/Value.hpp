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
    Custom
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
    static constexpr std::uint32_t InlineCapacity = 32U;
    Value() noexcept = default;
    Value(const Value&) noexcept = default;
    Value(Value&&) noexcept = default;
    Value& operator=(const Value&) noexcept = default;
    Value& operator=(Value&&) noexcept = default;
    ~Value() = default;
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
    static Value FromObject(TypeId type, Ref<Base::Object> value) noexcept;
    static Value NullObject(TypeId type) noexcept;
    static Result<Value> TryFromCustom(TypeId type, const void* source, const Ref<ValueTypeSemantics>& semantics) noexcept;
    TypeId Type() const noexcept { return type_; }
    ValueKind Kind() const noexcept { return kind_; }
    bool IsUnset() const noexcept { return kind_ == ValueKind::Unset; }
    bool IsNullObject() const noexcept { return kind_ == ValueKind::Object && !storage_; }
    bool IsInlineCustom() const noexcept { return kind_ == ValueKind::Custom && inlineCustom_; }
    bool AsBoolean() const noexcept;
    std::int64_t AsSignedInteger() const noexcept;
    std::uint64_t AsUnsignedInteger() const noexcept;
    double AsDouble() const noexcept;
    StringView AsString() const noexcept;
    const Ref<Base::Object>& AsObject() const noexcept;
    const void* AsCustom() const noexcept;
    void* MutableCustom() noexcept;
    bool Equals(const Value& other) const noexcept;
private:
    alignas(std::max_align_t) unsigned char inlineData_[InlineCapacity]{};
    TypeId type_ = InvalidTypeId;
    ValueKind kind_ = ValueKind::Unset;
    bool inlineCustom_ = false;
    Ref<Base::Object> storage_;
    Ref<ValueTypeSemantics> semantics_;
};

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
