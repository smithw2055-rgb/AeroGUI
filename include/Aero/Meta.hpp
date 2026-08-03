#pragma once

#include <Aero/Value.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events/RoutedEvent.hpp>
#include <Aero/Module.hpp>

#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Meta {
using Base::ValueCopyCallback;
using Base::ValueDestroyCallback;
using Base::ValueEqualsCallback;
using ObjectFactory = Base::Result<Base::Ref<Base::Object>> (*)() noexcept;
enum class ContentKind : std::uint8_t {
    Single = 0U,
    Collection
};

enum class ContentFlags : std::uint8_t {
    None = 0U,
    Visual = 1U << 0U
};

constexpr ContentFlags operator|(
    ContentFlags left,
    ContentFlags right) noexcept {
    return static_cast<ContentFlags>(
        static_cast<std::uint8_t>(left) |
        static_cast<std::uint8_t>(right));
}

constexpr bool HasContentFlag(
    ContentFlags value,
    ContentFlags flag) noexcept {
    return (static_cast<std::uint8_t>(value) &
        static_cast<std::uint8_t>(flag)) != 0U;
}

using ContentWriteCallback = void (*)(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void* context) noexcept;
using ContentClearCallback = void (*)(
    Base::Object& owner,
    void* context) noexcept;
using PropertyGetCallback = Base::Result<Value> (*)(
    const Base::Object& object,
    void* context) noexcept;
using PropertySetCallback = void (*)(
    Base::Object& object,
    const Value& value,
    void* context) noexcept;
using MethodInvokeCallback = Base::Result<Value> (*)(
    Base::Object& object,
    Base::Span<const Value> arguments,
    void* context) noexcept;
using ValueMemberGetCallback = Base::Result<Value> (*)(
    const void* object,
    Registry& runtime,
    void* context) noexcept;
using ValueMemberSetCallback = void (*)(
    void* object,
    const Value& value,
    Registry& runtime,
    void* context) noexcept;
using MetadataPropertyChangedCallback = void (*)(
    Base::Object& object,
    MemberId property,
    void* context) noexcept;
using PropertyChangeSubscribeCallback = Base::Result<std::uint64_t> (*)(
    Base::Object& object,
    MetadataPropertyChangedCallback callback,
    void* callbackContext,
    void* context) noexcept;
using PropertyChangeUnsubscribeCallback = Base::Result<bool> (*)(
    Base::Object& object,
    std::uint64_t subscription,
    void* context) noexcept;
enum class MetadataCollectionChangeAction : std::uint8_t {
    Add = 0U,
    Remove,
    Replace,
    Move,
    Reset
};
struct MetadataCollectionChangedEvent {
    MetadataCollectionChangeAction action =
        MetadataCollectionChangeAction::Reset;
    std::uint32_t oldIndex = UINT32_MAX;
    std::uint32_t newIndex = UINT32_MAX;
    std::uint32_t oldCount = 0U;
    std::uint32_t newCount = 0U;
};
using MetadataCollectionChangedCallback = void (*)(
    Base::Object& collection,
    const MetadataCollectionChangedEvent& event,
    void* context) noexcept;
using CollectionChangeSubscribeCallback = Base::Result<std::uint64_t> (*)(
    Base::Object& collection,
    MetadataCollectionChangedCallback callback,
    void* callbackContext,
    void* context) noexcept;
using CollectionChangeUnsubscribeCallback = Base::Result<bool> (*)(
    Base::Object& collection,
    std::uint64_t subscription,
    void* context) noexcept;

struct TypeRegistration {
    constexpr TypeRegistration(
        Base::StringView registeredNamespace,
        Base::StringView registeredName,
        TypeId registeredBase,
        TypeFlags registeredFlags,
        ObjectFactory registeredFactory,
        MetadataTypeKind registeredKind,
        TypeId registeredUnderlying,
        Base::Span<const TypeId> registeredInterfaces) noexcept
        : xamlNamespace(registeredNamespace),
          name(registeredName),
          baseType(registeredBase),
          flags(registeredFlags),
          factory(registeredFactory),
          kind(registeredKind),
          underlyingType(registeredUnderlying),
          interfaces(registeredInterfaces) {}

