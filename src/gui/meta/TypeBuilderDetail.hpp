#pragma once

// TypeBuilder authoring detail: MetadataAuthoringSession, description-session
// factories, and fluent helper adapters. Included from <Aero/Meta.hpp>.
// Canonical path: src/gui/meta (exposed via aero-meta-authoring include root).

#include "gui/meta/MetadataRegistrations.hpp"

#include <cstring>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Meta {

template<class T, class = void>
struct HasEquality : std::false_type {};

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
    return *static_cast<const T*>(left) ==
        *static_cast<const T*>(right);
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
    registration.alignment =
        static_cast<std::uint32_t>(alignof(T));
    registration.copy = &CopyValue<T>;
    registration.destroy =
        std::is_trivially_destructible_v<T>
        ? nullptr
        : &DestroyValue<T>;
    registration.equals = EqualityCallback<T>();
    registration.inlineSafe =
        std::is_trivially_copyable_v<T> &&
        sizeof(T) <= Value::InlineCapacity &&
        alignof(T) <= alignof(std::max_align_t);
    return registration;
}

template<class T>
Base::Result<Base::Ref<Base::Object>>
CreateDefaultObject() noexcept {
    static_assert(std::is_base_of_v<Base::Object, T>,
        "Default metadata factories require Object-derived types");
    static_assert(std::is_default_constructible_v<T>,
        "Default metadata factories require default-constructible types");
    static_assert(!std::is_abstract_v<T>,
        "Default metadata factories cannot construct abstract types");
    Base::Result<Base::Ref<T>> created = Base::MakeRef<T>();
    if (!created) return created.GetStatus();
    return Base::Ref<Base::Object>(
        std::move(created).Value());
}

template<auto Member>
struct MemberPointerTraits;

template<class Owner, class Field, Field Owner::*Member>
struct MemberPointerTraits<Member> {
    using OwnerType = Owner;
    using FieldType = Field;
};

template<class Owner, class Field, Field Owner::*Member>
Base::Result<Value> GetField(
    const void* object,
    Registry& runtime,
    void*) noexcept {
    if (object == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field target is null");
    }
    return ValueCodec<Field>::Encode(
        runtime,
        static_cast<const Owner*>(object)->*Member);
}

template<class Owner, class Field, Field Owner::*Member>
void SetField(
    void* object,
    const Value& value,
    Registry& runtime,
    void*) noexcept {
    if (object == nullptr) {
        return;
    }
    Base::Result<Field> decoded =
        ValueCodec<Field>::Decode(runtime, value);
    if (!decoded) return;
    static_cast<Owner*>(object)->*Member =
        std::move(decoded).Value();
    return;
}

template<class T>
MetadataAuthoringSession CreateDescriptionSession(
    Registration& context,
    TypeFlags flags) noexcept;

template<class T>
MetadataAuthoringSession CreateNamedDescriptionSession(
    Registration& context,
    Base::StringView metadataNamespace,
    Base::StringView metadataName,
    TypeFlags flags) noexcept;

class AERO_GUI_API MetadataAuthoringSession {
private:
    template<class>
    friend class ::Aero::Meta::TypeBuilder;
    template<class T>
    friend MetadataAuthoringSession CreateDescriptionSession(
        Registration& context,
        TypeFlags flags) noexcept;
    template<class T>
    friend MetadataAuthoringSession CreateNamedDescriptionSession(
        Registration& context,
        Base::StringView metadataNamespace,
        Base::StringView metadataName,
        TypeFlags flags) noexcept;

    MetadataAuthoringSession(
        Registration& context,
        const TypeRegistration& registration,
        TypeId expectedType) noexcept;

    MetadataAuthoringSession(
        const MetadataAuthoringSession&) = delete;
    MetadataAuthoringSession& operator=(
        const MetadataAuthoringSession&) = delete;
    MetadataAuthoringSession(
        MetadataAuthoringSession&&) noexcept = default;
    MetadataAuthoringSession& operator=(
        MetadataAuthoringSession&&) noexcept = default;

