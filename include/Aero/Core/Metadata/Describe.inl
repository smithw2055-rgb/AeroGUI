#if !defined(AERO_METADATA_DESCRIBE_IMPLEMENTATION)
#error "Describe.inl is an implementation companion; include <Aero/Core/Metadata/Describe.hpp>"
#endif

#include <Aero/Core/Metadata/MetadataContext.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Core::Detail {

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
struct MemberPointerTraits<Member> final {
    using OwnerType = Owner;
    using FieldType = Field;
};

template<class Owner, class Field, Field Owner::*Member>
Base::Result<Value> GetField(
    const void* object,
    MetadataRuntime& runtime,
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
Base::Result<void> SetField(
    void* object,
    const Value& value,
    MetadataRuntime& runtime,
    void*) noexcept {
    if (object == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field target is null");
    }
    Base::Result<Field> decoded =
        ValueCodec<Field>::Decode(runtime, value);
    if (!decoded) return decoded.GetStatus();
    static_cast<Owner*>(object)->*Member =
        std::move(decoded).Value();
    return {};
}

class AERO_API MetadataAuthoringSession final {
public:
    MetadataAuthoringSession(
        MetadataContext& context,
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
        MetadataRegistrationValues values =
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

    MetadataContext* context_ = nullptr;
    TypeId type_ = InvalidTypeId;
    Base::Status status_;
};

template<class T>
MetadataAuthoringSession CreateDescriptionSession(
    MetadataContext& context,
    TypeFlags flags) noexcept {
    if constexpr (std::is_enum_v<T>) {
        if constexpr (
            std::is_signed_v<std::underlying_type_t<T>>) {
            flags = flags | TypeFlags::SignedEnum;
        }
        return MetadataAuthoringSession(
            context,
            TypeRegistration::Enum(
                MetaTypeTraits<T>::Namespace(),
                MetaTypeTraits<T>::Name(),
                TypeOf<std::uint32_t>(),
                flags),
            TypeOf<T>());
    } else if constexpr (
        std::is_arithmetic_v<T> ||
        std::is_same_v<T, Base::String>) {
        return MetadataAuthoringSession(
            context,
            TypeRegistration::Primitive(
                MetaTypeTraits<T>::Namespace(),
                MetaTypeTraits<T>::Name(),
                flags),
            TypeOf<T>());
    } else if constexpr (
        std::is_base_of_v<Base::Object, T>) {
        return MetadataAuthoringSession(
            context,
            TypeRegistration::Object(
                MetaTypeTraits<T>::Namespace(),
                MetaTypeTraits<T>::Name(),
                MetaTypeTraits<T>::BaseType(),
                flags),
            TypeOf<T>());
    } else {
        if constexpr (std::is_trivially_copyable_v<T>) {
            flags = flags | TypeFlags::TriviallyCopyable;
        }
        return MetadataAuthoringSession(
            context,
            TypeRegistration::Struct(
                MetaTypeTraits<T>::Namespace(),
                MetaTypeTraits<T>::Name(),
                MetaTypeTraits<T>::BaseType(),
                flags),
            TypeOf<T>());
    }
}

} // namespace Aero::Core::Detail
