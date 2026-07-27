#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Events/RoutedEventCatalog.hpp>
#include <Aero/Core/Metadata/MetadataDsl.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace Aero::Core {

using RegistrationContext = MetaRegistrationContext;

namespace MetadataDetail {

template<class T>
constexpr TypeId ValueType() noexcept {
    if constexpr (std::is_same_v<T, float>) {
        return TypeOf<double>();
    } else {
        return TypeOf<T>();
    }
}

template<class T>
Base::Result<Value> EncodeRegistrationValue(
    RegistrationContext& context,
    const T& value) noexcept {
    const TypeId type = ValueType<T>();
    if constexpr (std::is_same_v<T, bool>) {
        return Value::FromBoolean(type, value);
    } else if constexpr (std::is_same_v<T, std::uint32_t>) {
        return Value::FromUnsignedInteger(type, value);
    } else if constexpr (std::is_same_v<T, double> ||
        std::is_same_v<T, float>) {
        return Value::FromDouble(
            type, static_cast<double>(value));
    } else if constexpr (std::is_same_v<T, Base::String>) {
        return Value::TryFromString(type, value.View());
    } else if constexpr (std::is_enum_v<T>) {
        using Underlying = std::underlying_type_t<T>;
        if constexpr (std::is_signed_v<Underlying>) {
            return Value::FromSignedInteger(
                type,
                static_cast<std::int64_t>(
                    static_cast<Underlying>(value)));
        } else {
            return Value::FromUnsignedInteger(
                type,
                static_cast<std::uint64_t>(
                    static_cast<Underlying>(value)));
        }
    } else {
        return context.Values().TryCreateValue(
            type, &value);
    }
}

template<class T>
Base::Result<Value> EncodeRuntimeValue(
    const T& value) noexcept {
    const TypeId type = ValueType<T>();
    if constexpr (std::is_same_v<T, bool>) {
        return Value::FromBoolean(type, value);
    } else if constexpr (std::is_same_v<T, std::uint32_t>) {
        return Value::FromUnsignedInteger(type, value);
    } else if constexpr (std::is_same_v<T, double> ||
        std::is_same_v<T, float>) {
        return Value::FromDouble(
            type, static_cast<double>(value));
    } else if constexpr (std::is_same_v<T, Base::String>) {
        return Value::TryFromString(type, value.View());
    } else if constexpr (std::is_enum_v<T>) {
        using Underlying = std::underlying_type_t<T>;
        if constexpr (std::is_signed_v<Underlying>) {
            return Value::FromSignedInteger(
                type,
                static_cast<std::int64_t>(
                    static_cast<Underlying>(value)));
        } else {
            return Value::FromUnsignedInteger(
                type,
                static_cast<std::uint64_t>(
                    static_cast<Underlying>(value)));
        }
    } else {
        return TryCreateRuntimeValue(type, &value);
    }
}

template<class T>
Base::Result<T> DecodeValue(
    const Value& value) noexcept {
    const TypeId type = ValueType<T>();
    if (value.Type() != type) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Property value type is incompatible");
    }

    if constexpr (std::is_same_v<T, bool>) {
        if (value.Kind() != ValueKind::Boolean) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property value is not Boolean");
        }
        return value.AsBoolean();
    } else if constexpr (std::is_same_v<T, std::uint32_t>) {
        if (value.Kind() != ValueKind::UnsignedInteger ||
            value.AsUnsignedInteger() > UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property value is not UInt32");
        }
        return static_cast<std::uint32_t>(
            value.AsUnsignedInteger());
    } else if constexpr (std::is_same_v<T, double>) {
        if (value.Kind() != ValueKind::Double) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property value is not Double");
        }
        return value.AsDouble();
    } else if constexpr (std::is_same_v<T, float>) {
        if (value.Kind() != ValueKind::Double) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property value is not a floating-point number");
        }
        return static_cast<float>(value.AsDouble());
    } else if constexpr (std::is_same_v<T, Base::String>) {
        if (value.Kind() != ValueKind::String) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property value is not String");
        }
        Base::String decoded;
        Base::Result<void> assigned =
            decoded.TryAssign(value.AsString());
        if (!assigned) return assigned.GetStatus();
        return decoded;
    } else if constexpr (std::is_enum_v<T>) {
        using Underlying = std::underlying_type_t<T>;
        if constexpr (std::is_signed_v<Underlying>) {
            if (value.Kind() != ValueKind::SignedInteger) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Property value is not a signed enum");
            }
            return static_cast<T>(
                static_cast<Underlying>(
                    value.AsSignedInteger()));
        } else {
            if (value.Kind() != ValueKind::UnsignedInteger) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Property value is not an unsigned enum");
            }
            return static_cast<T>(
                static_cast<Underlying>(
                    value.AsUnsignedInteger()));
        }
    } else {
        if (value.Kind() != ValueKind::Custom ||
            value.AsCustom() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property value is not a custom value");
        }
        return *static_cast<const T*>(value.AsCustom());
    }
}

