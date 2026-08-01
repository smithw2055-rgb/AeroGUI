#pragma once

#include <Aero/Meta/MetadataRegistrationValues.hpp>
#include <Aero/Meta/ValueCodec.hpp>
#include <Aero/RoutedEvent.hpp>

#include <functional>
#include <type_traits>
#include <utility>

#define AERO_METADATA_DESCRIBE_IMPLEMENTATION 1
#include <Aero/Meta/Describe.inl>
#undef AERO_METADATA_DESCRIBE_IMPLEMENTATION

namespace Aero::Core {

namespace Detail {

template<class T>
struct IsResultVoid final : std::false_type {};

template<>
struct IsResultVoid<Base::Result<void>> final
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
    MetadataRegistrationValues registrations =
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
        Base::Result<void> assigned = copied.TryAssign(
            std::invoke(Getter, owner));
        if (!assigned) return assigned.GetStatus();
        return ValueCodec<TValue>::Encode(copied);
    } else {
        return ValueCodec<TValue>::Encode(
            std::invoke(Getter, owner));
    }
}

template<class TOwner, class TValue, auto Setter>
Base::Result<void> SetOrdinaryProperty(
    Base::Object& object,
    const Value& stored,
    void*) noexcept {
    static_assert(
        std::is_base_of_v<Base::Object, TOwner>);
    Base::Result<TValue> decoded =
        ValueCodec<TValue>::Decode(stored);
    if (!decoded) return decoded.GetStatus();
    auto& owner = static_cast<TOwner&>(object);
    if constexpr (
        std::is_same_v<TValue, Base::String> &&
        std::is_invocable_v<
            decltype(Setter),
            TOwner&,
            Base::StringView>) {
        using Result = std::invoke_result_t<
            decltype(Setter), TOwner&, Base::StringView>;
        if constexpr (IsResultVoid<Result>::value) {
            return std::invoke(
                Setter, owner, decoded.Value().View());
        } else {
            std::invoke(
                Setter, owner, decoded.Value().View());
            return {};
        }
    } else {
        using Result = std::invoke_result_t<
            decltype(Setter), TOwner&, TValue>;
        if constexpr (IsResultVoid<Result>::value) {
            return std::invoke(
                Setter, owner,
                std::move(decoded).Value());
        } else {
            std::invoke(
                Setter, owner,
                std::move(decoded).Value());
            return {};
        }
    }
}

template<class TOwner, class TValue, class TGetter, class TSetter>
struct OrdinaryPropertyAdapter final {
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
            Base::Result<void> assigned = copied.TryAssign(
                std::invoke(adapter->getter, owner));
            if (!assigned) return assigned.GetStatus();
            return ValueCodec<TValue>::Encode(copied);
        } else {
            return ValueCodec<TValue>::Encode(
                std::invoke(adapter->getter, owner));
        }
    }

    static Base::Result<void> Set(
        Base::Object& object,
        const Value& stored,
        void* context) noexcept {
        const auto* adapter =
            static_cast<const OrdinaryPropertyAdapter*>(context);
        if (adapter == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Ordinary metadata property adapter is unavailable");
        }
        Base::Result<TValue> decoded =
            ValueCodec<TValue>::Decode(stored);
        if (!decoded) return decoded.GetStatus();
        auto& owner = static_cast<TOwner&>(object);
        if constexpr (
            std::is_same_v<TValue, Base::String> &&
            std::is_invocable_v<
                TSetter, TOwner&, Base::StringView>) {
            using Result = std::invoke_result_t<
                TSetter, TOwner&, Base::StringView>;
            if constexpr (IsResultVoid<Result>::value) {
                return std::invoke(
                    adapter->setter,
                    owner,
                    decoded.Value().View());
            } else {
                std::invoke(
                    adapter->setter,
                    owner,
                    decoded.Value().View());
                return {};
            }
        } else {
            using Result = std::invoke_result_t<
                TSetter, TOwner&, TValue>;
            if constexpr (IsResultVoid<Result>::value) {
                return std::invoke(
                    adapter->setter,
                    owner,
                    std::move(decoded).Value());
            } else {
                std::invoke(
                    adapter->setter,
                    owner,
                    std::move(decoded).Value());
                return {};
            }
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
                ValueCodec<TValue>::Decode(args.oldValue);
            Base::Result<TValue> newValue =
                ValueCodec<TValue>::Decode(args.newValue);
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
class TypeDescription final {
public:
    explicit TypeDescription(
        MetadataContext& context,
        TypeFlags flags = TypeFlags::None) noexcept
        : builder_(Detail::CreateDescriptionSession<T>(
              context, flags)) {}

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
        const Aero::RoutedEventRef<TOwner, TArgs>& event,
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
        Base::Result<::Aero::Core::Value> encoded =
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
        Base::Result<::Aero::Core::Value> encoded =
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
            "Describe<T>::Value requires an enum type");
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
                ::Aero::Core::Value>) {
            propertyFlags =
                propertyFlags |
                DependencyPropertyFlags::AnyValue;
        }
        if (options.IsStructural()) {
            propertyFlags =
                propertyFlags |
                DependencyPropertyFlags::Structural;
        }
        Base::Result<::Aero::Core::Value> encoded =
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

template<class T>
TypeDescription<T> Describe(
    MetadataContext& context,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeDescription<T>(context, flags);
}

} // namespace Aero::Core