    static constexpr TypeRegistration Object(
        Base::StringView metadataNamespace,
        Base::StringView name,
        TypeId baseType = InvalidTypeId,
        TypeFlags flags = TypeFlags::None,
        ObjectFactory factory = nullptr,
        Base::Span<const TypeId> interfaces = {}) noexcept {
        return {metadataNamespace, name, baseType, flags, factory,
            MetadataTypeKind::Object, InvalidTypeId, interfaces};
    }

    static constexpr TypeRegistration Interface(
        Base::StringView metadataNamespace,
        Base::StringView name,
        TypeFlags flags = TypeFlags::None,
        Base::Span<const TypeId> interfaces = {}) noexcept {
        return {metadataNamespace, name, InvalidTypeId,
            flags | TypeFlags::Abstract, nullptr,
            MetadataTypeKind::Interface, InvalidTypeId, interfaces};
    }

    static constexpr TypeRegistration Struct(
        Base::StringView metadataNamespace,
        Base::StringView name,
        TypeId baseType = InvalidTypeId,
        TypeFlags flags = TypeFlags::None) noexcept {
        return {metadataNamespace, name, baseType,
            flags | TypeFlags::ValueType | TypeFlags::Sealed, nullptr,
            MetadataTypeKind::Struct, InvalidTypeId, {}};
    }

    static constexpr TypeRegistration Enum(
        Base::StringView metadataNamespace,
        Base::StringView name,
        TypeId underlyingType,
        TypeFlags flags = TypeFlags::None) noexcept {
        return {metadataNamespace, name, InvalidTypeId,
            flags | TypeFlags::ValueType | TypeFlags::Sealed, nullptr,
            MetadataTypeKind::Enum, underlyingType, {}};
    }

    static constexpr TypeRegistration Primitive(
        Base::StringView metadataNamespace,
        Base::StringView name,
        TypeFlags flags = TypeFlags::None) noexcept {
        return {metadataNamespace, name, InvalidTypeId,
            flags | TypeFlags::ValueType | TypeFlags::Sealed, nullptr,
            MetadataTypeKind::Primitive, InvalidTypeId, {}};
    }

    Base::StringView xamlNamespace;
    Base::StringView name;
    TypeId baseType = InvalidTypeId;
    TypeFlags flags = TypeFlags::None;
    ObjectFactory factory = nullptr;
    MetadataTypeKind kind = MetadataTypeKind::Object;
    TypeId underlyingType = InvalidTypeId;
    Base::Span<const TypeId> interfaces;
};

struct PropertyRegistration {
    Base::StringView name;
    TypeId valueType = InvalidTypeId;
    PropertyFlags flags = PropertyFlags::None;
    PropertyAccessKind access = PropertyAccessKind::External;
    PropertyGetCallback get = nullptr;
    PropertySetCallback set = nullptr;
    PropertyProviderId provider = InvalidPropertyProviderId;
    void* context = nullptr;
};

struct FieldRegistration {
    Base::StringView name;
    TypeId valueType = InvalidTypeId;
    FieldFlags flags = FieldFlags::None;
    ValueMemberGetCallback get = nullptr;
    ValueMemberSetCallback set = nullptr;
    void* context = nullptr;
};

struct EnumValueRegistration {
    Base::StringView name;
    std::uint64_t rawValue = 0U;
};

struct EventRegistration {
    Base::StringView name;
    TypeId eventArgsType = InvalidTypeId;
    EventFlags flags = EventFlags::None;
};

struct MethodParameterRegistration {
    Base::StringView name;
    TypeId type = InvalidTypeId;
};

struct MethodRegistration {
    Base::StringView name;
    TypeId returnType = InvalidTypeId;
    Base::Span<const MethodParameterRegistration> parameters;
    MethodFlags flags = MethodFlags::None;
    MethodInvokeCallback invoke = nullptr;
    void* context = nullptr;
};

struct TypeFactoryRegistration {
    TypeId type = InvalidTypeId;
    ObjectFactory factory = nullptr;
};

struct ContentAccessorRegistration {
    TypeId type = InvalidTypeId;
    MemberId member = InvalidMemberId;
    ContentKind kind = ContentKind::Single;
    ContentFlags flags = ContentFlags::None;
    ContentWriteCallback write = nullptr;
    ContentClearCallback clear = nullptr;
    void* context = nullptr;
};

struct PropertyAccessorRegistration {
    MemberId member = InvalidMemberId;
    PropertyAccessKind access = PropertyAccessKind::External;
    PropertyGetCallback get = nullptr;
    PropertySetCallback set = nullptr;
    PropertyProviderId provider = InvalidPropertyProviderId;
    void* context = nullptr;
};

