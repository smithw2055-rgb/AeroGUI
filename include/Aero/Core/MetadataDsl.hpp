#pragma once

#include <Aero/Core/BuiltinTypeIds.hpp>
#include <Aero/Core/MetadataRuntime.hpp>
#include <Aero/Core/Presentation.hpp>

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Core {
namespace Detail {

template<class T, class = void>
struct HasEquality final : std::false_type {};

template<class T>
struct HasEquality<T, std::void_t<decltype(
    std::declval<const T&>() == std::declval<const T&>())>> final
    : std::true_type {};

template<class T>
Base::Result<void> CopyValue(
    void* destination,
    const void* source,
    void*) noexcept {
    if (destination == nullptr || source == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Value semantics copy received null storage");
    }
    new (destination) T(*static_cast<const T*>(source));
    return {};
}

template<class T>
void DestroyValue(void* value, void*) noexcept {
    if (value != nullptr) static_cast<T*>(value)->~T();
}

template<class T>
bool EqualValue(
    const void* left,
    const void* right,
    void*) noexcept {
    return *static_cast<const T*>(left) == *static_cast<const T*>(right);
}

template<class T>
bool EqualBytes(
    const void* left,
    const void* right,
    void*) noexcept {
    return std::memcmp(left, right, sizeof(T)) == 0;
}

template<class T>
constexpr ValueEqualsCallback EqualityCallback() noexcept {
    if constexpr (HasEquality<T>::value) {
        return &EqualValue<T>;
    } else if constexpr (std::is_trivially_copyable_v<T>) {
        return &EqualBytes<T>;
    } else {
        return nullptr;
    }
}

template<class T>
ValueTypeRegistration MakeValueTypeRegistration() noexcept {
    ValueTypeRegistration registration;
    registration.size = static_cast<std::uint32_t>(sizeof(T));
    registration.alignment = static_cast<std::uint32_t>(alignof(T));
    registration.copy = &CopyValue<T>;
    registration.destroy = std::is_trivially_destructible_v<T>
        ? nullptr : &DestroyValue<T>;
    registration.equals = EqualityCallback<T>();
    registration.inlineSafe =
        std::is_trivially_copyable_v<T> &&
        sizeof(T) <= Value::InlineCapacity &&
        alignof(T) <= alignof(std::max_align_t);
    return registration;
}

template<auto Member>
struct MemberPointerTraits;

template<class Owner, class Field, Field Owner::*Member>
struct MemberPointerTraits<Member> final {
    using OwnerType = Owner;
    using FieldType = Field;
};

} // namespace Detail

template<class T, class Enable = void>
struct MetaValueCodec {
    static constexpr TypeId Type() noexcept { return TypeOf<T>(); }

    static Base::Result<Value> Encode(
        MetadataRuntime& runtime,
        const T& value) noexcept {
        return runtime.TryCreateValue(Type(), &value);
    }

    static Base::Result<T> Decode(
        MetadataRuntime&,
        const Value& value) noexcept {
        if (value.Type() != Type() || value.Kind() != ValueKind::Custom) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata field value type is incompatible");
        }
        return *static_cast<const T*>(value.AsCustom());
    }
};

template<>
struct MetaValueCodec<bool, void> {
    static constexpr TypeId Type() noexcept { return BuiltinTypes::Boolean; }
    static Base::Result<Value> Encode(
        MetadataRuntime&, bool value) noexcept {
        return Value::FromBoolean(Type(), value);
    }
    static Base::Result<bool> Decode(
        MetadataRuntime&, const Value& value) noexcept {
        if (value.Type() != Type() || value.Kind() != ValueKind::Boolean) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Boolean metadata field is incompatible");
        }
        return value.AsBoolean();
    }
};

template<>
struct MetaValueCodec<std::uint32_t, void> {
    static constexpr TypeId Type() noexcept {
        return BuiltinTypes::UnsignedInteger;
    }
    static Base::Result<Value> Encode(
        MetadataRuntime&, std::uint32_t value) noexcept {
        return Value::FromUnsignedInteger(Type(), value);
    }
    static Base::Result<std::uint32_t> Decode(
        MetadataRuntime&, const Value& value) noexcept {
        if (value.Type() != Type() ||
            value.Kind() != ValueKind::UnsignedInteger ||
            value.AsUnsignedInteger() > UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Unsigned metadata field is incompatible");
        }
        return static_cast<std::uint32_t>(value.AsUnsignedInteger());
    }
};

