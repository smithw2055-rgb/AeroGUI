#pragma once

// Internal implementation for the public Describe<T> API.

#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Metadata/ValueCodec.hpp>

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

template<class T>
Base::Result<Base::Ref<Base::Object>> CreateDefaultObject() noexcept {
    static_assert(std::is_base_of_v<Base::Object, T>,
        "Default metadata factories require Object-derived types");
    static_assert(std::is_default_constructible_v<T>,
        "Default metadata factories require default-constructible types");
    static_assert(!std::is_abstract_v<T>,
        "Default metadata factories cannot construct abstract types");
    Base::Result<Base::Ref<T>> created = Base::MakeRef<T>();
    if (!created) return created.GetStatus();
    return Base::Ref<Base::Object>(std::move(created).Value());
}

template<auto Member>
struct MemberPointerTraits;

template<class Owner, class Field, Field Owner::*Member>
struct MemberPointerTraits<Member> final {
    using OwnerType = Owner;
    using FieldType = Field;
};

} // namespace Detail

namespace Detail {

template<class Owner, class Field, Field Owner::*Member>
Base::Result<Value> GetField(const void* object, MetadataRuntime& runtime, void*) noexcept {
    if (object == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata value field target is null");
    return ValueCodec<Field>::Encode(
        runtime, static_cast<const Owner*>(object)->*Member);
}

template<class Owner, class Field, Field Owner::*Member>
Base::Result<void> SetField(void* object, const Value& value, MetadataRuntime& runtime, void*) noexcept {
    if (object == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata value field target is null");
    Base::Result<Field> decoded =
        ValueCodec<Field>::Decode(runtime, value);
    if (!decoded) return decoded.GetStatus();
    static_cast<Owner*>(object)->*Member = std::move(decoded).Value();
    return {};
}

} // namespace Detail

namespace Detail {

template<class T>
class DescriptionBuilder final {
public:
    DescriptionBuilder(const DescriptionBuilder&) = delete;
    DescriptionBuilder& operator=(const DescriptionBuilder&) = delete;
    DescriptionBuilder(DescriptionBuilder&&) noexcept = default;
    DescriptionBuilder& operator=(DescriptionBuilder&&) noexcept = default;

    static DescriptionBuilder Object(
        MetadataContext& context,
        TypeFlags flags = TypeFlags::None) noexcept {
        return DescriptionBuilder(context, TypeRegistration::Object(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(),
            MetaTypeTraits<T>::BaseType(), flags));
    }

    static DescriptionBuilder Interface(
        MetadataContext& context,
        TypeFlags flags = TypeFlags::None) noexcept {
        return DescriptionBuilder(context, TypeRegistration::Interface(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(), flags));
    }

    static DescriptionBuilder Struct(
        MetadataContext& context,
        TypeFlags flags = TypeFlags::None) noexcept {
        if constexpr (std::is_trivially_copyable_v<T>) {
            flags = flags | TypeFlags::TriviallyCopyable;
        }
        return DescriptionBuilder(context, TypeRegistration::Struct(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(),
            MetaTypeTraits<T>::BaseType(), flags));
    }

    static DescriptionBuilder Enum(
        MetadataContext& context,
        TypeId underlyingType,
        TypeFlags flags = TypeFlags::None) noexcept {
        static_assert(std::is_enum_v<T>,
            "DescriptionBuilder::Enum requires an enum type");
        if constexpr (std::is_signed_v<std::underlying_type_t<T>>) {
            flags = flags | TypeFlags::SignedEnum;
        }
        return DescriptionBuilder(context, TypeRegistration::Enum(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(),
            underlyingType, flags));
    }

    static DescriptionBuilder Primitive(
        MetadataContext& context,
        TypeFlags flags = TypeFlags::None) noexcept {
        return DescriptionBuilder(context, TypeRegistration::Primitive(
            MetaTypeTraits<T>::Namespace(), MetaTypeTraits<T>::Name(), flags));
    }

    DescriptionBuilder& Implements(TypeId interfaceType) noexcept {
        if (Ok()) Record(context_->Types().TryRegisterInterface(type_, interfaceType));
        return *this;
    }
    template<class Interface> DescriptionBuilder& Implements() noexcept { return Implements(TypeOf<Interface>()); }
    DescriptionBuilder& Factory(ObjectFactory factory) noexcept {
        if (Ok()) Record(context_->Types().TrySetFactory(type_, factory));
        return *this;
    }
    DescriptionBuilder& DefaultFactory() noexcept {
        return Factory(&Detail::CreateDefaultObject<T>);
    }
    DescriptionBuilder& PropertyChangeNotifications(
        PropertyChangeSubscribeCallback subscribe,
        PropertyChangeUnsubscribeCallback unsubscribe,
        void* callbackContext = nullptr) noexcept {
        if (Ok()) {
            Record(context_->Types().TryRegisterPropertyChangeNotification(
                {type_, subscribe, unsubscribe, callbackContext}));
        }
        return *this;
    }
    DescriptionBuilder& CollectionChangeNotifications(
        CollectionChangeSubscribeCallback subscribe,
        CollectionChangeUnsubscribeCallback unsubscribe,
        void* callbackContext = nullptr) noexcept {
        if (Ok()) {
            Record(context_->Types().TryRegisterCollectionChangeNotification(
                {type_, subscribe, unsubscribe, callbackContext}));
        }
        return *this;
    }

    DescriptionBuilder& DependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView name,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr,
        PropertyChangedCallback changed = nullptr,
        UpdateSourceTrigger updateSourceTrigger =
            UpdateSourceTrigger::Default) noexcept {
        return RegisterDependencyProperty(declaredHandle, name, valueType,
            std::move(defaultValue), metadataFlags,
            DependencyPropertyFlags::None, validate, coerce,
            changed, updateSourceTrigger);
    }

    DescriptionBuilder& AttachedDependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView name,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr,
        PropertyChangedCallback changed = nullptr,
        UpdateSourceTrigger updateSourceTrigger =
            UpdateSourceTrigger::Default) noexcept {
        return RegisterDependencyProperty(declaredHandle, name, valueType,
            std::move(defaultValue), metadataFlags,
            DependencyPropertyFlags::Attached, validate, coerce,
            changed, updateSourceTrigger);
    }

    DescriptionBuilder& ReadOnlyDependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView name,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr,
        PropertyChangedCallback changed = nullptr,
        UpdateSourceTrigger updateSourceTrigger =
            UpdateSourceTrigger::Default) noexcept {
        return RegisterDependencyProperty(declaredHandle, name, valueType,
            std::move(defaultValue), metadataFlags,
            DependencyPropertyFlags::ReadOnly, validate, coerce,
            changed, updateSourceTrigger);
    }

