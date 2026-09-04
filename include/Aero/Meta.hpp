#pragma once

#include <Aero/Value.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Module.hpp>

#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero {
struct RoutedEventArgs;
}

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
using InterfaceCastThunk = void* (*)(Base::Object* object) noexcept;
using EventHandlerThunk = void (*)(
    Object* target,
    Object* sender,
    ::Aero::RoutedEventArgs& args) noexcept;

template<class T, class TInterface>
void* CastObjectToInterface(Base::Object* object) noexcept {
    static_assert(
        std::is_base_of_v<TInterface, T>,
        "Implements<TInterface>() requires T to derive the interface");
    if (object == nullptr) {
        return nullptr;
    }
    return static_cast<TInterface*>(static_cast<T*>(object));
}
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

} // namespace Aero::Meta

namespace Aero::Meta {
class MetadataAuthoringSession;
class RegistrationValues;
class RegistrationTypes;
} // namespace Aero::Meta

namespace Aero::Meta {

class DependencyPropertyRegistry;
class ValueTable;
template<class T>
class TypeBuilder;

} // namespace Aero::Meta

namespace Aero::Meta {

class Registry;

// Callback-scoped metadata authoring session. Module authors use Register<T>
// against this object; mutable tables and registration storage stay private to
// Registry.
class AERO_GUI_API Registration {
private:
    friend class Registry;
    template<class T>
    friend class TypeBuilder;
    friend class MetadataAuthoringSession;

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

class TypeRegistry;

AERO_GUI_API Base::Result<Value> CreateRegistrationValue(
    void* registrationState,
    TypeId type,
    const void* source) noexcept;
AERO_GUI_API RegistrationValues MakeRegistrationValues(
    void* registrationState) noexcept;

// Opaque callback-scoped value registration view used by ValueCodec. The
// backing registration store remains a Core implementation detail.
class AERO_GUI_API RegistrationValues {
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
    friend Base::Result<Value> CreateRegistrationValue(
        void* registrationState,
        TypeId type,
        const void* source) noexcept;
    friend RegistrationValues
    MakeRegistrationValues(
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

// Authoring session + TypeBuilder helpers (non-Aero private path).
#include "gui/meta/TypeBuilderDetail.hpp"

namespace Aero::Meta {

template<class TValue>
class FrameworkPropertyMetadata {
public:
    explicit FrameworkPropertyMetadata(
        TValue defaultValue,
        FrameworkPropertyMetadataOptions options =
            FrameworkPropertyMetadataOptions::None) noexcept
        : defaultValue_(std::move(defaultValue)),
          flags_(ToPropertyMetadataFlags(options)) {}

    FrameworkPropertyMetadata& Inherits() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::Inherits;
        return *this;
    }
    FrameworkPropertyMetadata& AffectsMeasure() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsMeasure;
        return *this;
    }
    FrameworkPropertyMetadata& AffectsArrange() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsArrange;
        return *this;
    }
    FrameworkPropertyMetadata& AffectsRender() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsRender;
        return *this;
    }
    FrameworkPropertyMetadata& AffectsParentMeasure() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsParentMeasure;
        return *this;
    }
    FrameworkPropertyMetadata& AffectsParentArrange() noexcept {
        flags_ = flags_ | PropertyMetadataFlags::AffectsParentArrange;
        return *this;
    }
    FrameworkPropertyMetadata& Structural() noexcept {
        structural_ = true;
        return *this;
    }
    FrameworkPropertyMetadata& BindsTwoWayByDefault() noexcept {
        flags_ = flags_ |
            PropertyMetadataFlags::BindsTwoWayByDefault;
        return *this;
    }
    FrameworkPropertyMetadata& Apply(
        FrameworkPropertyMetadataOptions options) noexcept {
        flags_ = flags_ | ToPropertyMetadataFlags(options);
        return *this;
    }
    FrameworkPropertyMetadata& UpdateSource(
        UpdateSourceTrigger trigger) noexcept {
        updateSourceTrigger_ = trigger;
        return *this;
    }
    FrameworkPropertyMetadata& Validate(
        ValidateValueCallback validate) noexcept {
        validate_ = validate;
        return *this;
    }
    FrameworkPropertyMetadata& Validate(
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
    FrameworkPropertyMetadata& Coerce(
        CoerceValueCallback coerce) noexcept {
        coerce_ = coerce;
        return *this;
    }
    FrameworkPropertyMetadata& Coerce(
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
    FrameworkPropertyMetadata& Changed(
        PropertyChangedCallback changed) noexcept {
        changed_ = changed;
        return *this;
    }
    FrameworkPropertyMetadata& Changed(
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
class TypeBuilder {
public:
    // P2.5 public fluent contract. Ordinary module authors use only:
    //   Factory / Implements / Property / Event+EventHandler / Override /
    //   Content+Collection / AddOwner / ValueSemantics / TextConverter /
    //   PropertyChangeNotifications / CollectionChangeNotifications / Value.
    // Raw implementation-only overloads (MemberId/callable forms) live in
    // the consolidated AERO_GUI_IMPLEMENTATION section below and are
    // invisible to SDK consumers.
    explicit TypeBuilder(
        Registration& context,
        TypeFlags flags = TypeFlags::None) noexcept
        : builder_(CreateDescriptionSession<T>(
              context, flags)) {}

    TypeBuilder(
        Registration& context,
        StringView metadataNamespace,
        StringView metadataName,
        TypeFlags flags = TypeFlags::None) noexcept
        : builder_(CreateNamedDescriptionSession<T>(
              context, metadataNamespace, metadataName, flags)) {}

    TypeBuilder(const TypeBuilder&) = delete;
    TypeBuilder& operator=(const TypeBuilder&) = delete;
    TypeBuilder(TypeBuilder&&) noexcept = default;
    TypeBuilder& operator=(TypeBuilder&&) noexcept = default;

    TypeBuilder& Factory() noexcept {
        builder_.Factory(
            &CreateDefaultObject<T>);
        return *this;
    }
    template<class TInterface>
    TypeBuilder& Implements() noexcept {
        builder_.Implements(
            TypeOf<TInterface>(),
            &CastObjectToInterface<T, TInterface>);
        return *this;
    }

    template<class TOwner, class TValue>
    TypeBuilder& Property(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const FrameworkPropertyMetadata<TValue>& options) noexcept {
        return RegisterProperty(
            property.Handle(), property.Name(),
            DependencyPropertyFlags::None, options);
    }

    template<class TOwner, class TValue>
    TypeBuilder& Property(
        const AttachedPropertyRef<TOwner, TValue>& property,
        const FrameworkPropertyMetadata<TValue>& options) noexcept {
        return RegisterProperty(
            property.Handle(), property.Name(),
            DependencyPropertyFlags::Attached, options);
    }

    template<class TOwner, class TValue>
    TypeBuilder& Property(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const FrameworkPropertyMetadata<TValue>& options) noexcept {
        return RegisterProperty(
            property.Handle(), property.Name(),
            DependencyPropertyFlags::ReadOnly, options);
    }

    template<
        class TValue,
        auto Getter,
        auto Setter>
    TypeBuilder& Property(
        StringView name,
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
            &GetOrdinaryProperty<
                T, TValue, Getter>;
        registration.set =
            &SetOrdinaryProperty<
                T, TValue, Setter>;
        builder_.Property(registration);
        return *this;
    }

    template<class TValue, auto Setter>
    TypeBuilder& Property(
        StringView name,
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
            &SetOrdinaryProperty<
                T, TValue, Setter>;
        builder_.Property(registration);
        return *this;
    }

    template<auto Getter, auto Setter>
    TypeBuilder& Property(
        StringView name,
        PropertyFlags flags = PropertyFlags::None) noexcept {
        using GetterResult = std::invoke_result_t<
            decltype(Getter), const T&>;
        using TValue = std::remove_cv_t<
            std::remove_reference_t<GetterResult>>;
        return Property<TValue, Getter, Setter>(
            name, flags);
    }

    template<class TGetter, class TSetter>
    TypeBuilder& Property(
        StringView name,
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
            std::is_same_v<GetterValue, StringView>,
            String,
            GetterValue>;
        static_assert(
            std::is_invocable_v<TSetter, T&, TValue> ||
            (std::is_same_v<TValue, String> &&
             std::is_invocable_v<
                 TSetter, T&, StringView>),
            "Ordinary metadata property setter is incompatible with getter");

        if (!builder_.Ok()) return *this;
        using Adapter = OrdinaryPropertyAdapter<
            T, TValue, TGetter, TSetter>;
        ::Aero::Result<Adapter*> adapter =
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
    TypeBuilder& Field(
        StringView name,
        FieldFlags flags = FieldFlags::None) noexcept {
        using Traits = MemberPointerTraits<Member>;
        using Owner = typename Traits::OwnerType;
        using FieldType = typename Traits::FieldType;
        static_assert(std::is_same_v<Owner, T>,
            "Metadata field member must belong to the described struct");
        builder_.Field({
            name,
            ValueCodec<FieldType>::Type(),
            flags,
            &GetField<Owner, FieldType, Member>,
            &SetField<Owner, FieldType, Member>,
            nullptr});
        return *this;
    }

    template<class TOwner, class TArgs>
    TypeBuilder& Event(
        const ::Aero::RoutedEventRef<TOwner, TArgs>& event,
        RoutingStrategy strategy = RoutingStrategy::Bubble) noexcept {
        static_assert(std::is_same_v<TOwner, T>,
            "Routed event owner must match described type");
        builder_.RoutedEvent(
            event.Handle(), event.Name(),
            TypeOf<TArgs>(), strategy);
        return *this;
    }

    // Describes a conventional code-behind handler used by XAML attributes
    // such as Click="OnHelloClick". The runtime connects the named method to
    // the routed event; users do not author Registry or facet callbacks.
    template<class TArgs, auto Handler>
    TypeBuilder& EventHandler(
        StringView name) noexcept {
        static_assert(std::is_invocable_v<
            decltype(Handler), T&, Object*, TArgs&>,
            "XAML event handler must accept (Object*, EventArgs& or const EventArgs&)");
        builder_.EventHandler(
            name,
            static_cast<EventHandlerThunk>(
                [](Object* target, Object* sender, RoutedEventArgs& args) noexcept {
                    if (target != nullptr) {
                        std::invoke(
                            Handler,
                            static_cast<T&>(*target),
                            sender,
                            static_cast<TArgs&>(args));
                    }
                }));
        return *this;
    }

    template<class TOwner, class TValue>
    TypeBuilder& Override(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const FrameworkPropertyMetadata<TValue>& options) noexcept {
        if (!builder_.Ok()) return *this;
        ::Aero::Result<::Aero::Meta::Value> encoded =
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

    TypeBuilder& Content(
        StringView name,
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
    TypeBuilder& Content(
        StringView name,
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
    TypeBuilder& Collection(
        StringView name,
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

    template<class TOwner, class TValue>
    TypeBuilder& AddOwner(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const FrameworkPropertyMetadata<TValue>& options) noexcept {
        if (!builder_.Ok()) return *this;
        ::Aero::Result<::Aero::Meta::Value> encoded =
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
            std::move(metadata),
            DependencyPropertyFlags::None);
        return *this;
    }

    template<class TOwner, class TValue>
    TypeBuilder& AddOwner(
        const AttachedPropertyRef<TOwner, TValue>& property,
        const FrameworkPropertyMetadata<TValue>& options) noexcept {
        if (!builder_.Ok()) return *this;
        ::Aero::Result<::Aero::Meta::Value> encoded =
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
            std::move(metadata),
            DependencyPropertyFlags::Attached);
        return *this;
    }

    template<class TAliasOwner, class TOwner, class TValue>
    TypeBuilder& AddOwner(
        const AttachedPropertyRef<TAliasOwner, TValue>& /*aliasProperty*/,
        const DependencyPropertyRef<TOwner, TValue>& sourceProperty,
        const FrameworkPropertyMetadata<TValue>& options) noexcept {
        if (!builder_.Ok()) return *this;
        ::Aero::Result<::Aero::Meta::Value> encoded =
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
            sourceProperty.Handle(), TypeOf<T>(),
            std::move(metadata),
            DependencyPropertyFlags::Attached);
        return *this;
    }

    TypeBuilder& ValueSemantics() noexcept {
        builder_.ValueSemantics(
            MakeValueTypeRegistration<T>());
        return *this;
    }

    template<auto Converter>
    TypeBuilder& TextConverter() noexcept {
        builder_.TextConverter(
            &ConvertTypedText<T, Converter>);
        return *this;
    }

    TypeBuilder& PropertyChangeNotifications() noexcept {
        builder_.PropertyChangeNotifications(
            &T::SubscribePropertyChanged,
            &T::UnsubscribePropertyChanged,
            nullptr);
        return *this;
    }

    TypeBuilder& PropertyChangeNotifications(
        PropertyChangeSubscribeCallback subscribe,
        PropertyChangeUnsubscribeCallback unsubscribe,
        void* callbackContext = nullptr) noexcept {
        builder_.PropertyChangeNotifications(
            subscribe, unsubscribe, callbackContext);
        return *this;
    }

    TypeBuilder& CollectionChangeNotifications(
        CollectionChangeSubscribeCallback subscribe,
        CollectionChangeUnsubscribeCallback unsubscribe,
        void* callbackContext = nullptr) noexcept {
        builder_.CollectionChangeNotifications(
            subscribe, unsubscribe, callbackContext);
        return *this;
    }

#if defined(AERO_GUI_IMPLEMENTATION)
    // P2.5 / B4: implementation-only overloads live in TypeBuilderInternal.inc
#include "gui/meta/TypeBuilderInternal.inc"
#endif

    TypeBuilder& Value(
        StringView name,
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

    ::Aero::Result<void> Result() const noexcept {
        return builder_.Finish();
    }
    bool Ok() const noexcept { return builder_.Ok(); }

private:
    template<class TValue>
    TypeBuilder& RegisterProperty(
        DependencyPropertyHandle handle,
        StringView name,
        DependencyPropertyFlags propertyFlags,
        const FrameworkPropertyMetadata<TValue>& options) noexcept {
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
        ::Aero::Result<::Aero::Meta::Value> encoded =
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

    MetadataAuthoringSession builder_;
};

} // namespace Aero::Meta

namespace Aero::Meta {

// Public metadata authoring entry. The fluent description object is an
// implementation type, while module code only names Register and
// Registration.
template<class T>
TypeBuilder<T> Register(
    Registration& registration,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeBuilder<T>(registration, flags);
}

template<class T>
TypeBuilder<T> Register(
    Registration& registration,
    StringView name,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeBuilder<T>(
        registration, AeroNamespaceUri(), name, flags);
}

template<class T>
TypeBuilder<T> Register(
    Registration& registration,
    StringView metadataNamespace,
    StringView name,
    TypeFlags flags = TypeFlags::None) noexcept {
    return TypeBuilder<T>(
        registration, metadataNamespace, name, flags);
}

} // namespace Aero::Meta

namespace Aero::Meta {

template<class T, class = void>
struct HasComponentDescription : std::false_type {};

template<class T>
struct HasComponentDescription<T, std::void_t<decltype(
    T::DescribeComponent(
        std::declval<TypeBuilder<T>&>()))>>
    : std::true_type {};

template<class T>
Result<void> RegisterComponentType(
    Registration& registration) noexcept {
    auto type = Register<T>(registration);
    type.Factory();
    if constexpr (HasComponentDescription<T>::value) {
        T::DescribeComponent(type);
    }
    return type.Result();
}

template<class... TComponents>
Result<void> RegisterComponentTypes(
    Registration& registration) noexcept {
    Result<void> status;
    const bool registered = ((status
        ? static_cast<bool>(
              status = RegisterComponentType<TComponents>(registration))
        : false) && ...);
    static_cast<void>(registered);
    return status;
}

} // namespace Aero::Meta

namespace Aero {

// One module declaration registers ordinary code-behind/custom-control types,
// default factories, and optional DescribeComponent metadata. Applications no
// longer author Registry or XAML facet callbacks for these types.
template<class... TComponents>
constexpr ModuleRegistration DefineComponentModule(
    StringView name) noexcept {
    return DefineModule(
        name,
        &Meta::RegisterComponentTypes<TComponents...>);
}

} // namespace Aero