template<
    class TOwner,
    class TValue,
    MemberKind TKind,
    class TRawHandle>
class TypedMember {
public:
    constexpr explicit TypedMember(
        Base::StringView name) noexcept
        : name_(name) {}

    constexpr Base::StringView Name() const noexcept {
        return name_;
    }

    static constexpr TypeId OwnerType() noexcept {
        return TypeOf<TOwner>();
    }

    static constexpr TypeId MemberType() noexcept {
        return ValueType<TValue>();
    }

    constexpr TRawHandle Handle() const noexcept {
        return {MakeMemberId(OwnerType(), TKind, name_)};
    }

    constexpr operator TRawHandle() const noexcept {
        return Handle();
    }

private:
    Base::StringView name_;
};

} // namespace MetadataDetail

template<class TOwner, class TValue>
class DependencyPropertyRef final
    : public MetadataDetail::TypedMember<
        TOwner,
        TValue,
        MemberKind::Property,
        DependencyPropertyHandle> {
    using Member = MetadataDetail::TypedMember<
        TOwner,
        TValue,
        MemberKind::Property,
        DependencyPropertyHandle>;

public:
    constexpr explicit DependencyPropertyRef(
        Base::StringView name) noexcept
        : Member(name) {}

    static constexpr TypeId ValueType() noexcept {
        return MetadataDetail::ValueType<TValue>();
    }

    Base::Result<TValue> Get(
        const DependencyObject& object) const noexcept {
        Base::Result<Value> value =
            object.GetValue(this->Handle());
        if (!value) return value.GetStatus();
        return MetadataDetail::DecodeValue<TValue>(
            value.Value());
    }

    Base::Result<void> Set(
        DependencyObject& object,
        const TValue& value) const noexcept {
        Base::Result<Value> encoded =
            MetadataDetail::EncodeRuntimeValue(value);
        if (!encoded) return encoded.GetStatus();
        return object.SetValue(
            this->Handle(), encoded.Value());
    }

    Base::Result<void> SetCurrent(
        DependencyObject& object,
        const TValue& value) const noexcept {
        Base::Result<Value> encoded =
            MetadataDetail::EncodeRuntimeValue(value);
        if (!encoded) return encoded.GetStatus();
        return object.SetCurrentValue(
            this->Handle(), encoded.Value());
    }

    Base::Result<void> Clear(
        DependencyObject& object) const noexcept {
        return object.ClearValue(this->Handle());
    }
};

template<class TOwner, class TEventArgs>
class RoutedEventRef final
    : public MetadataDetail::TypedMember<
        TOwner,
        TEventArgs,
        MemberKind::Event,
        RoutedEventHandle> {
    using Member = MetadataDetail::TypedMember<
        TOwner,
        TEventArgs,
        MemberKind::Event,
        RoutedEventHandle>;

public:
    constexpr explicit RoutedEventRef(
        Base::StringView name) noexcept
        : Member(name) {}

    static constexpr TypeId EventArgsType() noexcept {
        return MetadataDetail::ValueType<TEventArgs>();
    }
};

template<class TOwner, class TValue>
constexpr DependencyPropertyRef<TOwner, TValue>
DefineProperty(Base::StringView name) noexcept {
    return DependencyPropertyRef<TOwner, TValue>(name);
}