    MetadataAuthoringSession& Implements(
        TypeId interfaceType,
        InterfaceCastThunk cast = nullptr) noexcept;
    MetadataAuthoringSession& Factory(
        ObjectFactory factory) noexcept;
    MetadataAuthoringSession& PropertyChangeNotifications(
        PropertyChangeSubscribeCallback subscribe,
        PropertyChangeUnsubscribeCallback unsubscribe,
        void* callbackContext) noexcept;
    MetadataAuthoringSession& CollectionChangeNotifications(
        CollectionChangeSubscribeCallback subscribe,
        CollectionChangeUnsubscribeCallback unsubscribe,
        void* callbackContext) noexcept;
    MetadataAuthoringSession& DependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView name,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        DependencyPropertyFlags propertyFlags,
        ValidateValueCallback validate,
        CoerceValueCallback coerce,
        PropertyChangedCallback changed,
        UpdateSourceTrigger updateSourceTrigger) noexcept;
    MetadataAuthoringSession& Override(
        DependencyPropertyHandle property,
        TypeId ownerType,
        PropertyMetadata metadata) noexcept;
    MetadataAuthoringSession& AddOwner(
        DependencyPropertyHandle property,
        TypeId ownerType,
        PropertyMetadata metadata,
        DependencyPropertyFlags flags = DependencyPropertyFlags::None) noexcept;
    MetadataAuthoringSession& RoutedEvent(
        RoutedEventHandle declaredHandle,
        Base::StringView name,
        TypeId eventArgsType,
        RoutingStrategy strategy) noexcept;
    MetadataAuthoringSession& Content(
        Base::StringView name,
        TypeId valueType,
        ContentKind kind,
        ContentWriteCallback write,
        ContentClearCallback clear,
        ContentFlags contentFlags,
        void* contentContext) noexcept;
    MetadataAuthoringSession& Collection(
        Base::StringView name,
        TypeId valueType,
        ContentWriteCallback write,
        ContentClearCallback clear,
        PropertyFlags propertyFlags,
        ContentFlags contentFlags,
        void* contentContext) noexcept;
    MetadataAuthoringSession& Property(
        const PropertyRegistration& registration) noexcept;
    MetadataAuthoringSession& Field(
        const FieldRegistration& registration) noexcept;
    MetadataAuthoringSession& Method(
        const MethodRegistration& registration) noexcept;
    MetadataAuthoringSession& EventHandler(
        Base::StringView name,
        EventHandlerThunk thunk) noexcept;
    MetadataAuthoringSession& EnumValueRaw(
        Base::StringView name,
        std::uint64_t rawValue) noexcept;
    MetadataAuthoringSession& Content(
        MemberId member) noexcept;
    MetadataAuthoringSession& ContentAccessor(
        MemberId member,
        ContentKind kind,
        ContentWriteCallback write,
        ContentClearCallback clear,
        ContentFlags contentFlags,
        void* contentContext) noexcept;
    MetadataAuthoringSession& ValueSemantics(
        const ValueTypeRegistration& registration) noexcept;
    MetadataAuthoringSession& TextConverter(
        TextValueConverterCallback converter) noexcept;

    MetadataAuthoringSession& Fail(
        Base::Status status) noexcept;
    bool Ok() const noexcept { return status_.IsOk(); }
    Base::Result<void> Finish() const noexcept;

    template<class TValue>
    Base::Result<Value> Encode(
        const TValue& value) noexcept {
        RegistrationValues values =
            context_->Values();
        return ValueCodec<TValue>::Encode(
            values, value);
    }

    template<class TContext>
    Base::Result<std::decay_t<TContext>*>
    OwnBehaviorContext(TContext&& value) noexcept {
        using Stored = std::decay_t<TContext>;
        Stored temporary(std::forward<TContext>(value));
        Base::Result<void*> stored =
            OwnBehaviorContextRaw(
                sizeof(Stored),
                alignof(Stored),
                &temporary,
                [](void* destination, void* source) noexcept {
                    new (destination) Stored(std::move(
                        *static_cast<Stored*>(source)));
                },
                [](void* storedValue) noexcept {
                    static_cast<Stored*>(storedValue)->~Stored();
                });
        if (!stored) return stored.GetStatus();
        return static_cast<Stored*>(stored.Value());
    }

    void ReleaseBehaviorContext(void* value) noexcept;