struct ValueMemberAccessorRegistration {
    MemberId member = InvalidMemberId;
    ValueMemberGetCallback get = nullptr;
    ValueMemberSetCallback set = nullptr;
    void* context = nullptr;
};

struct MethodInvokerRegistration {
    MemberId member = InvalidMemberId;
    MethodInvokeCallback invoke = nullptr;
    void* context = nullptr;
};

struct PropertyChangeNotificationRegistration {
    TypeId type = InvalidTypeId;
    PropertyChangeSubscribeCallback subscribe = nullptr;
    PropertyChangeUnsubscribeCallback unsubscribe = nullptr;
    void* context = nullptr;
};

struct CollectionChangeNotificationRegistration {
    TypeId type = InvalidTypeId;
    CollectionChangeSubscribeCallback subscribe = nullptr;
    CollectionChangeUnsubscribeCallback unsubscribe = nullptr;
    void* context = nullptr;
};


} // namespace Aero::Meta

namespace Aero::Meta::Detail {
class MetadataAuthoringSession;
}

namespace Aero::Meta {

class DependencyPropertyRegistry;
class RegistrationTypes;
class RegistrationValues;
class ValueTable;
template<class T>
class TypeDescription;

} // namespace Aero::Meta

namespace Aero::Meta {

class Registry;

// Callback-scoped metadata authoring session. Module authors use Register<T>
// against this object; mutable tables and registration storage stay private to
// Registry.
class AERO_API Registration {
private:
    friend class Registry;
    template<class T>
    friend class TypeDescription;
    friend class Detail::MetadataAuthoringSession;

    explicit Registration(void* state) noexcept
        : state_(state) {}

    RegistrationValues Values() noexcept;
    RegistrationValues Values() const noexcept;
    RegistrationTypes Types() noexcept;
    ValueTable& ValueRegistrations() noexcept;
    DependencyPropertyRegistry& DependencyProperties() noexcept;

    void* state_ = nullptr;
};

} // namespace Aero::Meta

namespace Aero::Meta { class Registry; class Registration; }

namespace Aero::Meta {

class RegistrationValues;
class TypeRegistry;
class ValueTypeSemantics;
struct TextValueConverterRegistration;
struct ValueTypeRegistration;

namespace Detail {
AERO_API Base::Result<Value> CreateRegistrationValue(
    void* registrationState,
    TypeId type,
    const void* source) noexcept;
AERO_API RegistrationValues MakeRegistrationValues(
    void* registrationState) noexcept;
}

// Opaque callback-scoped value registration view used by ValueCodec. The
// backing registration store remains a Core implementation detail.
class AERO_API RegistrationValues {
public:
    Base::Result<void> RegisterValueSemantics(
        TypeId type,
        const ValueTypeRegistration& registration) const noexcept;
    Base::Result<void> RegisterTextConverter(
        const TextValueConverterRegistration& registration) const noexcept;
    Base::Result<Value> TryCreateValue(
        TypeId type,
        const void* source) const noexcept;
    Base::Result<Value> TryConvertText(
        TypeId type,
        Base::StringView text) const noexcept;

    const Base::Ref<ValueTypeSemantics>* FindValueSemantics(
        TypeId type) const noexcept;
    const TextValueConverterRegistration* FindTextConverter(
        TypeId type) const noexcept;
    bool IsFrozen() const noexcept;
    const TypeRegistry& Types() const noexcept;

private:
    friend class ::Aero::Meta::Registration;
    friend Base::Result<Value> Detail::CreateRegistrationValue(
        void* registrationState,
        TypeId type,
        const void* source) noexcept;
    friend RegistrationValues
    Detail::MakeRegistrationValues(
        void* registrationState) noexcept;

    RegistrationValues(
        const void* registrations,
        void* mutableRegistrations) noexcept
        : registrations_(registrations),
          mutableRegistrations_(mutableRegistrations) {}

    const void* registrations_ = nullptr;
    void* mutableRegistrations_ = nullptr;
};

} // namespace Aero::Meta

namespace Aero::Meta::Detail {

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

class AERO_API MetadataAuthoringSession {
public:
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
        TypeId interfaceType) noexcept;
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
        PropertyMetadata metadata) noexcept;
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

