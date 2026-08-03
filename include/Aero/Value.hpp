#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/MetadataId.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
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
#include <limits>
#include <type_traits>
#include <utility>

namespace Aero::Base {

using TypeId = Base::MetaTypeId;
using MemberId = Base::MetaMemberId;

inline constexpr TypeId InvalidTypeId = Base::InvalidMetaTypeId;
inline constexpr MemberId InvalidMemberId = Base::InvalidMetaMemberId;

} // namespace Aero::Base

namespace Aero::Base {

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

using ValueCopyCallback = Base::Result<void> (*)(void* destination, const void* source, void* context) noexcept;
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

class AERO_API ValueTypeSemantics : public Base::Object {
public:
    explicit ValueTypeSemantics(const ValueTypeRegistration& registration) noexcept : registration_(registration) {}
    const ValueTypeRegistration& Registration() const noexcept { return registration_; }
private:
    ValueTypeRegistration registration_;
};

class AERO_API Value {
public:
    static constexpr std::uint32_t InlineCapacity = 32U;
    Value() noexcept = default;
    Value(const Value&) noexcept = default;
    Value(Value&&) noexcept = default;
    Value& operator=(const Value&) noexcept = default;
    Value& operator=(Value&&) noexcept = default;
    ~Value() = default;
    static Value Unset() noexcept;
    static Value FromBoolean(TypeId type, bool value) noexcept;
    static Value FromSignedInteger(TypeId type, std::int64_t value) noexcept;
    static Value FromUnsignedInteger(TypeId type, std::uint64_t value) noexcept;
    static Value FromDouble(TypeId type, double value) noexcept;
    static Base::Result<Value> TryFromString(TypeId type, Base::StringView value) noexcept;
    static Value FromObject(TypeId type, Base::Ref<Base::Object> value) noexcept;
    static Value NullObject(TypeId type) noexcept;
    static Base::Result<Value> TryFromCustom(TypeId type, const void* source, const Base::Ref<ValueTypeSemantics>& semantics) noexcept;
    TypeId Type() const noexcept { return type_; }
    ValueKind Kind() const noexcept { return kind_; }
    bool IsUnset() const noexcept { return kind_ == ValueKind::Unset; }
    bool IsNullObject() const noexcept { return kind_ == ValueKind::Object && !storage_; }
    bool IsInlineCustom() const noexcept { return kind_ == ValueKind::Custom && inlineCustom_; }
    bool AsBoolean() const noexcept;
    std::int64_t AsSignedInteger() const noexcept;
    std::uint64_t AsUnsignedInteger() const noexcept;
    double AsDouble() const noexcept;
    Base::StringView AsString() const noexcept;
    const Base::Ref<Base::Object>& AsObject() const noexcept;
    const void* AsCustom() const noexcept;
    void* MutableCustom() noexcept;
    bool Equals(const Value& other) const noexcept;
private:
    alignas(std::max_align_t) unsigned char inlineData_[InlineCapacity]{};
    TypeId type_ = InvalidTypeId;
    ValueKind kind_ = ValueKind::Unset;
    bool inlineCustom_ = false;
    Base::Ref<Base::Object> storage_;
    Base::Ref<ValueTypeSemantics> semantics_;
};

inline bool operator==(const Value& left, const Value& right) noexcept { return left.Equals(right); }
inline bool operator!=(const Value& left, const Value& right) noexcept { return !(left == right); }

using TextValueConverterCallback = Base::Result<Value> (*)(TypeId targetType, Base::StringView text, void* context) noexcept;
struct TextValueConverterRegistration {
    TypeId type = InvalidTypeId;
    TextValueConverterCallback convert = nullptr;
    void* context = nullptr;
};

} // namespace Aero::Base