private:
    Base::Result<void*> OwnBehaviorContextRaw(
        std::size_t size,
        std::size_t alignment,
        void* source,
        void (*construct)(void*, void*) noexcept,
        void (*destroyValue)(void*) noexcept) noexcept;
    void Record(Base::Result<void> result) noexcept;

    template<class TValue>
    void Record(Base::Result<TValue>& result) noexcept {
        if (status_.IsOk() && !result) {
            status_ = result.GetStatus();
        }
    }

    Registration* context_ = nullptr;
    TypeId type_ = InvalidTypeId;
    Base::Status status_;
};

template<class T, class = void>
struct HasRuntimeTypeToken : std::false_type {};

template<class T>
struct HasRuntimeTypeToken<T, std::void_t<decltype(
    TypeTraits<T>::Token())>> : std::true_type {};

template<class T>
MetadataAuthoringSession CreateDescriptionSession(
    Registration& context,
    TypeFlags flags) noexcept {
    const TypeId typeId = TypeTraits<T>::Id();
    const Base::StringView metadataNamespace = TypeTraits<T>::Namespace();
    const Base::StringView metadataName = TypeTraits<T>::Name();
    TypeId baseType = InvalidTypeId;
    MetadataTypeKind kind = MetadataTypeKind::Struct;
    TypeRegistration registration = TypeRegistration::Struct(
        metadataNamespace, metadataName, baseType, flags);

    if constexpr (std::is_enum_v<T>) {
        if constexpr (
            std::is_signed_v<std::underlying_type_t<T>>) {
            flags = flags | TypeFlags::SignedEnum;
        }
        kind = MetadataTypeKind::Enum;
        registration = TypeRegistration::Enum(
            metadataNamespace, metadataName,
            TypeOf<std::uint32_t>(), flags);
    } else if constexpr (
        std::is_arithmetic_v<T> ||
        std::is_same_v<T, Base::String>) {
        kind = MetadataTypeKind::Primitive;
        registration = TypeRegistration::Primitive(
            metadataNamespace, metadataName, flags);
    } else if constexpr (
        std::is_base_of_v<Base::Object, T>) {
        kind = MetadataTypeKind::Object;
        baseType = TypeTraits<T>::BaseType();
        registration = TypeRegistration::Object(
            metadataNamespace, metadataName, baseType, flags);
    } else if constexpr (std::is_abstract_v<T>) {
        kind = MetadataTypeKind::Interface;
        registration = TypeRegistration::Interface(
            metadataNamespace, metadataName, flags);
    } else {
        if constexpr (std::is_trivially_copyable_v<T>) {
            flags = flags | TypeFlags::TriviallyCopyable;
        }
        kind = MetadataTypeKind::Struct;
        baseType = TypeTraits<T>::BaseType();
        registration = TypeRegistration::Struct(
            metadataNamespace, metadataName, baseType, flags);
    }

    Base::Status bindingStatus = BindRuntimeTypeInfo(
        typeId,
        RuntimeTypeInfo{
            typeId,
            metadataNamespace,
            metadataName,
            baseType,
            kind});
    if constexpr (HasRuntimeTypeToken<T>::value) {
        if (bindingStatus.IsOk()) {
            bindingStatus = BindRuntimeTypeInfo(
                TypeTraits<T>::Token(),
                RuntimeTypeInfo{
                    typeId,
                    metadataNamespace,
                    metadataName,
                    baseType,
                    kind});
        }
    }

    MetadataAuthoringSession session(
        context, registration, typeId);
    if (!bindingStatus.IsOk()) {
        session.Fail(bindingStatus);
    }
    return session;
}