    DescriptionBuilder& RoutedEvent(
        RoutedEventHandle declaredHandle,
        Base::StringView name,
        TypeId eventArgsType,
        RoutingStrategy strategy) noexcept {
        if (!Ok()) return *this;
        RoutedEventCatalog* events = context_->RoutedEvents();
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

    template<class TValue>
    DescriptionBuilder& Content(
        Base::StringView name,
        ContentKind kind,
        ContentWriteCallback write = nullptr,
        ContentClearCallback clear = nullptr,
        ContentFlags contentFlags = ContentFlags::None,
        void* contentContext = nullptr) noexcept {
        return Content(name, TypeOf<TValue>(), kind, write, clear,
            contentFlags, contentContext);
    }

    DescriptionBuilder& Content(
        Base::StringView name,
        TypeId valueType,
        ContentKind kind,
        ContentWriteCallback write = nullptr,
        ContentClearCallback clear = nullptr,
        ContentFlags contentFlags = ContentFlags::None,
        void* contentContext = nullptr) noexcept {
        if (!Ok()) return *this;
        if (valueType == InvalidTypeId) {
            return Fail(Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Content property value type is invalid"));
        }
        if ((write == nullptr) != (clear == nullptr) ||
            (write == nullptr && contentFlags != ContentFlags::None)) {
            return Fail(Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Content access requires matching write and clear callbacks"));
        }
        PropertyFlags flags = PropertyFlags::Structural;
        if (kind == ContentKind::Collection) {
            flags = flags | PropertyFlags::Collection;
        }
        Base::Result<MemberId> member = context_->Types().TryRegisterProperty(
            type_, {name, valueType, flags});
        if (!member) return Fail(member.GetStatus());
        Record(context_->Types().TrySetContentMember(type_, member.Value()));
        if (write != nullptr) {
            Record(context_->Types().TrySetContentAccessor({
                type_, member.Value(), kind, contentFlags,
                write, clear, contentContext}));
        }
        return *this;
    }

    DescriptionBuilder& Collection(
        Base::StringView name,
        TypeId valueType,
        ContentWriteCallback write,
        ContentClearCallback clear,
        PropertyFlags propertyFlags =
            PropertyFlags::Structural,
        ContentFlags contentFlags = ContentFlags::None,
        void* contentContext = nullptr) noexcept {
        if (!Ok()) return *this;
        if (valueType == InvalidTypeId ||
            write == nullptr || clear == nullptr) {
            return Fail(Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Collection property requires a value type and access callbacks"));
        }
        Base::Result<MemberId> member =
            context_->Types().TryRegisterProperty(
                type_,
                {
                    name,
                    valueType,
                    propertyFlags |
                        PropertyFlags::Structural |
                        PropertyFlags::Collection});
        if (!member) return Fail(member.GetStatus());
        Record(context_->Types().TrySetContentAccessor({
            type_,
            member.Value(),
            ContentKind::Collection,
            contentFlags,
            write,
            clear,
            contentContext}));
        return *this;
    }

    DescriptionBuilder& Property(const PropertyRegistration& registration) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterProperty(type_, registration); Record(result); } return *this; }
    DescriptionBuilder& Field(const FieldRegistration& registration) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterField(type_, registration); Record(result); } return *this; }