template<class TOwner, class TEventArgs>
constexpr RoutedEventRef<TOwner, TEventArgs>
DefineEvent(Base::StringView name) noexcept {
    return RoutedEventRef<TOwner, TEventArgs>(name);
}

template<class T>
class TypeBuilder final {
public:
    TypeBuilder(
        RegistrationContext& context,
        MetaTypeBuilder<T>&& builder) noexcept
        : context_(&context),
          builder_(std::move(builder)) {}

    TypeBuilder(const TypeBuilder&) = delete;
    TypeBuilder& operator=(const TypeBuilder&) = delete;
    TypeBuilder(TypeBuilder&&) noexcept = default;
    TypeBuilder& operator=(TypeBuilder&&) noexcept = default;

    TypeBuilder& Factory() noexcept {
        builder_.DefaultFactory();
        return *this;
    }

    template<class TOwner, class TValue>
    TypeBuilder& Property(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& defaultValue,
        PropertyMetadataFlags flags =
            PropertyMetadataFlags::None,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr) noexcept {
        return RegisterProperty(
            property,
            MetadataDetail::EncodeRegistrationValue(
                *context_, defaultValue),
            flags,
            validate,
            coerce,
            DependencyPropertyFlags::None);
    }

    template<class TOwner, class TValue>
    TypeBuilder& Property(
        const DependencyPropertyRef<TOwner, TValue>& property,
        Value defaultValue,
        PropertyMetadataFlags flags =
            PropertyMetadataFlags::None,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr) noexcept {
        return RegisterProperty(
            property,
            Base::Result<Value>(std::move(defaultValue)),
            flags,
            validate,
            coerce,
            DependencyPropertyFlags::None);
    }

    template<class TOwner, class TValue>
    TypeBuilder& ReadOnlyProperty(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& defaultValue,
        PropertyMetadataFlags flags =
            PropertyMetadataFlags::None,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr) noexcept {
        return RegisterProperty(
            property,
            MetadataDetail::EncodeRegistrationValue(
                *context_, defaultValue),
            flags,
            validate,
            coerce,
            DependencyPropertyFlags::ReadOnly);
    }

    template<class TOwner, class TValue>
    TypeBuilder& AttachedProperty(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& defaultValue,
        PropertyMetadataFlags flags =
            PropertyMetadataFlags::None,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr) noexcept {
        return RegisterProperty(
            property,
            MetadataDetail::EncodeRegistrationValue(
                *context_, defaultValue),
            flags,
            validate,
            coerce,
            DependencyPropertyFlags::Attached);
    }

    template<class TOwner, class TEventArgs>
    TypeBuilder& Event(
        const RoutedEventRef<TOwner, TEventArgs>& event,
        RoutingStrategy strategy =
            RoutingStrategy::Bubble) noexcept {
        static_assert(
            std::is_same_v<TOwner, T>,
            "Routed event owner must match the described type");
        if (context_->RoutedEvents() != nullptr) {
            builder_.RoutedEvent(
                event.Handle(),
                event.Name(),
                event.EventArgsType(),
                strategy);
        }
        return *this;
    }

    template<class TOwner, class TValue>
    TypeBuilder& Override(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& defaultValue,
        PropertyMetadataFlags flags =
            PropertyMetadataFlags::None,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr,
        PropertyChangedCallback changed = nullptr) noexcept {
        Base::Result<Value> encoded =
            MetadataDetail::EncodeRegistrationValue(
                *context_, defaultValue);
        if (!encoded) {
            builder_.Fail(encoded.GetStatus());
            return *this;
        }
        PropertyMetadata metadata;
        metadata.defaultValue = std::move(encoded).Value();
        metadata.flags = flags;
        metadata.validate = validate;
        metadata.coerce = coerce;
        metadata.changed = changed;
        Base::Result<void> result =
            context_->DependencyProperties().
                TryOverrideMetadata(
                    property.Handle(),
                    TypeOf<T>(),
                    metadata);
        if (!result) builder_.Fail(result.GetStatus());
        return *this;
    }

    template<class TInterface>
    TypeBuilder& Implements() noexcept {
        builder_.template Implements<TInterface>();
        return *this;
    }