template<>
struct MetaValueCodec<double, void> {
    static constexpr TypeId Type() noexcept { return BuiltinTypes::Double; }
    static Base::Result<Value> Encode(
        MetadataRuntime&, double value) noexcept {
        return Value::FromDouble(Type(), value);
    }
    static Base::Result<double> Decode(
        MetadataRuntime&, const Value& value) noexcept {
        if (value.Type() != Type() || value.Kind() != ValueKind::Double) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Double metadata field is incompatible");
        }
        return value.AsDouble();
    }
};

template<class T>
struct MetaValueCodec<T, std::enable_if_t<std::is_enum_v<T>>> {
    using Underlying = std::underlying_type_t<T>;
    static constexpr TypeId Type() noexcept { return TypeOf<T>(); }
    static Base::Result<Value> Encode(MetadataRuntime&, T value) noexcept {
        if constexpr (std::is_signed_v<Underlying>) {
            return Value::FromSignedInteger(Type(), static_cast<std::int64_t>(static_cast<Underlying>(value)));
        } else {
            return Value::FromUnsignedInteger(Type(), static_cast<std::uint64_t>(static_cast<Underlying>(value)));
        }
    }
    static Base::Result<T> Decode(MetadataRuntime&, const Value& value) noexcept {
        if (value.Type() != Type()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Enum metadata field type is incompatible");
        }
        if constexpr (std::is_signed_v<Underlying>) {
            if (value.Kind() != ValueKind::SignedInteger) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Signed enum metadata field is incompatible");
            return static_cast<T>(static_cast<Underlying>(value.AsSignedInteger()));
        } else {
            if (value.Kind() != ValueKind::UnsignedInteger) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Unsigned enum metadata field is incompatible");
            return static_cast<T>(static_cast<Underlying>(value.AsUnsignedInteger()));
        }
    }
};