template<class T>
MetadataAuthoringSession CreateNamedDescriptionSession(
    Registration& context,
    Base::StringView metadataNamespace,
    Base::StringView metadataName,
    TypeFlags flags) noexcept {
    const TypeId typeId = MakeTypeId(metadataNamespace, metadataName);
    TypeId baseType = InvalidTypeId;
    MetadataTypeKind kind = MetadataTypeKind::Struct;
    TypeRegistration registration = TypeRegistration::Struct(
        metadataNamespace, metadataName, baseType, flags);

    if constexpr (std::is_enum_v<T>) {
        if constexpr (
            std::is_signed_v<std::underlying_type_t<T>>) {
            flags = flags | TypeFlags::SignedEnum;
        }
        kind = MetadataTypeKind::Enum;
        registration = TypeRegistration::Enum(
            metadataNamespace, metadataName,
            TypeOf<std::uint32_t>(), flags);
    } else if constexpr (
        std::is_arithmetic_v<T> ||
        std::is_same_v<T, Base::String>) {
        kind = MetadataTypeKind::Primitive;
        registration = TypeRegistration::Primitive(
            metadataNamespace, metadataName, flags);
    } else if constexpr (
        std::is_base_of_v<Base::Object, T>) {
        kind = MetadataTypeKind::Object;
        baseType = TypeTraits<T>::BaseType();
        registration = TypeRegistration::Object(
            metadataNamespace, metadataName, baseType, flags);
    } else if constexpr (std::is_abstract_v<T>) {
        kind = MetadataTypeKind::Interface;
        registration = TypeRegistration::Interface(
            metadataNamespace, metadataName, flags);
    } else {
        if constexpr (std::is_trivially_copyable_v<T>) {
            flags = flags | TypeFlags::TriviallyCopyable;
        }
        kind = MetadataTypeKind::Struct;
        baseType = TypeTraits<T>::BaseType();
        registration = TypeRegistration::Struct(
            metadataNamespace, metadataName, baseType, flags);
    }

    Base::Status bindingStatus = BindRuntimeTypeInfo(
        typeId,
        RuntimeTypeInfo{
            typeId,
            metadataNamespace,
            metadataName,
            baseType,
            kind});
    if constexpr (HasRuntimeTypeToken<T>::value) {
        if (bindingStatus.IsOk()) {
            bindingStatus = BindRuntimeTypeInfo(
                TypeTraits<T>::Token(),
                RuntimeTypeInfo{
                    typeId,
                    metadataNamespace,
                    metadataName,
                    baseType,
                    kind});
        }
    }

    MetadataAuthoringSession session(
        context, registration, typeId);
    if (!bindingStatus.IsOk()) {
        session.Fail(bindingStatus);
    }
    return session;
}

} // namespace Aero::Meta




namespace Aero::Meta {


template<class T>
struct IsResultVoid : std::false_type {};

template<>
struct IsResultVoid<Base::Result<void>>
    : std::true_type {};

template<class T, auto Converter>
Base::Result<Value> ConvertTypedText(
    TypeId targetType,
    Base::StringView text,
    void* context) noexcept {
    using ConverterResult = std::invoke_result_t<
        decltype(Converter), Base::StringView>;
    static_assert(
        std::is_same_v<ConverterResult, Base::Result<T>>,
        "Typed metadata text converters must return Base::Result<T>");
    if (targetType != ValueCodec<T>::Type()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Typed metadata text converter received a mismatched type");
    }
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Typed metadata text converter has no value registry");
    }
    Base::Result<T> converted =
        std::invoke(Converter, text);
    if (!converted) return converted.GetStatus();
    RegistrationValues registrations =
        MakeRegistrationValues(context);
    return ValueCodec<T>::Encode(
        registrations, converted.Value());
}

template<class TOwner, class TValue, auto Getter>
Base::Result<Value> GetOrdinaryProperty(
    const Base::Object& object,
    void*) noexcept {
    static_assert(
        std::is_base_of_v<Base::Object, TOwner>);
    const auto& owner =
        static_cast<const TOwner&>(object);
    if constexpr (
        std::is_same_v<TValue, Base::String> &&
        std::is_same_v<
            std::remove_cv_t<std::remove_reference_t<
                std::invoke_result_t<
                    decltype(Getter),
                    const TOwner&>>>,
            Base::StringView>) {
        Base::String copied;
        Base::Result<void> assigned = copied.Assign(
            std::invoke(Getter, owner));
        if (!assigned) return assigned.GetStatus();
        return ValueCodec<TValue>::Encode(copied);
    } else {
        return ValueCodec<TValue>::Encode(
            std::invoke(Getter, owner));
    }
}

