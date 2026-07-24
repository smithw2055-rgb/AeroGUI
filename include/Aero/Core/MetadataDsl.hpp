#pragma once

#include <Aero/Core/BuiltinTypeIds.hpp>
#include <Aero/Core/MetadataRuntime.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/ObjectTree.hpp>

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Core {

enum class ContentKind : std::uint8_t {
    Single = 0U,
    Collection
};

template<>
struct MetaTypeTraits<bool> {
    static constexpr TypeId Id() noexcept { return BuiltinTypes::Boolean; }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Boolean"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<std::uint32_t> {
    static constexpr TypeId Id() noexcept {
        return BuiltinTypes::UnsignedInteger;
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "UInt32"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<double> {
    static constexpr TypeId Id() noexcept { return BuiltinTypes::Double; }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Double"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Base::String> {
    static constexpr TypeId Id() noexcept { return BuiltinTypes::String; }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "String"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

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

template<>
struct MetaValueCodec<float, void> {
    static constexpr TypeId Type() noexcept { return BuiltinTypes::Double; }
    static Base::Result<Value> Encode(
        MetadataRuntime&, float value) noexcept {
        return Value::FromDouble(Type(), static_cast<double>(value));
    }
    static Base::Result<float> Decode(
        MetadataRuntime&, const Value& value) noexcept {
        if (value.Type() != Type() || value.Kind() != ValueKind::Double) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Float metadata field is incompatible");
        }
        return static_cast<float>(value.AsDouble());
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

    static MetaTypeBuilder Object(
        MetaRegistrationContext& context,
        TypeFlags flags = TypeFlags::None) noexcept {
        return MetaTypeBuilder(context, TypeRegistration::Object(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(),
            MetaTypeTraits<T>::BaseType(), flags));
    }

    static MetaTypeBuilder Interface(
        MetaRegistrationContext& context,
        TypeFlags flags = TypeFlags::None) noexcept {
        return MetaTypeBuilder(context, TypeRegistration::Interface(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(), flags));
    }

    static MetaTypeBuilder Struct(
        MetaRegistrationContext& context,
        TypeFlags flags = TypeFlags::None) noexcept {
        if constexpr (std::is_trivially_copyable_v<T>) {
            flags = flags | TypeFlags::TriviallyCopyable;
        }
        return MetaTypeBuilder(context, TypeRegistration::Struct(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(),
            MetaTypeTraits<T>::BaseType(), flags));
    }

    static MetaTypeBuilder Enum(
        MetaRegistrationContext& context,
        TypeId underlyingType,
        TypeFlags flags = TypeFlags::None) noexcept {
        static_assert(std::is_enum_v<T>,
            "MetaTypeBuilder::Enum requires an enum type");
        if constexpr (std::is_signed_v<std::underlying_type_t<T>>) {
            flags = flags | TypeFlags::SignedEnum;
        }
        return MetaTypeBuilder(context, TypeRegistration::Enum(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(),
            underlyingType, flags));
    }

    static MetaTypeBuilder Primitive(
        MetaRegistrationContext& context,
        TypeFlags flags = TypeFlags::None) noexcept {
        return MetaTypeBuilder(context, TypeRegistration::Primitive(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(), flags));
    }

    MetaTypeBuilder& Implements(TypeId interfaceType) noexcept {
        if (Ok()) Record(context_->Types().TryRegisterInterface(type_, interfaceType));
        return *this;
    }
    template<class Interface> MetaTypeBuilder& Implements() noexcept { return Implements(TypeOf<Interface>()); }
    MetaTypeBuilder& Factory(ObjectFactory factory) noexcept {
        if (Ok()) Record(context_->Types().TrySetFactory(type_, factory));
        return *this;
    }

    MetaTypeBuilder& DependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView name,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr) noexcept {
        return RegisterDependencyProperty(declaredHandle, name, valueType,
            std::move(defaultValue), metadataFlags,
            DependencyPropertyFlags::None, validate, coerce);
    }

    MetaTypeBuilder& AttachedDependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView name,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr) noexcept {
        return RegisterDependencyProperty(declaredHandle, name, valueType,
            std::move(defaultValue), metadataFlags,
            DependencyPropertyFlags::Attached, validate, coerce);
    }

    MetaTypeBuilder& RoutedEvent(
        RoutedEventHandle declaredHandle,
        Base::StringView name,
        TypeId eventArgsType,
        RoutingStrategy strategy) noexcept {
        if (!Ok()) return *this;
        RoutedEventRegistry* events = context_->RoutedEvents();
        if (events == nullptr) {
            return Fail(Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Routed event metadata requires a registry"));
        }
        if (name.Empty() ||
            declaredHandle != MakeRoutedEventHandle(type_, name)) {
            return Fail(Base::Status::Failure(Base::ErrorCode::IdCollision,
                "Typed routed event handle does not match owner and name"));
        }
        Base::Result<RoutedEventHandle> registered = events->TryRegister(
            {name, type_, eventArgsType, strategy});
        if (!registered) {
            return Fail(registered.GetStatus());
        }
        if (registered.Value() != declaredHandle) {
            return Fail(Base::Status::Failure(Base::ErrorCode::IdCollision,
                "Routed event registry returned a different handle"));
        }
        return *this;
    }

    MetaTypeBuilder& Content(
        Base::StringView name,
        ContentKind kind) noexcept {
        if (!Ok()) return *this;
        PropertyFlags flags = PropertyFlags::Structural;
        if (kind == ContentKind::Collection) {
            flags = flags | PropertyFlags::Collection;
        }
        Base::Result<MemberId> member = context_->Types().TryRegisterProperty(
            type_, {name, BuiltinTypes::UIElement, flags});
        if (!member) return Fail(member.GetStatus());
        Record(context_->Types().TrySetContentMember(type_, member.Value()));
        return *this;
    }

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
    MetaTypeBuilder& ValueSemantics(
        const ValueTypeRegistration& registration) noexcept {
        if (Ok()) {
            Record(context_->Values().TryRegisterValueSemantics(
                type_, registration));
        }
        return *this;
    }

    MetaTypeBuilder& ValueSemantics() noexcept {
        return ValueSemantics(Detail::MakeValueTypeRegistration<T>());
    }
    MetaTypeBuilder& TextConverter(TextValueConverterCallback converter, void* context = nullptr) noexcept {
        if (Ok()) Record(context_->Values().TryRegisterTextConverter({type_, converter, context}));
        return *this;
    }
    MetaTypeBuilder& Fail(Base::Status status) noexcept {
        if (status_.IsOk() && !status.IsOk()) status_ = status;
        return *this;
    }

    bool Ok() const noexcept { return status_.IsOk(); }
    TypeId Type() const noexcept { return type_; }
    Base::Status Status() const noexcept { return status_; }
    Base::Result<void> Finish() const noexcept { return status_.IsOk() ? Base::Result<void>() : Base::Result<void>(status_); }

private:
    MetaTypeBuilder& RegisterDependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView name,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        DependencyPropertyFlags propertyFlags,
        ValidateValueCallback validate,
        CoerceValueCallback coerce) noexcept {
        if (!Ok()) return *this;
        if (name.Empty() ||
            declaredHandle != MakeDependencyPropertyHandle(type_, name)) {
            return Fail(Base::Status::Failure(Base::ErrorCode::IdCollision,
                "Typed dependency property handle does not match owner and name"));
        }
        DependencyPropertyRegistration registration;
        registration.name = name;
        registration.ownerType = type_;
        registration.valueType = valueType;
        registration.flags = propertyFlags;
        registration.metadata.defaultValue = std::move(defaultValue);
        registration.metadata.flags = metadataFlags;
        registration.metadata.validate = validate;
        registration.metadata.coerce = coerce;
        Base::Result<DependencyPropertyRegistrationResult> registered =
            context_->DependencyProperties().TryRegister(registration);
        if (!registered) return Fail(registered.GetStatus());
        if (registered.Value().property != declaredHandle) {
            return Fail(Base::Status::Failure(Base::ErrorCode::IdCollision,
                "Dependency property registry returned a different handle"));
        }
        return *this;
    }

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