namespace Detail {

template<class Owner, class Field, Field Owner::*Member>
Base::Result<Value> GetField(const void* object, MetadataRuntime& runtime, void*) noexcept {
    if (object == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata value field target is null");
    return MetaValueCodec<Field>::Encode(runtime, static_cast<const Owner*>(object)->*Member);
}

template<class Owner, class Field, Field Owner::*Member>
Base::Result<void> SetField(void* object, const Value& value, MetadataRuntime& runtime, void*) noexcept {
    if (object == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata value field target is null");
    Base::Result<Field> decoded = MetaValueCodec<Field>::Decode(runtime, value);
    if (!decoded) return decoded.GetStatus();
    static_cast<Owner*>(object)->*Member = std::move(decoded).Value();
    return {};
}

} // namespace Detail

template<class T>
class MetaTypeBuilder final {
public:
    MetaTypeBuilder(const MetaTypeBuilder&) = delete;
    MetaTypeBuilder& operator=(const MetaTypeBuilder&) = delete;
    MetaTypeBuilder(MetaTypeBuilder&&) noexcept = default;
    MetaTypeBuilder& operator=(MetaTypeBuilder&&) noexcept = default;

    static MetaTypeBuilder Object(MetaRegistrationContext& context, TypeFlags flags = TypeFlags::None) noexcept {
        return MetaTypeBuilder(context, {MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(), MetaTypeTraits<T>::BaseType(), flags, nullptr, MetadataTypeKind::Object});
    }
    static MetaTypeBuilder Interface(MetaRegistrationContext& context, TypeFlags flags = TypeFlags::None) noexcept {
        return MetaTypeBuilder(context, {MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(), InvalidTypeId, flags | TypeFlags::Abstract, nullptr, MetadataTypeKind::Interface});
    }
    static MetaTypeBuilder Struct(MetaRegistrationContext& context, TypeFlags flags = TypeFlags::None) noexcept {
        if constexpr (std::is_trivially_copyable_v<T>) flags = flags | TypeFlags::TriviallyCopyable;
        return MetaTypeBuilder(context, {MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(), InvalidTypeId, flags | TypeFlags::ValueType, nullptr, MetadataTypeKind::Struct});
    }
    static MetaTypeBuilder Enum(MetaRegistrationContext& context, TypeId underlyingType, TypeFlags flags = TypeFlags::None) noexcept {
        static_assert(std::is_enum_v<T>, "MetaTypeBuilder::Enum requires an enum type");
        if constexpr (std::is_signed_v<std::underlying_type_t<T>>) flags = flags | TypeFlags::SignedEnum;
        return MetaTypeBuilder(context, {MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(), InvalidTypeId, flags | TypeFlags::ValueType, nullptr, MetadataTypeKind::Enum, underlyingType});
    }

    MetaTypeBuilder& Implements(TypeId interfaceType) noexcept {
        if (Ok()) Record(context_->Types().TryRegisterInterface(type_, interfaceType));
        return *this;
    }
    template<class Interface> MetaTypeBuilder& Implements() noexcept { return Implements(TypeOf<Interface>()); }
    MetaTypeBuilder& Factory(ObjectFactory factory) noexcept { if (Ok()) Record(context_->Types().TrySetFactory(type_, factory)); return *this; }
    MetaTypeBuilder& Property(const PropertyRegistration& registration) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterProperty(type_, registration); Record(result); } return *this; }
    MetaTypeBuilder& Field(const FieldRegistration& registration) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterField(type_, registration); Record(result); } return *this; }

    template<auto Member>
    MetaTypeBuilder& Field(Base::StringView name, FieldFlags flags = FieldFlags::None) noexcept {
        using Traits = Detail::MemberPointerTraits<Member>;
        using Owner = typename Traits::OwnerType;
        using FieldType = typename Traits::FieldType;
        static_assert(std::is_same_v<Owner, T>, "Metadata field member must belong to the described struct");
        return Field({name, MetaValueCodec<FieldType>::Type(), flags, &Detail::GetField<Owner, FieldType, Member>, &Detail::SetField<Owner, FieldType, Member>, nullptr});
    }

    MetaTypeBuilder& Event(const EventRegistration& registration) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterEvent(type_, registration); Record(result); } return *this; }
    MetaTypeBuilder& Method(const MethodRegistration& registration) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterMethod(type_, registration); Record(result); } return *this; }
    MetaTypeBuilder& EnumValue(Base::StringView name, std::uint64_t rawValue) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterEnumValue(type_, {name, rawValue}); Record(result); } return *this; }
    MetaTypeBuilder& EnumValue(Base::StringView name, T value) noexcept {
        static_assert(std::is_enum_v<T>, "Typed EnumValue requires an enum builder");
        using Underlying = std::underlying_type_t<T>;
        using Unsigned = std::make_unsigned_t<Underlying>;
        return EnumValue(name, static_cast<std::uint64_t>(static_cast<Unsigned>(static_cast<Underlying>(value))));
    }
    MetaTypeBuilder& Content(MemberId member) noexcept { if (Ok()) Record(context_->Types().TrySetContentMember(type_, member)); return *this; }
    MetaTypeBuilder& ValueSemantics() noexcept {
        if (Ok()) { auto result = context_->Values().TryRegisterValueSemantics(type_, Detail::MakeValueTypeRegistration<T>()); Record(result); }
        return *this;
    }
    MetaTypeBuilder& TextConverter(TextValueConverterCallback converter, void* context = nullptr) noexcept {
        if (Ok()) Record(context_->Values().TryRegisterTextConverter({type_, converter, context}));
        return *this;
    }
    bool Ok() const noexcept { return status_.IsOk(); }
    TypeId Type() const noexcept { return type_; }
    Base::Status Status() const noexcept { return status_; }
    Base::Result<void> Finish() const noexcept { return status_.IsOk() ? Base::Result<void>() : Base::Result<void>(status_); }

private:
    MetaTypeBuilder(MetaRegistrationContext& context, const TypeRegistration& registration) noexcept : context_(&context) {
        Base::Result<TypeId> result = context_->Types().TryRegisterType(registration);
        if (!result) { status_ = result.GetStatus(); return; }
        type_ = result.Value();
        if (type_ != TypeOf<T>()) status_ = Base::Status::Failure(Base::ErrorCode::IdCollision, "Typed metadata descriptor does not match TypeOf<T>()");
    }
    void Record(Base::Result<void> result) noexcept { if (status_.IsOk() && !result) status_ = result.GetStatus(); }
    template<class U> void Record(Base::Result<U>& result) noexcept { if (status_.IsOk() && !result) status_ = result.GetStatus(); }
    MetaRegistrationContext* context_ = nullptr;
    TypeId type_ = InvalidTypeId;
    Base::Status status_;
};

} // namespace Aero::Core

#define AERO_DEFINE_META_TYPE(typeName, metadataNamespace, metadataName) \
    namespace Aero::Core { template<> struct MetaTypeTraits<typeName> { \
        static constexpr TypeId Id() noexcept { return MakeTypeId(Aero::Base::StringView(metadataNamespace), Aero::Base::StringView(metadataName)); } \
        static constexpr Aero::Base::StringView Namespace() noexcept { return Aero::Base::StringView(metadataNamespace); } \
        static constexpr Aero::Base::StringView Name() noexcept { return Aero::Base::StringView(metadataName); } \
        static constexpr TypeId BaseType() noexcept { return InvalidTypeId; } \
    }; }