template<class TOwner, class TValue, auto Setter>
void SetOrdinaryProperty(
    Base::Object& object,
    const Value& stored,
    void*) noexcept {
    static_assert(
        std::is_base_of_v<Base::Object, TOwner>);
    Base::Result<TValue> decoded =
        ValueCodec<TValue>::Decode(stored);
    if (!decoded) return;
    auto& owner = static_cast<TOwner&>(object);
    if constexpr (
        std::is_same_v<TValue, Base::String> &&
        std::is_invocable_v<
            decltype(Setter),
            TOwner&,
            Base::StringView>) {
        using SetterResult = std::invoke_result_t<
            decltype(Setter), TOwner&, Base::StringView>;
        if constexpr (IsResultVoid<SetterResult>::value) {
            (void)std::invoke(Setter, owner, decoded.Value().View());
        } else {
            std::invoke(Setter, owner, decoded.Value().View());
        }
    } else {
        using SetterResult = std::invoke_result_t<
            decltype(Setter), TOwner&, TValue>;
        if constexpr (IsResultVoid<SetterResult>::value) {
            (void)std::invoke(
                Setter, owner, std::move(decoded).Value());
        } else {
            std::invoke(
                Setter, owner, std::move(decoded).Value());
        }
    }
}

template<class TOwner, class TValue, class TGetter, class TSetter>
struct OrdinaryPropertyAdapter {
    TGetter getter;
    TSetter setter;

    static Base::Result<Value> Get(
        const Base::Object& object,
        void* context) noexcept {
        const auto* adapter =
            static_cast<const OrdinaryPropertyAdapter*>(context);
        if (adapter == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Ordinary metadata property adapter is unavailable");
        }
        const auto& owner = static_cast<const TOwner&>(object);
        if constexpr (
            std::is_same_v<TValue, Base::String> &&
            std::is_same_v<
                std::remove_cv_t<std::remove_reference_t<
                    std::invoke_result_t<
                        TGetter, const TOwner&>>>,
                Base::StringView>) {
            Base::String copied;
            Base::Result<void> assigned = copied.Assign(
                std::invoke(adapter->getter, owner));
            if (!assigned) return assigned.GetStatus();
            return ValueCodec<TValue>::Encode(copied);
        } else {
            return ValueCodec<TValue>::Encode(
                std::invoke(adapter->getter, owner));
        }
    }

    static void Set(
        Base::Object& object,
        const Value& stored,
        void* context) noexcept {
        const auto* adapter =
            static_cast<const OrdinaryPropertyAdapter*>(context);
        if (adapter == nullptr) return;
        Base::Result<TValue> decoded =
            ValueCodec<TValue>::Decode(stored);
        if (!decoded) return;
        auto& owner = static_cast<TOwner&>(object);
        if constexpr (
            std::is_same_v<TValue, Base::String> &&
            std::is_invocable_v<
                TSetter, TOwner&, Base::StringView>) {
            std::invoke(
                adapter->setter,
                owner,
                decoded.Value().View());
        } else {
            std::invoke(
                adapter->setter,
                owner,
                std::move(decoded).Value());
        }
    }
};

template<class TOwner, class TArgs, auto Handler>
Base::Result<Value> InvokeEventHandler(
    Base::Object& object,
    Base::Span<const Value> arguments,
    void*) noexcept {
    static_assert(std::is_base_of_v<Base::Object, TOwner>,
        "XAML event handler owner must derive from Object");
    static_assert(std::is_invocable_v<
        decltype(Handler), TOwner&, Base::Object*, TArgs&>,
        "XAML event handler must accept (Object*, EventArgs& or const EventArgs&)");
    if (arguments.Size() != 2U ||
        arguments[0].Kind() != ValueKind::Object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML event handler received an incompatible argument list");
    }
    Base::Result<TArgs> eventArgs =
        ValueCodec<TArgs>::Decode(arguments[1]);
    if (!eventArgs) return eventArgs.GetStatus();
    std::invoke(
        Handler,
        static_cast<TOwner&>(object),
        arguments[0].IsNullObject()
            ? nullptr
            : arguments[0].AsObject().Get(),
        eventArgs.Value());
    return Value{};
}



} // namespace Aero::Meta