template<class T>
MetadataAuthoringSession CreateDescriptionSession(
    Registration& context,
    TypeFlags flags) noexcept {
    if constexpr (std::is_enum_v<T>) {
        if constexpr (
            std::is_signed_v<std::underlying_type_t<T>>) {
            flags = flags | TypeFlags::SignedEnum;
        }
        return MetadataAuthoringSession(
            context,
            TypeRegistration::Enum(
                TypeTraits<T>::Namespace(),
                TypeTraits<T>::Name(),
                TypeOf<std::uint32_t>(),
                flags),
            TypeTraits<T>::Id());
    } else if constexpr (
        std::is_arithmetic_v<T> ||
        std::is_same_v<T, Base::String>) {
        return MetadataAuthoringSession(
            context,
            TypeRegistration::Primitive(
                TypeTraits<T>::Namespace(),
                TypeTraits<T>::Name(),
                flags),
            TypeTraits<T>::Id());
    } else if constexpr (
        std::is_base_of_v<Base::Object, T>) {
        return MetadataAuthoringSession(
            context,
            TypeRegistration::Object(
                TypeTraits<T>::Namespace(),
                TypeTraits<T>::Name(),
                TypeTraits<T>::BaseType(),
                flags),
            TypeTraits<T>::Id());
    } else {
        if constexpr (std::is_trivially_copyable_v<T>) {
            flags = flags | TypeFlags::TriviallyCopyable;
        }
        return MetadataAuthoringSession(
            context,
            TypeRegistration::Struct(
                TypeTraits<T>::Namespace(),
                TypeTraits<T>::Name(),
                TypeTraits<T>::BaseType(),
                flags),
            TypeTraits<T>::Id());
    }
}

template<class T, class = void>
struct HasRuntimeTypeToken : std::false_type {};

template<class T>
struct HasRuntimeTypeToken<T, std::void_t<decltype(
    TypeTraits<T>::Token())>> : std::true_type {};

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
    } else {
        if constexpr (std::is_trivially_copyable_v<T>) {
            flags = flags | TypeFlags::TriviallyCopyable;
        }
        kind = MetadataTypeKind::Struct;
        baseType = TypeTraits<T>::BaseType();
        registration = TypeRegistration::Struct(
            metadataNamespace, metadataName, baseType, flags);
    }

    Base::Status bindingStatus = Base::Status::Ok();
    if constexpr (HasRuntimeTypeToken<T>::value) {
        bindingStatus = BindRuntimeTypeInfo(
            TypeTraits<T>::Token(),
            RuntimeTypeInfo{
                typeId,
                metadataNamespace,
                metadataName,
                baseType,
                kind});
    }

    MetadataAuthoringSession session(
        context, registration, typeId);
    if (!bindingStatus.IsOk()) {
        session.Fail(bindingStatus);
    }
    return session;
}

} // namespace Aero::Meta::Detail




namespace Aero::Meta {

namespace Detail {

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

} // namespace Detail

template<class TValue>
class PropertyOptions {
public:
    explicit PropertyOptions(TValue defaultValue) noexcept
        : defaultValue_(std::move(defaultValue)) {}