namespace Aero { template<class TOwner, class TArgs> class RoutedEventRef; }
namespace Aero::Meta { class Registry; class Registration; }
namespace Aero::Meta {
using Base::InvalidMemberId;
using Base::InvalidTypeId;
using Base::MemberId;
using Base::TypeId;
using Base::Value;
using Base::ValueKind;
using Base::ValueTypeRegistration;
using Base::ValueTypeSemantics;
using Base::TextValueConverterCallback;
using Base::TextValueConverterRegistration;

class BehaviorTable;
class RegistrationTypes;
class TypeRegistry;
template<class TOwner, class TValue> class DependencyPropertyRef;
template<class TOwner, class TValue> class AttachedPropertyRef;
template<class TOwner, class TValue> class ReadOnlyPropertyRef;
inline constexpr std::uint32_t TypeIdAlgorithmVersion = 1U;
inline constexpr std::uint32_t MetadataSchemaFormatVersion = 2U;
inline constexpr std::uint32_t MetadataProgramFormatVersion = 8U;

enum class MetadataTypeKind : std::uint8_t {
    Object = 0U,
    Interface,
    Struct,
    Enum,
    Primitive
};

// Runtime description associated with a C++ enum/value type.  The stable
// token is deliberately separate from the XAML TypeId: it identifies the C++
// type without relying on RTTI or a DLL-local address.  Enum and value macros
// below expose only that token and query this catalog at runtime.
struct RuntimeTypeInfo {
    TypeId id = InvalidTypeId;
    Base::StringView xamlNamespace;
    Base::StringView name;
    TypeId baseType = InvalidTypeId;
    MetadataTypeKind kind = MetadataTypeKind::Struct;
};

AERO_API Base::Status BindRuntimeTypeInfo(
    TypeId token,
    const RuntimeTypeInfo& info) noexcept;
AERO_API RuntimeTypeInfo ResolveRuntimeTypeInfo(
    TypeId token) noexcept;

enum class MemberKind : std::uint8_t {
    Property = 1U,
    Event = 2U,
    Method = 3U,
    Field = 4U,
    EnumValue = 5U
};

enum class TypeFlags : std::uint32_t {
    None = 0U,
    Abstract = 1U << 0U,
    Sealed = 1U << 1U,
    ValueType = 1U << 2U,
    Collection = 1U << 3U,
    MarkupExtension = 1U << 4U,
    FlagsEnum = 1U << 5U,
    TriviallyCopyable = 1U << 6U,
    SignedEnum = 1U << 7U
};

enum class PropertyFlags : std::uint32_t {
    None = 0U,
    Attached = 1U << 0U,
    ReadOnly = 1U << 1U,
    Inherits = 1U << 2U,
    AffectsMeasure = 1U << 3U,
    AffectsArrange = 1U << 4U,
    AffectsRender = 1U << 5U,
    AffectsParentMeasure = 1U << 6U,
    AffectsParentArrange = 1U << 7U,
    Structural = 1U << 8U,
    Collection = 1U << 9U,
    WriteOnly = 1U << 10U,
    AnyValue = 1U << 11U
};

enum class FieldFlags : std::uint32_t {
    None = 0U,
    ReadOnly = 1U << 0U,
    Transient = 1U << 1U
};

enum class PropertyAccessKind : std::uint8_t {
    External = 0U,
    Ordinary = 1U,
    Provider = 2U
};

using PropertyProviderId = std::uint64_t;
inline constexpr PropertyProviderId InvalidPropertyProviderId = 0U;
inline constexpr PropertyProviderId DependencyPropertyProviderId =
    UINT64_C(0x445050524F564944);

enum class MethodFlags : std::uint32_t {
    None = 0U,
    Const = 1U << 0U
};

enum class EventFlags : std::uint32_t {
    None = 0U,
    Attached = 1U << 0U,
    Routed = 1U << 1U
};

constexpr TypeFlags operator|(
    TypeFlags left, TypeFlags right) noexcept {
    return static_cast<TypeFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr TypeFlags operator&(
    TypeFlags left, TypeFlags right) noexcept {
    return static_cast<TypeFlags>(
        static_cast<std::uint32_t>(left) &
        static_cast<std::uint32_t>(right));
}

constexpr PropertyFlags operator|(
    PropertyFlags left, PropertyFlags right) noexcept {
    return static_cast<PropertyFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr FieldFlags operator|(
    FieldFlags left, FieldFlags right) noexcept {
    return static_cast<FieldFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr EventFlags operator|(
    EventFlags left, EventFlags right) noexcept {
    return static_cast<EventFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

struct TypeFlagPredicate {
    constexpr bool operator()(
        TypeFlags value,
        TypeFlags flag) const noexcept {
        return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
    }
};

inline constexpr TypeFlagPredicate HasTypeFlag{};

struct FieldFlagPredicate {
    constexpr bool operator()(
        FieldFlags value,
        FieldFlags flag) const noexcept {
        return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
    }
};

inline constexpr FieldFlagPredicate HasFieldFlag{};


constexpr TypeId MakeTypeId(Base::StringView xamlNamespace, Base::StringView name) noexcept { return Base::MakeMetaTypeId(xamlNamespace, name); }
constexpr Base::StringView AeroNamespaceUri() noexcept { return Base::DefaultMetadataNamespaceUri(); }
constexpr TypeId MakeTypeId(Base::StringView name) noexcept { return Base::MakeMetaTypeId(name); }

struct NoMetadataBase {};

template<class T>
struct TypeTraits {
    static constexpr TypeId Id() noexcept { return T::StaticTypeId(); }
    static constexpr Base::StringView Namespace() noexcept {
        return T::StaticMetadataNamespace();
    }
    static constexpr Base::StringView Name() noexcept {
        return T::StaticMetadataName();
    }
    static constexpr TypeId BaseType() noexcept {
        if constexpr (std::is_same_v<typename T::BaseType,
            NoMetadataBase>) {
            return InvalidTypeId;
        } else {
            return TypeTraits<typename T::BaseType>::Id();
        }
    }
};

template<>
struct TypeTraits<Base::Object> {
    static constexpr TypeId Id() noexcept {
        return Base::Object::StaticTypeId();
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Object"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<class T>
inline TypeId TypeOf() noexcept { return TypeTraits<T>::Id(); }

// Enum/value declarations intentionally contain no XAML names, TypeIds or
// enum value tables.  Those are supplied by Meta::Register from the central
// metadata implementation.  The token is derived from the spelling supplied
// to the macro and is therefore stable in static and shared builds without
// RTTI.
#define AERO_DECLARE_TYPE_ENUM(typeName) \
namespace Aero::Meta { \
template<> struct TypeTraits<typeName> { \
    static constexpr TypeId Token() noexcept { \
        return Base::MakeMetaTypeId( \
            Base::StringView("AERO.CPP.ENUM"), \
            Base::StringView(#typeName)); \
    } \
    static TypeId Id() noexcept { \
        return ResolveRuntimeTypeInfo(Token()).id; \
    } \
    static Base::StringView Namespace() noexcept { \
        return ResolveRuntimeTypeInfo(Token()).xamlNamespace; \
    } \
    static Base::StringView Name() noexcept { \
        return ResolveRuntimeTypeInfo(Token()).name; \
    } \
    static TypeId BaseType() noexcept { \
        return ResolveRuntimeTypeInfo(Token()).baseType; \
    } \
    static constexpr MetadataTypeKind Kind() noexcept { \
        return MetadataTypeKind::Enum; \
    } \
}; \
}

#define AERO_DECLARE_TYPE_VALUE(typeName) \
namespace Aero::Meta { \
template<> struct TypeTraits<typeName> { \
    static constexpr TypeId Token() noexcept { \
        return Base::MakeMetaTypeId( \
            Base::StringView("AERO.CPP.VALUE"), \
            Base::StringView(#typeName)); \
    } \
    static TypeId Id() noexcept { \
        return ResolveRuntimeTypeInfo(Token()).id; \
    } \
    static Base::StringView Namespace() noexcept { \
        return ResolveRuntimeTypeInfo(Token()).xamlNamespace; \
    } \
    static Base::StringView Name() noexcept { \
        return ResolveRuntimeTypeInfo(Token()).name; \
    } \
    static TypeId BaseType() noexcept { \
        return ResolveRuntimeTypeInfo(Token()).baseType; \
    } \
    static constexpr MetadataTypeKind Kind() noexcept { \
        return MetadataTypeKind::Struct; \
    } \
}; \
}

constexpr MemberId MakeMemberId(TypeId ownerType, MemberKind kind, Base::StringView name) noexcept {
    constexpr char domain[] = "AERO.MEMBER.V1";
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU64(ownerType);
    builder.AddByte(static_cast<std::uint8_t>(kind));
    builder.AddString(name);
    return builder.Finish();
}
AERO_API MemberId MakeMethodId(TypeId ownerType, Base::StringView name, Base::Span<const TypeId> parameterTypes) noexcept;


} // namespace Aero::Meta

#define AERO_DECLARE_TYPE_NAMED( \
    typeName, metadataBaseType, metadataNamespace, metadataName) \
public: \
    using Self = typeName; \
    using BaseType = metadataBaseType; \
    struct Members { \
        template<class TValue> \
        using Property = Aero::DependencyPropertyRef<Self, TValue>; \
        template<class TValue> \
        using AttachedProperty = Aero::AttachedPropertyRef<Self, TValue>; \
        template<class TValue> \
        using ReadOnlyProperty = Aero::ReadOnlyPropertyRef<Self, TValue>; \
        template<class TArgs> \
        using RoutedEvent = Aero::RoutedEventRef<Self, TArgs>; \
    }; \
    static constexpr Aero::Base::StringView \
    StaticMetadataNamespace() noexcept { \
        return Aero::Base::StringView(metadataNamespace); \
    } \
    static constexpr Aero::Base::StringView \
    StaticMetadataName() noexcept { \
        return Aero::Base::StringView(metadataName); \
    } \
    inline static constexpr Aero::Base::TypeId StaticTypeIdValue_ = \
        Aero::Meta::MakeTypeId( \
            Aero::Base::StringView(metadataNamespace), \
            Aero::Base::StringView(metadataName)); \
    static constexpr Aero::Base::TypeId StaticTypeId() noexcept { \
        return StaticTypeIdValue_; \
    }

#define AERO_DECLARE_TYPE(typeName, metadataBaseType) \
    AERO_DECLARE_TYPE_NAMED( \
        typeName, metadataBaseType, \
        Aero::Meta::AeroNamespaceUri(), #typeName)

namespace Aero::Meta {

// A metadata value that identifies an object type. It is deliberately
// distinct from the uint32_t storage used by TypeId so markup can resolve
// qualified type names without treating every integer property as a type.
struct TypeReference {
    TypeId type = InvalidTypeId;

    constexpr bool IsValid() const noexcept {
        return type != InvalidTypeId;
    }
};

template<>
struct TypeTraits<Value> {
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
struct TypeTraits<TypeReference> {
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
struct TypeTraits<Base::ResourceUri> {
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

AERO_API Base::Result<Value> TryEncodeValue(
    TypeId type,
    const void* source) noexcept;

template<>
struct TypeTraits<bool> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Boolean"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Boolean"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

#define AERO_DEFINE_INTEGER_META_TYPE(cppType, metadataName) \
    template<> \
    struct TypeTraits<cppType> { \
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
struct TypeTraits<double> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Double"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Double"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct TypeTraits<Base::String> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("String"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "String"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<class T, class Enable = void>
struct ValueCodec {
    static TypeId Type() noexcept { return TypeOf<T>(); }

    template<class TMetadata>
    static Base::Result<Value> Encode(
        TMetadata& runtime,
        const T& value) noexcept {
        return runtime.TryCreateValue(Type(), &value);
    }

    static Base::Result<Value> Encode(const T& value) noexcept {
        return TryEncodeValue(Type(), &value);
    }

    template<class TMetadata>
    static Base::Result<T> Decode(
        TMetadata&,
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
    static TypeId Type() noexcept {
        return TypeOf<Value>();
    }
    template<class TMetadata>
    static Base::Result<Value> Encode(
        TMetadata&,
        const Value& value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(
        const Value& value) noexcept {
        return value;
    }
    template<class TMetadata>
    static Base::Result<Value> Decode(
        TMetadata&,
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
    static TypeId Type() noexcept {
        return TypeOf<TypeReference>();
    }
    template<class TMetadata>
    static Base::Result<Value> Encode(
        TMetadata&,
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
    template<class TMetadata>
    static Base::Result<TypeReference> Decode(
        TMetadata&,
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
    static TypeId Type() noexcept { return TypeOf<bool>(); }
    template<class TMetadata>
    static Base::Result<Value> Encode(TMetadata&, bool value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(bool value) noexcept {
        return Value::FromBoolean(Type(), value);
    }
    template<class TMetadata>
    static Base::Result<bool> Decode(
        TMetadata&, const Value& value) noexcept {
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
    static TypeId Type() noexcept { return TypeOf<T>(); }
    template<class TMetadata>
    static Base::Result<Value> Encode(TMetadata&, T value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(T value) noexcept {
        return Value::FromSignedInteger(
            Type(), static_cast<std::int64_t>(value));
    }
    template<class TMetadata>
    static Base::Result<T> Decode(
        TMetadata&, const Value& value) noexcept {
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
    static TypeId Type() noexcept { return TypeOf<T>(); }
    template<class TMetadata>
    static Base::Result<Value> Encode(TMetadata&, T value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(T value) noexcept {
        return Value::FromUnsignedInteger(
            Type(), static_cast<std::uint64_t>(value));
    }
    template<class TMetadata>
    static Base::Result<T> Decode(
        TMetadata&, const Value& value) noexcept {
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
    static TypeId Type() noexcept { return TypeOf<double>(); }
    template<class TMetadata>
    static Base::Result<Value> Encode(TMetadata&, double value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(double value) noexcept {
        return Value::FromDouble(Type(), value);
    }
    template<class TMetadata>
    static Base::Result<double> Decode(
        TMetadata&, const Value& value) noexcept {
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
    static TypeId Type() noexcept { return TypeOf<double>(); }
    template<class TMetadata>
    static Base::Result<Value> Encode(TMetadata&, float value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(float value) noexcept {
        return Value::FromDouble(
            Type(), static_cast<double>(value));
    }
    template<class TMetadata>
    static Base::Result<float> Decode(
        TMetadata&, const Value& value) noexcept {
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
    static TypeId Type() noexcept {
        return TypeOf<Base::String>();
    }
    template<class TMetadata>
    static Base::Result<Value> Encode(
        TMetadata&, const Base::String& value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(
        const Base::String& value) noexcept {
        return Value::TryFromString(Type(), value.View());
    }
    template<class TMetadata>
    static Base::Result<Base::String> Decode(
        TMetadata&, const Value& value) noexcept {
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
            decoded.Assign(value.AsString());
        if (!assigned) return assigned.GetStatus();
        return decoded;
    }
};

template<class T>
struct ValueCodec<T, std::enable_if_t<std::is_enum_v<T>>> {
    using Underlying = std::underlying_type_t<T>;
    static TypeId Type() noexcept { return TypeOf<T>(); }
    template<class TMetadata>
    static Base::Result<Value> Encode(TMetadata&, T value) noexcept {
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
    template<class TMetadata>
    static Base::Result<T> Decode(
        TMetadata&, const Value& value) noexcept {
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
    static TypeId Type() noexcept { return TypeOf<T>(); }
    template<class TMetadata>
    static Base::Result<Value> Encode(
        TMetadata&, const Base::Ref<T>& value) noexcept {
        return Encode(value);
    }
    static Base::Result<Value> Encode(
        const Base::Ref<T>& value) noexcept {
        if (!value) return Value::NullObject(Type());
        return Value::FromObject(
            Type(), Base::Ref<Base::Object>(value));
    }
    template<class TMetadata>
    static Base::Result<Base::Ref<T>> Decode(
        TMetadata&, const Value& value) noexcept {
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

} // namespace Aero::Meta

namespace Aero::Base::Detail {

namespace ValueConversion {

AERO_API Base::StringView Trim(Base::StringView value) noexcept;
AERO_API bool EqualsAsciiInsensitive(
    Base::StringView left,
    Base::StringView right) noexcept;
AERO_API Base::Result<double> ParseDouble(
    Base::StringView text) noexcept;

AERO_API Base::Result<bool> ConvertBoolean(
    Base::StringView text) noexcept;
AERO_API Base::Result<double> ConvertDouble(
    Base::StringView text) noexcept;
AERO_API Base::Result<Base::String> ConvertString(
    Base::StringView text) noexcept;
AERO_API Base::Result<Base::ResourceUri> ConvertResourceUri(
    Base::StringView text) noexcept;

template<class T>
Base::Result<T> ConvertInteger(
    Base::StringView text) noexcept {
    static_assert(
        std::is_integral_v<T> &&
        !std::is_same_v<T, bool>);
    Base::String buffer;
    Base::Result<void> assigned =
        buffer.Assign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    errno = 0;
    if constexpr (std::is_signed_v<T>) {
        const long long value =
            std::strtoll(buffer.CStr(), &end, 10);
        if (end == buffer.CStr() || *end != '\0' ||
            errno == ERANGE ||
            value < static_cast<long long>(
                std::numeric_limits<T>::min()) ||
            value > static_cast<long long>(
                std::numeric_limits<T>::max())) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Text is not a compatible signed integer");
        }
        return static_cast<T>(value);
    } else {
        const unsigned long long value =
            std::strtoull(buffer.CStr(), &end, 10);
        if (end == buffer.CStr() || *end != '\0' ||
            errno == ERANGE ||
            value > static_cast<unsigned long long>(
                std::numeric_limits<T>::max()) ||
            (!buffer.Empty() && buffer.View()[0] == '-')) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Text is not a compatible unsigned integer");
        }
        return static_cast<T>(value);
    }
}

} // namespace ValueConversion

namespace Validate {

template<class T>
bool Finite(const T& value) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        return std::isfinite(value);
    } else {
        return true;
    }
}

template<class T>
bool NonNegative(const T& value) noexcept {
    return Finite(value) && value >= T{0};
}

template<class T>
bool Positive(const T& value) noexcept {
    return Finite(value) && value > T{0};
}

} // namespace Validate

} // namespace Aero::Base::Detail