    template<class TValue>
    TypeBuilder& Content(
        Base::StringView name,
        ContentKind kind,
        ContentWriteCallback write = nullptr,
        ContentClearCallback clear = nullptr,
        ContentFlags flags = ContentFlags::None,
        void* contentContext = nullptr) noexcept {
        builder_.template Content<TValue>(
            name,
            kind,
            write,
            clear,
            flags,
            contentContext);
        return *this;
    }

    template<auto TMember>
    TypeBuilder& Field(
        Base::StringView name,
        FieldFlags flags = FieldFlags::None) noexcept {
        builder_.template Field<TMember>(name, flags);
        return *this;
    }

    TypeBuilder& EnumValue(
        Base::StringView name,
        T value) noexcept {
        builder_.EnumValue(name, value);
        return *this;
    }

    TypeBuilder& ValueSemantics(
        const ValueTypeRegistration& registration) noexcept {
        builder_.ValueSemantics(registration);
        return *this;
    }

    TypeBuilder& ValueSemantics() noexcept {
        builder_.ValueSemantics();
        return *this;
    }

    TypeBuilder& TextConverter(
        TextValueConverterCallback converter,
        void* converterContext = nullptr) noexcept {
        builder_.TextConverter(
            converter, converterContext);
        return *this;
    }

    MetaTypeBuilder<T>& Advanced() noexcept {
        return builder_;
    }

    const MetaTypeBuilder<T>& Advanced() const noexcept {
        return builder_;
    }

    bool Ok() const noexcept {
        return builder_.Ok();
    }

    Base::Status Status() const noexcept {
        return builder_.Status();
    }

    Base::Result<void> Finish() const noexcept {
        return builder_.Finish();
    }

private:
    template<class TOwner, class TValue>
    TypeBuilder& RegisterProperty(
        const DependencyPropertyRef<TOwner, TValue>& property,
        Base::Result<Value> defaultValue,
        PropertyMetadataFlags flags,
        ValidateValueCallback validate,
        CoerceValueCallback coerce,
        DependencyPropertyFlags propertyFlags) noexcept {
        static_assert(
            std::is_same_v<TOwner, T>,
            "Dependency property owner must match the described type");
        if (!defaultValue) {
            builder_.Fail(defaultValue.GetStatus());
            return *this;
        }

        if (propertyFlags ==
            DependencyPropertyFlags::ReadOnly) {
            builder_.ReadOnlyDependencyProperty(
                property.Handle(),
                property.Name(),
                property.ValueType(),
                std::move(defaultValue).Value(),
                flags,
                validate,
                coerce);
        } else if (propertyFlags ==
            DependencyPropertyFlags::Attached) {
            builder_.AttachedDependencyProperty(
                property.Handle(),
                property.Name(),
                property.ValueType(),
                std::move(defaultValue).Value(),
                flags,
                validate,
                coerce);
        } else {
            builder_.DependencyProperty(
                property.Handle(),
                property.Name(),
                property.ValueType(),
                std::move(defaultValue).Value(),
                flags,
                validate,
                coerce);
        }
        return *this;
    }

    RegistrationContext* context_ = nullptr;
    MetaTypeBuilder<T> builder_;
};

template<class T>
TypeBuilder<T> Describe(
    RegistrationContext& context,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeBuilder<T>(
        context,
        MetaTypeBuilder<T>::Object(context, flags));
}

template<class T>
TypeBuilder<T> DescribeInterface(
    RegistrationContext& context,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeBuilder<T>(
        context,
        MetaTypeBuilder<T>::Interface(context, flags));
}

template<class T>
TypeBuilder<T> DescribeStruct(
    RegistrationContext& context,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeBuilder<T>(
        context,
        MetaTypeBuilder<T>::Struct(context, flags));
}

template<class T, class TUnderlying = std::uint32_t>
TypeBuilder<T> DescribeEnum(
    RegistrationContext& context,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeBuilder<T>(
        context,
        MetaTypeBuilder<T>::Enum(
            context,
            TypeOf<TUnderlying>(),
            flags));
}

template<class T>
TypeBuilder<T> DescribePrimitive(
    RegistrationContext& context,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeBuilder<T>(
        context,
        MetaTypeBuilder<T>::Primitive(context, flags));
}

} // namespace Aero::Core