    PropertyOptions& Inherits() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::Inherits;
        return *this;
    }
    PropertyOptions& AffectsMeasure() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsMeasure;
        return *this;
    }
    PropertyOptions& AffectsArrange() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsArrange;
        return *this;
    }
    PropertyOptions& AffectsRender() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsRender;
        return *this;
    }
    PropertyOptions& AffectsParentMeasure() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsParentMeasure;
        return *this;
    }
    PropertyOptions& AffectsParentArrange() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsParentArrange;
        return *this;
    }
    PropertyOptions& Structural() noexcept {
        structural_ = true;
        return *this;
    }
    PropertyOptions& BindsTwoWayByDefault() noexcept {
        flags_ = flags_ |
            PropertyMetadataFlags::BindsTwoWayByDefault;
        return *this;
    }
    PropertyOptions& Apply(
        FrameworkPropertyMetadataOptions options) noexcept {
        flags_ = flags_ | ToPropertyMetadataFlags(options);
        return *this;
    }
    PropertyOptions& UpdateSource(
        UpdateSourceTrigger trigger) noexcept {
        updateSourceTrigger_ = trigger;
        return *this;
    }
    PropertyOptions& Validate(
        ValidateValueCallback validate) noexcept {
        validate_ = validate;
        return *this;
    }
    PropertyOptions& Validate(
        bool (*validate)(const TValue&) noexcept) noexcept {
        validate_ = [validate](
            const Value& stored) noexcept {
            Base::Result<TValue> decoded =
                ValueCodec<TValue>::Decode(stored);
            return decoded &&
                validate(decoded.Value());
        };
        return *this;
    }
    PropertyOptions& Coerce(
        CoerceValueCallback coerce) noexcept {
        coerce_ = coerce;
        return *this;
    }
    PropertyOptions& Coerce(
        Base::Result<TValue> (*coerce)(
            DependencyObject&,
            const DependencyProperty&,
            const TValue&) noexcept) noexcept {
        coerce_ = [coerce](
            DependencyObject& object,
            const DependencyProperty& property,
            const Value& stored) noexcept
            -> Base::Result<Value> {
            Base::Result<TValue> decoded =
                ValueCodec<TValue>::Decode(stored);
            if (!decoded) return decoded.GetStatus();
            Base::Result<TValue> result = coerce(
                object, property, decoded.Value());
            if (!result) return result.GetStatus();
            return ValueCodec<TValue>::Encode(
                result.Value());
        };
        return *this;
    }
    PropertyOptions& Changed(
        PropertyChangedCallback changed) noexcept {
        changed_ = changed;
        return *this;
    }
    PropertyOptions& Changed(
        void (*changed)(
            DependencyObject&,
            const TValue&,
            const TValue&) noexcept) noexcept {
        changed_ = [changed](
            DependencyObject& object,
            const DependencyPropertyChangedEventArgs&
                args) noexcept {
            Base::Result<TValue> oldValue =
                ValueCodec<TValue>::Decode(args.GetOldValue());
            Base::Result<TValue> newValue =
                ValueCodec<TValue>::Decode(args.GetNewValue());
            if (oldValue && newValue) {
                changed(
                    object,
                    oldValue.Value(),
                    newValue.Value());
            }
        };
        return *this;
    }

    const TValue& DefaultValue() const noexcept {
        return defaultValue_;
    }
    PropertyMetadataFlags Flags() const noexcept {
        return flags_;
    }
    UpdateSourceTrigger DefaultUpdateSourceTrigger() const noexcept {
        return updateSourceTrigger_;
    }
    ValidateValueCallback Validator() const noexcept {
        return validate_;
    }
    CoerceValueCallback Coercer() const noexcept {
        return coerce_;
    }
    PropertyChangedCallback ChangeCallback() const noexcept {
        return changed_;
    }
    bool IsStructural() const noexcept {
        return structural_;
    }

private:
    TValue defaultValue_;
    PropertyMetadataFlags flags_ = PropertyMetadataFlags::None;
    UpdateSourceTrigger updateSourceTrigger_ =
        UpdateSourceTrigger::Default;
    ValidateValueCallback validate_ = nullptr;
    CoerceValueCallback coerce_ = nullptr;
    PropertyChangedCallback changed_ = nullptr;
    bool structural_ = false;
};

template<class T>
class TypeDescription {
public:
    explicit TypeDescription(
        Registration& context,
        TypeFlags flags = TypeFlags::None) noexcept
        : builder_(Detail::CreateDescriptionSession<T>(
              context, flags)) {}

    TypeDescription(
        Registration& context,
        Base::StringView metadataNamespace,
        Base::StringView metadataName,
        TypeFlags flags = TypeFlags::None) noexcept
        : builder_(Detail::CreateNamedDescriptionSession<T>(
              context, metadataNamespace, metadataName, flags)) {}

    TypeDescription(const TypeDescription&) = delete;
    TypeDescription& operator=(const TypeDescription&) = delete;
    TypeDescription(TypeDescription&&) noexcept = default;
    TypeDescription& operator=(TypeDescription&&) noexcept = default;

    TypeDescription& Factory() noexcept {
        builder_.Factory(
            &Detail::CreateDefaultObject<T>);
        return *this;
    }
    TypeDescription& Factory(ObjectFactory factory) noexcept {
        builder_.Factory(factory);
        return *this;
    }
    template<class TInterface>
    TypeDescription& Implements() noexcept {
        builder_.Implements(TypeOf<TInterface>());
        return *this;
    }

