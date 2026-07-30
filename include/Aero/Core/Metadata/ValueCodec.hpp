#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace Aero::Core {

// A metadata value that identifies an object type. It is deliberately
// distinct from the uint32_t storage used by TypeId so markup can resolve
// qualified type names without treating every integer property as a type.
struct TypeReference final {
    TypeId type = InvalidTypeId;

    constexpr bool IsValid() const noexcept {
        return type != InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Value> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("Any");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "Any";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<TypeReference> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("TypeReference");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "TypeReference";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Base::ResourceUri> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ResourceUri");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ResourceUri";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

AERO_API Base::Result<Value> TryCreateRuntimeValue(
    TypeId type,
    const void* source) noexcept;

template<>
struct MetaTypeTraits<bool> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Boolean"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Boolean"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

#define AERO_DEFINE_INTEGER_META_TYPE(cppType, metadataName) \
    template<> \
    struct MetaTypeTraits<cppType> { \
        static constexpr TypeId Id() noexcept { \
            return MakeTypeId(metadataName); \
        } \
        static constexpr Base::StringView Namespace() noexcept { \
            return AeroNamespaceUri(); \
        } \
        static constexpr Base::StringView Name() noexcept { \
            return metadataName; \
        } \
        static constexpr TypeId BaseType() noexcept { \
            return InvalidTypeId; \
        } \
    }

AERO_DEFINE_INTEGER_META_TYPE(std::int8_t, "Int8");
AERO_DEFINE_INTEGER_META_TYPE(std::int16_t, "Int16");
AERO_DEFINE_INTEGER_META_TYPE(std::int32_t, "Int32");
AERO_DEFINE_INTEGER_META_TYPE(std::int64_t, "Int64");
AERO_DEFINE_INTEGER_META_TYPE(std::uint8_t, "UInt8");
AERO_DEFINE_INTEGER_META_TYPE(std::uint16_t, "UInt16");
AERO_DEFINE_INTEGER_META_TYPE(std::uint32_t, "UInt32");
AERO_DEFINE_INTEGER_META_TYPE(std::uint64_t, "UInt64");

#undef AERO_DEFINE_INTEGER_META_TYPE

template<>
struct MetaTypeTraits<double> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Double"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Double"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Base::String> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("String"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "String"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<class T, class Enable = void>
struct ValueCodec {
    static constexpr TypeId Type() noexcept { return TypeOf<T>(); }

    template<class TRuntime>
    static Base::Result<Value> Encode(
        TRuntime& runtime,
        const T& value) noexcept {
        return runtime.TryCreateValue(Type(), &value);
    }

    static Base::Result<Value> Encode(const T& value) noexcept {
        return TryCreateRuntimeValue(Type(), &value);
    }

    template<class TRuntime>
    static Base::Result<T> Decode(
        TRuntime&,
        const Value& value) noexcept {
        return Decode(value);
    }

    static Base::Result<T> Decode(const Value& value) noexcept {
        if (value.Type() != Type() ||
            value.Kind() != ValueKind::Custom ||
            value.AsCustom() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Custom metadata value is incompatible");
        }
        return *static_cast<const T*>(value.AsCustom());
    }
};

template<>
struct ValueCodec<Value, void> {
    static constexpr TypeId Type() noexcept {
        return TypeOf<Value>();
    }
    template<class TRuntime>
    static Base::Result<Value> Encode(
        TRuntime&,
        const Value& value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(
        const Value& value) noexcept {
        return value;
    }
    template<class TRuntime>
    static Base::Result<Value> Decode(
        TRuntime&,
        const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<Value> Decode(
        const Value& value) noexcept {
        return value;
    }
};

template<>
struct ValueCodec<TypeReference, void> {
    static constexpr TypeId Type() noexcept {
        return TypeOf<TypeReference>();
    }
    template<class TRuntime>
    static Base::Result<Value> Encode(
        TRuntime&,
        TypeReference value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(
        TypeReference value) noexcept {
        if (!value.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata type reference is invalid");
        }
        return Value::FromUnsignedInteger(
            Type(), value.type);
    }
    template<class TRuntime>
    static Base::Result<TypeReference> Decode(
        TRuntime&,
        const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<TypeReference> Decode(
        const Value& value) noexcept {
        if (value.Type() != Type() ||
            value.Kind() != ValueKind::UnsignedInteger ||
            value.AsUnsignedInteger() >
                static_cast<std::uint64_t>(
                    std::numeric_limits<TypeId>::max())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata type reference is incompatible");
        }
        TypeReference result{
            static_cast<TypeId>(
                value.AsUnsignedInteger())};
        if (!result.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata type reference is invalid");
        }
        return result;
    }
};

template<>
struct ValueCodec<bool, void> {
    static constexpr TypeId Type() noexcept { return TypeOf<bool>(); }
    template<class TRuntime>
    static Base::Result<Value> Encode(TRuntime&, bool value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(bool value) noexcept {
        return Value::FromBoolean(Type(), value);
    }
    template<class TRuntime>
    static Base::Result<bool> Decode(
        TRuntime&, const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<bool> Decode(const Value& value) noexcept {
        if (value.Type() != Type() ||
            value.Kind() != ValueKind::Boolean) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Boolean metadata value is incompatible");
        }
        return value.AsBoolean();
    }
};

template<class T>
struct ValueCodec<T, std::enable_if_t<
    std::is_integral_v<T> && std::is_signed_v<T> &&
    !std::is_same_v<T, bool>>> {
    static constexpr TypeId Type() noexcept { return TypeOf<T>(); }
    template<class TRuntime>
    static Base::Result<Value> Encode(TRuntime&, T value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(T value) noexcept {
        return Value::FromSignedInteger(
            Type(), static_cast<std::int64_t>(value));
    }
    template<class TRuntime>
    static Base::Result<T> Decode(
        TRuntime&, const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<T> Decode(const Value& value) noexcept {
        if (value.Type() != Type() ||
            value.Kind() != ValueKind::SignedInteger ||
            value.AsSignedInteger() <
                static_cast<std::int64_t>(
                    std::numeric_limits<T>::min()) ||
            value.AsSignedInteger() >
                static_cast<std::int64_t>(
                    std::numeric_limits<T>::max())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Signed metadata value is incompatible");
        }
        return static_cast<T>(value.AsSignedInteger());
    }
};

template<class T>
struct ValueCodec<T, std::enable_if_t<
    std::is_integral_v<T> && std::is_unsigned_v<T> &&
    !std::is_same_v<T, bool>>> {
    static constexpr TypeId Type() noexcept { return TypeOf<T>(); }
    template<class TRuntime>
    static Base::Result<Value> Encode(TRuntime&, T value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(T value) noexcept {
        return Value::FromUnsignedInteger(
            Type(), static_cast<std::uint64_t>(value));
    }
    template<class TRuntime>
    static Base::Result<T> Decode(
        TRuntime&, const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<T> Decode(const Value& value) noexcept {
        if (value.Type() != Type() ||
            value.Kind() != ValueKind::UnsignedInteger ||
            value.AsUnsignedInteger() >
                static_cast<std::uint64_t>(
                    std::numeric_limits<T>::max())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Unsigned metadata value is incompatible");
        }
        return static_cast<T>(value.AsUnsignedInteger());
    }
};

template<>
struct ValueCodec<double, void> {
    static constexpr TypeId Type() noexcept { return TypeOf<double>(); }
    template<class TRuntime>
    static Base::Result<Value> Encode(TRuntime&, double value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(double value) noexcept {
        return Value::FromDouble(Type(), value);
    }
    template<class TRuntime>
    static Base::Result<double> Decode(
        TRuntime&, const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<double> Decode(const Value& value) noexcept {
        if (value.Type() != Type() ||
            value.Kind() != ValueKind::Double) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Double metadata value is incompatible");
        }
        return value.AsDouble();
    }
};

template<>
struct ValueCodec<float, void> {
    static constexpr TypeId Type() noexcept { return TypeOf<double>(); }
    template<class TRuntime>
    static Base::Result<Value> Encode(TRuntime&, float value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(float value) noexcept {
        return Value::FromDouble(
            Type(), static_cast<double>(value));
    }
    template<class TRuntime>
    static Base::Result<float> Decode(
        TRuntime&, const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<float> Decode(const Value& value) noexcept {
        if (value.Type() != Type() ||
            value.Kind() != ValueKind::Double) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Float metadata value is incompatible");
        }
        return static_cast<float>(value.AsDouble());
    }
};

template<>
struct ValueCodec<Base::String, void> {
    static constexpr TypeId Type() noexcept {
        return TypeOf<Base::String>();
    }
    template<class TRuntime>
    static Base::Result<Value> Encode(
        TRuntime&, const Base::String& value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(
        const Base::String& value) noexcept {
        return Value::TryFromString(Type(), value.View());
    }
    template<class TRuntime>
    static Base::Result<Base::String> Decode(
        TRuntime&, const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<Base::String> Decode(
        const Value& value) noexcept {
        if (value.Type() != Type() ||
            value.Kind() != ValueKind::String) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "String metadata value is incompatible");
        }
        Base::String decoded;
        Base::Result<void> assigned =
            decoded.TryAssign(value.AsString());
        if (!assigned) return assigned.GetStatus();
        return decoded;
    }
};

template<class T>
struct ValueCodec<T, std::enable_if_t<std::is_enum_v<T>>> {
    using Underlying = std::underlying_type_t<T>;
    static constexpr TypeId Type() noexcept { return TypeOf<T>(); }
    template<class TRuntime>
    static Base::Result<Value> Encode(TRuntime&, T value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(T value) noexcept {
        if constexpr (std::is_signed_v<Underlying>) {
            return Value::FromSignedInteger(
                Type(),
                static_cast<std::int64_t>(
                    static_cast<Underlying>(value)));
        } else {
            return Value::FromUnsignedInteger(
                Type(),
                static_cast<std::uint64_t>(
                    static_cast<Underlying>(value)));
        }
    }
    template<class TRuntime>
    static Base::Result<T> Decode(
        TRuntime&, const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<T> Decode(const Value& value) noexcept {
        if (value.Type() != Type()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Enum metadata value type is incompatible");
        }
        if constexpr (std::is_signed_v<Underlying>) {
            if (value.Kind() != ValueKind::SignedInteger) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Signed enum metadata value is incompatible");
            }
            return static_cast<T>(
                static_cast<Underlying>(
                    value.AsSignedInteger()));
        } else {
            if (value.Kind() != ValueKind::UnsignedInteger) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Unsigned enum metadata value is incompatible");
            }
            return static_cast<T>(
                static_cast<Underlying>(
                    value.AsUnsignedInteger()));
        }
    }
};

template<class T>
struct ValueCodec<Base::Ref<T>, void> {
    static constexpr TypeId Type() noexcept { return TypeOf<T>(); }
    template<class TRuntime>
    static Base::Result<Value> Encode(
        TRuntime&, const Base::Ref<T>& value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(
        const Base::Ref<T>& value) noexcept {
        if (!value) return Value::NullObject(Type());
        return Value::FromObject(
            Type(), Base::Ref<Base::Object>(value));
    }
    template<class TRuntime>
    static Base::Result<Base::Ref<T>> Decode(
        TRuntime&, const Value& value) noexcept {
        return Decode(value);
    }
    static Base::Result<Base::Ref<T>> Decode(
        const Value& value) noexcept {
        if (value.Kind() != ValueKind::Object) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Object metadata value is incompatible");
        }
        if (value.IsNullObject() || !value.AsObject()) {
            return Base::Ref<T>{};
        }
        return Base::Ref<T>::FromBorrowed(
            *static_cast<T*>(value.AsObject().Get()));
    }
};

} // namespace Aero::Core