    template<auto Member>
    DescriptionBuilder& Field(Base::StringView name, FieldFlags flags = FieldFlags::None) noexcept {
        using Traits = Detail::MemberPointerTraits<Member>;
        using Owner = typename Traits::OwnerType;
        using FieldType = typename Traits::FieldType;
        static_assert(std::is_same_v<Owner, T>, "Metadata field member must belong to the described struct");
        return Field({name, ValueCodec<FieldType>::Type(), flags, &Detail::GetField<Owner, FieldType, Member>, &Detail::SetField<Owner, FieldType, Member>, nullptr});
    }

    DescriptionBuilder& Event(const EventRegistration& registration) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterEvent(type_, registration); Record(result); } return *this; }
    DescriptionBuilder& Method(const MethodRegistration& registration) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterMethod(type_, registration); Record(result); } return *this; }
    DescriptionBuilder& EnumValueRaw(Base::StringView name, std::uint64_t rawValue) noexcept { if (Ok()) { auto result = context_->Types().TryRegisterEnumValue(type_, {name, rawValue}); Record(result); } return *this; }
    DescriptionBuilder& EnumValue(Base::StringView name, T value) noexcept {
        static_assert(std::is_enum_v<T>, "Typed EnumValue requires an enum builder");
        using Underlying = std::underlying_type_t<T>;
        using Unsigned = std::make_unsigned_t<Underlying>;
        return EnumValueRaw(name, static_cast<std::uint64_t>(static_cast<Unsigned>(static_cast<Underlying>(value))));
    }
    DescriptionBuilder& Content(MemberId member) noexcept { if (Ok()) Record(context_->Types().TrySetContentMember(type_, member)); return *this; }
    DescriptionBuilder& ValueSemantics(
        const ValueTypeRegistration& registration) noexcept {
        if (Ok()) {
            Record(context_->Values().TryRegisterValueSemantics(
                type_, registration));
        }
        return *this;
    }

    DescriptionBuilder& ValueSemantics() noexcept {
        return ValueSemantics(Detail::MakeValueTypeRegistration<T>());
    }
    DescriptionBuilder& TextConverter(TextValueConverterCallback converter, void* context = nullptr) noexcept {
        if (Ok()) Record(context_->Values().TryRegisterTextConverter({type_, converter, context}));
        return *this;
    }
    DescriptionBuilder& Fail(Base::Status status) noexcept {
        if (status_.IsOk() && !status.IsOk()) status_ = status;
        return *this;
    }

    bool Ok() const noexcept { return status_.IsOk(); }
    TypeId Type() const noexcept { return type_; }
    Base::Status Status() const noexcept { return status_; }
    Base::Result<void> Finish() const noexcept { return status_.IsOk() ? Base::Result<void>() : Base::Result<void>(status_); }

private:
    DescriptionBuilder& RegisterDependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView name,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        DependencyPropertyFlags propertyFlags,
        ValidateValueCallback validate,
        CoerceValueCallback coerce,
        PropertyChangedCallback changed,
        UpdateSourceTrigger updateSourceTrigger) noexcept {
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
        registration.metadata.changed = changed;
        registration.metadata.defaultUpdateSourceTrigger =
            updateSourceTrigger;
        Base::Result<DependencyPropertyRegistrationResult> registered =
            context_->DependencyProperties().TryRegister(registration);
        if (!registered) return Fail(registered.GetStatus());
        if (registered.Value().property != declaredHandle) {
            return Fail(Base::Status::Failure(Base::ErrorCode::IdCollision,
                "Dependency property registry returned a different handle"));
        }
        return *this;
    }

    DescriptionBuilder(MetadataContext& context, const TypeRegistration& registration) noexcept : context_(&context) {
        Base::Result<TypeId> result = context_->Types().TryRegisterType(registration);
        if (!result) { status_ = result.GetStatus(); return; }
        type_ = result.Value();
        if (type_ != TypeOf<T>()) status_ = Base::Status::Failure(Base::ErrorCode::IdCollision, "Typed metadata descriptor does not match TypeOf<T>()");
    }
    void Record(Base::Result<void> result) noexcept { if (status_.IsOk() && !result) status_ = result.GetStatus(); }
    template<class U> void Record(Base::Result<U>& result) noexcept { if (status_.IsOk() && !result) status_ = result.GetStatus(); }
    MetadataContext* context_ = nullptr;
    TypeId type_ = InvalidTypeId;
    Base::Status status_;
};

} // namespace Detail

} // namespace Aero::Core