    template<class TOwner, class TValue>
    TypeDescription& Property(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const PropertyOptions<TValue>& options) noexcept {
        return RegisterProperty(
            property.Handle(), property.Name(),
            DependencyPropertyFlags::None, options);
    }

    template<class TOwner, class TValue>
    TypeDescription& Property(
        const AttachedPropertyRef<TOwner, TValue>& property,
        const PropertyOptions<TValue>& options) noexcept {
        return RegisterProperty(
            property.Handle(), property.Name(),
            DependencyPropertyFlags::Attached, options);
    }

    template<class TOwner, class TValue>
    TypeDescription& Property(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const PropertyOptions<TValue>& options) noexcept {
        return RegisterProperty(
            property.Handle(), property.Name(),
            DependencyPropertyFlags::ReadOnly, options);
    }

    template<
        class TValue,
        auto Getter,
        auto Setter>
    TypeDescription& Property(
        Base::StringView name,
        PropertyFlags flags = PropertyFlags::None) noexcept {
        static_assert(
            std::is_invocable_v<
                decltype(Getter), const T&>,
            "Metadata property getter must be invocable on the described type");
        PropertyRegistration registration;
        registration.name = name;
        registration.valueType =
            ValueCodec<TValue>::Type();
        registration.flags = flags;
        registration.access =
            PropertyAccessKind::Ordinary;
        registration.get =
            &Detail::GetOrdinaryProperty<
                T, TValue, Getter>;
        registration.set =
            &Detail::SetOrdinaryProperty<
                T, TValue, Setter>;
        builder_.Property(registration);
        return *this;
    }

    template<class TValue, auto Setter>
    TypeDescription& Property(
        Base::StringView name,
        PropertyFlags flags = PropertyFlags::None) noexcept {
        static_assert(
            std::is_invocable_v<
                decltype(Setter), T&, TValue>,
            "Metadata property setter is incompatible with the described type");
        PropertyRegistration registration;
        registration.name = name;
        registration.valueType =
            ValueCodec<TValue>::Type();
        registration.flags = flags;
        registration.access =
            PropertyAccessKind::Ordinary;
        registration.set =
            &Detail::SetOrdinaryProperty<
                T, TValue, Setter>;
        builder_.Property(registration);
        return *this;
    }

    template<auto Getter, auto Setter>
    TypeDescription& Property(
        Base::StringView name,
        PropertyFlags flags = PropertyFlags::None) noexcept {
        using GetterResult = std::invoke_result_t<
            decltype(Getter), const T&>;
        using TValue = std::remove_cv_t<
            std::remove_reference_t<GetterResult>>;
        return Property<TValue, Getter, Setter>(
            name, flags);
    }

    template<class TGetter, class TSetter>
    TypeDescription& Property(
        Base::StringView name,
        TGetter getter,
        TSetter setter,
        PropertyFlags flags = PropertyFlags::None) noexcept {
        static_assert(
            std::is_member_function_pointer_v<TGetter> &&
            std::is_member_function_pointer_v<TSetter>,
            "Ordinary metadata property accessors must be member functions");
        static_assert(
            std::is_invocable_v<TGetter, const T&>,
            "Ordinary metadata property getter must be const-invocable");
        using GetterResult =
            std::invoke_result_t<TGetter, const T&>;
        using GetterValue = std::remove_cv_t<
            std::remove_reference_t<GetterResult>>;
        using TValue = std::conditional_t<
            std::is_same_v<GetterValue, Base::StringView>,
            Base::String,
            GetterValue>;
        static_assert(
            std::is_invocable_v<TSetter, T&, TValue> ||
            (std::is_same_v<TValue, Base::String> &&
             std::is_invocable_v<
                 TSetter, T&, Base::StringView>),
            "Ordinary metadata property setter is incompatible with getter");

        if (!builder_.Ok()) return *this;
        using Adapter = Detail::OrdinaryPropertyAdapter<
            T, TValue, TGetter, TSetter>;
        Base::Result<Adapter*> adapter =
            builder_.OwnBehaviorContext(
                Adapter{getter, setter});
        if (!adapter) {
            builder_.Fail(adapter.GetStatus());
            return *this;
        }

        PropertyRegistration registration;
        registration.name = name;
        registration.valueType = ValueCodec<TValue>::Type();
        registration.flags = flags;
        registration.access = PropertyAccessKind::Ordinary;
        registration.get = &Adapter::Get;
        registration.set = &Adapter::Set;
        registration.context = adapter.Value();
        builder_.Property(registration);
        if (!builder_.Ok()) {
            builder_.ReleaseBehaviorContext(adapter.Value());
        }
        return *this;
    }

    template<auto Member>
    TypeDescription& Field(
        Base::StringView name,
        FieldFlags flags = FieldFlags::None) noexcept {
        using Traits = Detail::MemberPointerTraits<Member>;
        using Owner = typename Traits::OwnerType;
        using FieldType = typename Traits::FieldType;
        static_assert(std::is_same_v<Owner, T>,
            "Metadata field member must belong to the described struct");
        builder_.Field({
            name,
            ValueCodec<FieldType>::Type(),
            flags,
            &Detail::GetField<Owner, FieldType, Member>,
            &Detail::SetField<Owner, FieldType, Member>,
            nullptr});
        return *this;
    }

    template<class TOwner, class TArgs>
    TypeDescription& Event(
        const ::Aero::RoutedEventRef<TOwner, TArgs>& event,
        RoutingStrategy strategy = RoutingStrategy::Bubble) noexcept {
        static_assert(std::is_same_v<TOwner, T>,
            "Routed event owner must match described type");
        builder_.RoutedEvent(
            event.Handle(), event.Name(),
            TypeOf<TArgs>(), strategy);
        return *this;
    }

    template<class TOwner, class TValue>
    TypeDescription& Override(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const PropertyOptions<TValue>& options) noexcept {
        if (!builder_.Ok()) return *this;
        Base::Result<::Aero::Meta::Value> encoded =
            builder_.Encode(options.DefaultValue());
        if (!encoded) {
            builder_.Fail(encoded.GetStatus());
            return *this;
        }
        PropertyMetadata metadata;
        metadata.defaultValue = std::move(encoded).Value();
        metadata.flags = options.Flags();
        metadata.defaultUpdateSourceTrigger =
            options.DefaultUpdateSourceTrigger();
        metadata.validate = options.Validator();
        metadata.coerce = options.Coercer();
        metadata.changed = options.ChangeCallback();
        builder_.Override(
            property.Handle(), TypeOf<T>(),
            std::move(metadata));
        return *this;
    }

    TypeDescription& Content(
        Base::StringView name,
        TypeId valueType,
        ContentKind kind,
        ContentWriteCallback write = nullptr,
        ContentClearCallback clear = nullptr,
        ContentFlags flags = ContentFlags::None,
        void* callbackContext = nullptr) noexcept {
        builder_.Content(
            name, valueType, kind, write, clear,
            flags, callbackContext);
        return *this;
    }

    template<class TValue>
    TypeDescription& Content(
        Base::StringView name,
        ContentKind kind,
        ContentWriteCallback write = nullptr,
        ContentClearCallback clear = nullptr,
        ContentFlags flags = ContentFlags::None,
        void* callbackContext = nullptr) noexcept {
        return Content(
            name, TypeOf<TValue>(), kind, write, clear,
            flags, callbackContext);
    }

    template<class TValue>
    TypeDescription& Collection(
        Base::StringView name,
        ContentWriteCallback write,
        ContentClearCallback clear,
        PropertyFlags propertyFlags =
            PropertyFlags::Structural,
        ContentFlags contentFlags = ContentFlags::None,
        void* callbackContext = nullptr) noexcept {
        builder_.Collection(
            name,
            TypeOf<TValue>(),
            write,
            clear,
            propertyFlags,
            contentFlags,
            callbackContext);
        return *this;
    }

    TypeDescription& Content(MemberId member) noexcept {
        builder_.Content(member);
        return *this;
    }

    template<class TOwner, class TValue>
    TypeDescription& AddOwner(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const PropertyOptions<TValue>& options) noexcept {
        if (!builder_.Ok()) return *this;
        Base::Result<::Aero::Meta::Value> encoded =
            builder_.Encode(options.DefaultValue());
        if (!encoded) {
            builder_.Fail(encoded.GetStatus());
            return *this;
        }
        PropertyMetadata metadata;
        metadata.defaultValue = std::move(encoded).Value();
        metadata.flags = options.Flags();
        metadata.defaultUpdateSourceTrigger =
            options.DefaultUpdateSourceTrigger();
        metadata.validate = options.Validator();
        metadata.coerce = options.Coercer();
        metadata.changed = options.ChangeCallback();
        builder_.AddOwner(
            property.Handle(), TypeOf<T>(),
            std::move(metadata));
        return *this;
    }

    TypeDescription& ContentAccessor(
        MemberId member,
        ContentKind kind,
        ContentWriteCallback write,
        ContentClearCallback clear,
        ContentFlags flags = ContentFlags::None,
        void* callbackContext = nullptr) noexcept {
        builder_.ContentAccessor(
            member, kind, write, clear,
            flags, callbackContext);
        return *this;
    }

    TypeDescription& ValueSemantics(
        const ValueTypeRegistration& registration) noexcept {
        builder_.ValueSemantics(registration);
        return *this;
    }

    TypeDescription& ValueSemantics() noexcept {
        builder_.ValueSemantics(
            Detail::MakeValueTypeRegistration<T>());
        return *this;
    }

    template<auto Converter>
    TypeDescription& TextConverter() noexcept {
        builder_.TextConverter(
            &Detail::ConvertTypedText<T, Converter>);
        return *this;
    }

    TypeDescription& TextConverter(
        TextValueConverterCallback converter) noexcept {
        builder_.TextConverter(converter);
        return *this;
    }

    TypeDescription& PropertyChangeNotifications(
        PropertyChangeSubscribeCallback subscribe,
        PropertyChangeUnsubscribeCallback unsubscribe,
        void* callbackContext = nullptr) noexcept {
        builder_.PropertyChangeNotifications(
            subscribe, unsubscribe, callbackContext);
        return *this;
    }

    TypeDescription& CollectionChangeNotifications(
        CollectionChangeSubscribeCallback subscribe,
        CollectionChangeUnsubscribeCallback unsubscribe,
        void* callbackContext = nullptr) noexcept {
        builder_.CollectionChangeNotifications(
            subscribe, unsubscribe, callbackContext);
        return *this;
    }

    TypeDescription& Value(
        Base::StringView name,
        T value) noexcept {
        static_assert(
            std::is_enum_v<T>,
            "Register<T>::Value requires an enum type");
        using Underlying = std::underlying_type_t<T>;
        using Unsigned = std::make_unsigned_t<Underlying>;
        builder_.EnumValueRaw(
            name,
            static_cast<std::uint64_t>(
                static_cast<Unsigned>(
                    static_cast<Underlying>(value))));
        return *this;
    }

    Base::Result<void> Result() const noexcept {
        return builder_.Finish();
    }
    bool Ok() const noexcept { return builder_.Ok(); }

private:
    template<class TValue>
    TypeDescription& RegisterProperty(
        DependencyPropertyHandle handle,
        Base::StringView name,
        DependencyPropertyFlags propertyFlags,
        const PropertyOptions<TValue>& options) noexcept {
        if (!builder_.Ok()) return *this;
        if constexpr (
            std::is_same_v<
                TValue,
                ::Aero::Meta::Value>) {
            propertyFlags =
                propertyFlags |
                DependencyPropertyFlags::AnyValue;
        }
        if (options.IsStructural()) {
            propertyFlags =
                propertyFlags |
                DependencyPropertyFlags::Structural;
        }
        Base::Result<::Aero::Meta::Value> encoded =
            builder_.Encode(options.DefaultValue());
        if (!encoded) {
            builder_.Fail(encoded.GetStatus());
            return *this;
        }
        builder_.DependencyProperty(
            handle, name, ValueCodec<TValue>::Type(),
            std::move(encoded).Value(), options.Flags(),
            propertyFlags,
            options.Validator(), options.Coercer(),
            options.ChangeCallback(),
            options.DefaultUpdateSourceTrigger());
        return *this;
    }

    Detail::MetadataAuthoringSession builder_;
};

} // namespace Aero::Meta

namespace Aero::Meta {

// Public metadata authoring entry. The fluent description object is an
// implementation type, while module code only names Register and
// Registration.
template<class T>
TypeDescription<T> Register(
    Registration& registration,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeDescription<T>(registration, flags);
}

template<class T>
TypeDescription<T> Register(
    Registration& registration,
    Base::StringView name,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeDescription<T>(
        registration, AeroNamespaceUri(), name, flags);
}

template<class T>
TypeDescription<T> Register(
    Registration& registration,
    Base::StringView metadataNamespace,
    Base::StringView name,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeDescription<T>(
        registration, metadataNamespace, name, flags);
}

} // namespace Aero::Meta
