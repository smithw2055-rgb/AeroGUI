#pragma once

#include <Aero/Meta.hpp>

namespace Aero {
class MetadataPrivate;
class MetaTable;
}

namespace Aero::Meta {
using Detail::CollectionChangeNotificationRegistration;
using Detail::CollectionChangeSubscribeCallback;
using Detail::CollectionChangeUnsubscribeCallback;
using Detail::ContentAccessorRegistration;
using Detail::ContentClearCallback;
using Detail::ContentWriteCallback;
using Detail::EnumValueRegistration;
using Detail::EventRegistration;
using Detail::FieldRegistration;
using Detail::MetadataCollectionChangeAction;
using Detail::MetadataCollectionChangedCallback;
using Detail::MetadataCollectionChangedEvent;
using Detail::MetadataPropertyChangedCallback;
using Detail::MethodInvokeCallback;
using Detail::MethodInvokerRegistration;
using Detail::MethodParameterRegistration;
using Detail::MethodRegistration;
using Detail::ObjectFactory;
using Detail::PropertyAccessorRegistration;
using Detail::PropertyChangeNotificationRegistration;
using Detail::PropertyChangeSubscribeCallback;
using Detail::PropertyChangeUnsubscribeCallback;
using Detail::PropertyGetCallback;
using Detail::PropertyRegistration;
using Detail::PropertySetCallback;
using Detail::RegistrationValues;
using Detail::RegistrationTypes;
using Detail::TypeFactoryRegistration;
using Detail::TypeRegistration;
using Detail::ValueMemberAccessorRegistration;
using Detail::ValueMemberGetCallback;
using Detail::ValueMemberSetCallback;

class PropertyInfo {
public:
    PropertyInfo(PropertyInfo&&) noexcept = default;
    PropertyInfo& operator=(PropertyInfo&&) noexcept = default;
    PropertyInfo(const PropertyInfo&) = delete;
    PropertyInfo& operator=(const PropertyInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId ValueType() const noexcept { return valueType_; }
    PropertyFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }
private:
    friend class TypeRegistry;
    PropertyInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId valueType_ = InvalidTypeId;
    PropertyFlags flags_ = PropertyFlags::None;
    Base::String name_;
};

class FieldInfo {
public:
    FieldInfo(FieldInfo&&) noexcept = default;
    FieldInfo& operator=(FieldInfo&&) noexcept = default;
    FieldInfo(const FieldInfo&) = delete;
    FieldInfo& operator=(const FieldInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId ValueType() const noexcept { return valueType_; }
    FieldFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }
private:
    friend class TypeRegistry;
    FieldInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId valueType_ = InvalidTypeId;
    FieldFlags flags_ = FieldFlags::None;
    Base::String name_;
};

class EnumValueInfo {
public:
    EnumValueInfo(EnumValueInfo&&) noexcept = default;
    EnumValueInfo& operator=(EnumValueInfo&&) noexcept = default;
    EnumValueInfo(const EnumValueInfo&) = delete;
    EnumValueInfo& operator=(const EnumValueInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    std::uint64_t RawValue() const noexcept { return rawValue_; }
    Base::StringView Name() const noexcept { return name_.View(); }
private:
    friend class TypeRegistry;
    EnumValueInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    std::uint64_t rawValue_ = 0U;
    Base::String name_;
};

class MethodParameterInfo {
public:
    MethodParameterInfo(MethodParameterInfo&&) noexcept = default;
    MethodParameterInfo& operator=(MethodParameterInfo&&) noexcept = default;
    MethodParameterInfo(const MethodParameterInfo&) = delete;
    MethodParameterInfo& operator=(const MethodParameterInfo&) = delete;
    Base::StringView Name() const noexcept { return name_.View(); }
    TypeId Type() const noexcept { return type_; }
private:
    friend class TypeRegistry;
    MethodParameterInfo() noexcept = default;
    TypeId type_ = InvalidTypeId;
    Base::String name_;
};

class MethodInfo {
public:
    MethodInfo(MethodInfo&&) noexcept = default;
    MethodInfo& operator=(MethodInfo&&) noexcept = default;
    MethodInfo(const MethodInfo&) = delete;
    MethodInfo& operator=(const MethodInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId ReturnType() const noexcept { return returnType_; }
    MethodFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }
    Base::Span<const MethodParameterInfo> Parameters() const noexcept {
        return {parameters_.Data(), parameters_.Size()};
    }
private:
    friend class TypeRegistry;
    MethodInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId returnType_ = InvalidTypeId;
    MethodFlags flags_ = MethodFlags::None;
    Base::String name_;
    Base::Vector<MethodParameterInfo> parameters_;
};

class EventInfo {
public:
    EventInfo(EventInfo&&) noexcept = default;
    EventInfo& operator=(EventInfo&&) noexcept = default;
    EventInfo(const EventInfo&) = delete;
    EventInfo& operator=(const EventInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId EventArgsType() const noexcept { return eventArgsType_; }
    EventFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }
private:
    friend class TypeRegistry;
    EventInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId eventArgsType_ = InvalidTypeId;
    EventFlags flags_ = EventFlags::None;
    Base::String name_;
};

class TypeInfo {
public:
    TypeInfo(TypeInfo&&) noexcept = default;
    TypeInfo& operator=(TypeInfo&&) noexcept = default;
    TypeInfo(const TypeInfo&) = delete;
    TypeInfo& operator=(const TypeInfo&) = delete;
    TypeId Id() const noexcept { return id_; }
    TypeId BaseType() const noexcept { return baseType_; }
    TypeId UnderlyingType() const noexcept { return underlyingType_; }
    MetadataTypeKind Kind() const noexcept { return kind_; }
    TypeFlags Flags() const noexcept { return flags_; }
    bool IsFlagsEnum() const noexcept {
        return kind_ == MetadataTypeKind::Enum &&
            HasTypeFlag(flags_, TypeFlags::FlagsEnum);
    }
    Base::StringView XamlNamespace() const noexcept { return xamlNamespace_.View(); }
    Base::StringView Name() const noexcept { return name_.View(); }
    Base::Span<const TypeId> Interfaces() const noexcept { return {interfaces_.Data(), interfaces_.Size()}; }
    Base::Span<const PropertyInfo> Properties() const noexcept { return {properties_.Data(), properties_.Size()}; }
    Base::Span<const FieldInfo> Fields() const noexcept { return {fields_.Data(), fields_.Size()}; }
    Base::Span<const EnumValueInfo> EnumValues() const noexcept { return {enumValues_.Data(), enumValues_.Size()}; }
    Base::Span<const EventInfo> Events() const noexcept { return {events_.Data(), events_.Size()}; }
    Base::Span<const MethodInfo> Methods() const noexcept { return {methods_.Data(), methods_.Size()}; }
    MemberId ContentMember() const noexcept { return contentMember_; }
private:
    friend class TypeRegistry;
    TypeInfo() noexcept = default;
    TypeId id_ = InvalidTypeId;
    TypeId baseType_ = InvalidTypeId;
    TypeId underlyingType_ = InvalidTypeId;
    MetadataTypeKind kind_ = MetadataTypeKind::Object;
    TypeFlags flags_ = TypeFlags::None;
    Base::String xamlNamespace_;
    Base::String name_;
    Base::Vector<TypeId> interfaces_;
    Base::Vector<PropertyInfo> properties_;
    Base::Vector<FieldInfo> fields_;
    Base::Vector<EnumValueInfo> enumValues_;
    Base::Vector<EventInfo> events_;
    Base::Vector<MethodInfo> methods_;
    MemberId contentMember_ = InvalidMemberId;
};

class TypeRegistry {
public:
    TypeRegistry() noexcept;
    ~TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;
    TypeRegistry(TypeRegistry&&) = delete;
    TypeRegistry& operator=(TypeRegistry&&) = delete;
    Base::Result<void> Freeze() noexcept;
    Base::Result<Base::HashCode> ComputeHash() const noexcept;
    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t TypeCount() const noexcept { return types_.Size(); }
    std::uint32_t PropertyCount() const noexcept {
        std::uint32_t count = 0U;
        for (const TypeInfo& type : types_) {
            count += type.Properties().Size();
        }
        return count;
    }
    std::uint32_t FieldCount() const noexcept {
        std::uint32_t count = 0U;
        for (const TypeInfo& type : types_) {
            count += type.Fields().Size();
        }
        return count;
    }
    std::uint32_t EnumValueCount() const noexcept {
        std::uint32_t count = 0U;
        for (const TypeInfo& type : types_) {
            count += type.EnumValues().Size();
        }
        return count;
    }
    std::uint32_t EventCount() const noexcept {
        std::uint32_t count = 0U;
        for (const TypeInfo& type : types_) {
            count += type.Events().Size();
        }
        return count;
    }
    std::uint32_t MethodCount() const noexcept {
        std::uint32_t count = 0U;
        for (const TypeInfo& type : types_) {
            count += type.Methods().Size();
        }
        return count;
    }
    Base::Span<const TypeInfo> Types() const noexcept { return {types_.Data(), types_.Size()}; }
    const TypeInfo* FindType(TypeId id) const noexcept;
    const TypeInfo* FindType(Base::StringView xamlNamespace, Base::StringView name) const noexcept;
    const PropertyInfo* FindProperty(MemberId id) const noexcept;
    const PropertyInfo* FindProperty(TypeId ownerType, Base::StringView name, bool includeBaseTypes = true) const noexcept;
    const FieldInfo* FindField(MemberId id) const noexcept;
    const FieldInfo* FindField(TypeId ownerType, Base::StringView name) const noexcept;
    const EnumValueInfo* FindEnumValue(MemberId id) const noexcept;
    const EnumValueInfo* FindEnumValue(TypeId ownerType, Base::StringView name) const noexcept;
    const EnumValueInfo* FindEnumValue(TypeId ownerType, std::uint64_t rawValue) const noexcept;
    bool IsEnumValue(TypeId type, std::uint64_t rawValue) const noexcept;
    const EventInfo* FindEvent(MemberId id) const noexcept;
    const EventInfo* FindEvent(TypeId ownerType, Base::StringView name, bool includeBaseTypes = true) const noexcept;
    const MethodInfo* FindMethod(MemberId id) const noexcept;
    const MethodInfo* FindMethod(TypeId ownerType, Base::StringView name, Base::Span<const TypeId> parameterTypes, bool includeBaseTypes = true) const noexcept;
    MemberId FindContentMember(TypeId type) const noexcept;
    bool IsDerivedFrom(TypeId type, TypeId expectedBase) const noexcept;
    bool Implements(TypeId type, TypeId interfaceType) const noexcept;
    bool IsAssignableFrom(TypeId targetType, TypeId sourceType) const noexcept;
private:
    friend class ::Aero::Meta::Registry;
    friend class Detail::RegistrationTypes;
    Base::Result<TypeId> RegisterType(BehaviorTable& behaviors, const TypeRegistration& registration) noexcept;
    Base::Result<void> RegisterInterface(TypeId ownerType, TypeId interfaceType) noexcept;
    Base::Result<MemberId> RegisterProperty(BehaviorTable& behaviors, TypeId ownerType, const PropertyRegistration& registration) noexcept;
    Base::Result<MemberId> RegisterField(BehaviorTable& behaviors, TypeId ownerType, const FieldRegistration& registration) noexcept;
    Base::Result<MemberId> RegisterEnumValue(TypeId ownerType, const EnumValueRegistration& registration) noexcept;
    Base::Result<MemberId> RegisterEvent(TypeId ownerType, const EventRegistration& registration) noexcept;
    Base::Result<MemberId> RegisterMethod(BehaviorTable& behaviors, TypeId ownerType, const MethodRegistration& registration) noexcept;
    Base::Result<void> SetFactory(BehaviorTable& behaviors, TypeId type, ObjectFactory factory) noexcept;
    Base::Result<void> SetContentMember(TypeId type, MemberId member) noexcept;
    struct MemberLocation { std::uint32_t typeIndex = 0U; std::uint32_t memberIndex = 0U; MemberKind kind = MemberKind::Property; };
    Base::Vector<TypeInfo> types_;
    Base::HashMap<TypeId, std::uint32_t> typeIndex_;
    Base::HashMap<MemberId, MemberLocation> memberIndex_;
    bool frozen_ = false;
    TypeInfo* MutableType(TypeId id) noexcept;
    const TypeInfo* TypeAt(std::uint32_t index) const noexcept;
    const PropertyInfo* PropertyAt(const MemberLocation& location) const noexcept;
    const FieldInfo* FieldAt(const MemberLocation& location) const noexcept;
    const EnumValueInfo* EnumValueAt(const MemberLocation& location) const noexcept;
    const EventInfo* EventAt(const MemberLocation& location) const noexcept;
    const MethodInfo* MethodAt(const MemberLocation& location) const noexcept;
};

} // namespace Aero::Meta



namespace Aero::Meta { class Registration; }

namespace Aero::Meta {

} // namespace Aero::Meta

namespace Aero::Meta::Detail {
class MetaTable;
class MetadataPrivate;
} // namespace Aero::Meta::Detail

namespace Aero::Meta {


using MetadataPropertyProviderGetCallback = Base::Result<Value> (*)(
    const Base::Object& object,
    const PropertyInfo& property,
    void* context) noexcept;
using MetadataPropertyProviderSetCallback = Base::Result<void> (*)(
    Base::Object& object,
    const PropertyInfo& property,
    const Value& value,
    void* context) noexcept;

struct MetadataPropertyProviderRegistration {
    PropertyProviderId id = InvalidPropertyProviderId;
    TypeId objectType = InvalidTypeId;
    MetadataPropertyProviderGetCallback get = nullptr;
    MetadataPropertyProviderSetCallback set = nullptr;
    void* context = nullptr;
};

struct ContentInfo {
    TypeId ownerType = InvalidTypeId;
    MemberId member = InvalidMemberId;
    ContentKind kind = ContentKind::Single;
    ContentFlags flags = ContentFlags::None;
    bool writable = false;
    bool clearable = false;

    bool IsValid() const noexcept {
        return ownerType != InvalidTypeId && member != InvalidMemberId;
    }
    bool IsVisual() const noexcept {
        return HasContentFlag(flags, ContentFlags::Visual);
    }
};

using MetadataModuleId = std::uint64_t;
inline constexpr MetadataModuleId InvalidMetadataModuleId = 0U;

constexpr MetadataModuleId MakeMetadataModuleId(
    Base::StringView name) noexcept {
    constexpr char domain[] = "AERO.METADATA.MODULE.V1";
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddString(name);
    return builder.Finish();
}

using MetadataModuleRegisterCallback = Base::Result<void> (*)(
    Meta::Registration& context) noexcept;
using MetadataModuleRegisterContextCallback = Base::Result<void> (*)(
    Meta::Registration& context,
    void* userContext) noexcept;

struct MetadataModuleRegistration {
    MetadataModuleId id = InvalidMetadataModuleId;
    Base::StringView name;
    std::uint32_t schemaVersion = 1U;
    MetadataModuleRegisterCallback registerModule = nullptr;
    MetadataModuleRegisterContextCallback registerModuleWithContext = nullptr;
    void* context = nullptr;
};

} // namespace Aero::Meta

namespace Aero::Meta {

using Meta::ContentInfo;
using Meta::DependencyPropertyRegistry;
using Meta::MemberId;
using Meta::MetadataCollectionChangedCallback;
using Meta::MetadataModuleRegistration;
using Meta::MetadataPropertyChangedCallback;
using Meta::MetadataPropertyProviderRegistration;
using Meta::PropertyFlags;
using Meta::PropertyInfo;
using Meta::PropertyProviderId;
using Meta::TypeId;
using Meta::TypeInfo;
using Meta::TypeRegistry;
using Meta::Value;

// Registry has two explicit phases:
//
// 1. Registration phase: deterministic module callbacks populate mutable
//    TypeRegistry, dependency/routed registries, and registration value services.
// 2. Runtime phase: Seal() freezes the structural registry and materializes
//    internal executable runtime tables.
//
// Runtime structural lookup uses Types(). Registry references and
// registration-value views obtained before the next module
// transaction are provisional because a successful transaction replaces the
// complete candidate storage.
class Registry {
public:
    Registry() noexcept;
    ~Registry() noexcept;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = delete;
    Registry& operator=(Registry&&) = delete;

    bool IsValid() const noexcept;
    bool IsSealed() const noexcept;
    std::uint32_t ModuleCount() const noexcept;

    Base::Result<void> RegisterModule(
        const MetadataModuleRegistration& registration) noexcept;
    Base::Result<void> Seal() noexcept;

    // Property providers are registered after the structural type graph is
    // sealed. Complete() finalizes executable metadata on this same object.
    Base::Result<void> RegisterPropertyProvider(
        const MetadataPropertyProviderRegistration& registration) noexcept;
    Base::Result<void> Complete() noexcept;
    bool IsReady() const noexcept;

    bool CanReadProperty(MemberId member) const noexcept;
    bool CanWriteProperty(MemberId member) const noexcept;
    bool CanReadValueMember(MemberId member) const noexcept;
    bool CanWriteValueMember(MemberId member) const noexcept;
    MemberId FindContentMember(TypeId type) const noexcept;
    Base::Result<ContentInfo> GetContentInfo(MemberId member) const noexcept;
    Base::Result<void> WriteContent(
        Base::Object& owner,
        MemberId member,
        const Base::Ref<Base::Object>& value) const noexcept;
    Base::Result<void> ClearContent(
        Base::Object& owner,
        MemberId member) const noexcept;
    Base::Result<std::uint64_t> SubscribePropertyChanged(
        Base::Object& object,
        MetadataPropertyChangedCallback callback,
        void* callbackContext = nullptr) const noexcept;
    Base::Result<bool> UnsubscribePropertyChanged(
        Base::Object& object,
        std::uint64_t subscription) const noexcept;
    Base::Result<Base::Ref<Base::Object>> CreateObject(
        TypeId type) const noexcept;
    Base::Result<Value> TryCreateValue(
        TypeId type,
        const void* source) const noexcept;
    Base::Result<Value> TryConvertText(
        TypeId type,
        Base::StringView text) const noexcept;
    Base::Result<Value> GetValueMember(
        const Value& owner,
        MemberId member) const noexcept;
    Base::Result<void> SetValueMember(
        Value& owner,
        MemberId member,
        const Value& value) const noexcept;
    Base::Result<Value> GetProperty(
        const Base::Object& object,
        MemberId member) const noexcept;
    Base::Result<void> SetProperty(
        Base::Object& object,
        MemberId member,
        const Value& value) const noexcept;
    Base::Result<Value> InvokeMethod(
        Base::Object& object,
        MemberId member,
        Base::Span<const Value> arguments) const noexcept;

    // Structural registration data is exposed read-only. Mutable registration
    // is confined to module callbacks and their Registration.
    const TypeRegistry& Types() const noexcept;
    const DependencyPropertyRegistry& DependencyProperties() const noexcept;
    Base::Result<Base::HashCode> ComputeSchemaHash() const noexcept;

private:
    friend class ::Aero::MetadataPrivate;

    struct Storage;
    Storage* storage_ = nullptr;

    DependencyPropertyRegistry& DependencyProperties() noexcept;
    void* RoutedEventState() noexcept;
    const ::Aero::MetaTable& RuntimeData() const noexcept;

    static Base::Status OutOfMemoryStatus() noexcept;
    static bool HasPropertyFlag(
        PropertyFlags value,
        PropertyFlags flag) noexcept;
    static Base::Status MetadataNotReady() noexcept;
    static Base::Status UnsupportedProperty() noexcept;
    bool IsRegisteredEnumValue(
        TypeId type,
        const Value& value) const noexcept;
    Base::Result<Value> ConvertEnumText(
        const TypeInfo& type,
        Base::StringView input) const noexcept;
    Base::Result<void> ValidatePropertyTarget(
        const Base::Object& object,
        const PropertyInfo& property) const noexcept;
    Base::Result<Value> GetDependencyProperty(
        const Base::Object& object,
        const PropertyInfo& property) const noexcept;
    Base::Result<void> SetDependencyProperty(
        Base::Object& object,
        const PropertyInfo& property,
        const Value& value) const noexcept;
    const MetadataPropertyProviderRegistration* FindProvider(
        PropertyProviderId id) const noexcept;

    static Base::Result<void> ValidateRegistration(
        const MetadataModuleRegistration& registration) noexcept;
    Base::Result<Storage*> BuildCandidate(
        const MetadataModuleRegistration* extra,
        bool seal) const noexcept;
};


} // namespace Aero::Meta


// Consolidated private metadata contract.
// Built-in identifiers, metadata context glue and routed-event catalog.

#include <Aero/Base/StringView.hpp>


namespace Aero::Meta::BuiltinTypes {

inline constexpr TypeId Object = MakeTypeId(Base::StringView("Object"));
inline constexpr TypeId DependencyObject =
    MakeTypeId(Base::StringView("DependencyObject"));
inline constexpr TypeId Visual = MakeTypeId(Base::StringView("Visual"));
inline constexpr TypeId UIElement =
    MakeTypeId(Base::StringView("UIElement"));
inline constexpr TypeId FrameworkElement =
    MakeTypeId(Base::StringView("FrameworkElement"));

inline constexpr TypeId Panel = MakeTypeId(Base::StringView("Panel"));
inline constexpr TypeId Decorator = MakeTypeId(Base::StringView("Decorator"));
inline constexpr TypeId Control = MakeTypeId(Base::StringView("Control"));
inline constexpr TypeId ContentControl =
    MakeTypeId(Base::StringView("ContentControl"));
inline constexpr TypeId UserControl =
    MakeTypeId(Base::StringView("UserControl"));
inline constexpr TypeId ButtonBase =
    MakeTypeId(Base::StringView("ButtonBase"));
inline constexpr TypeId Button =
    MakeTypeId(Base::StringView("Button"));
inline constexpr TypeId RepeatButton =
    MakeTypeId(Base::StringView("RepeatButton"));
inline constexpr TypeId ToggleButton =
    MakeTypeId(Base::StringView("ToggleButton"));
inline constexpr TypeId CheckBox =
    MakeTypeId(Base::StringView("CheckBox"));
inline constexpr TypeId RadioButton =
    MakeTypeId(Base::StringView("RadioButton"));
inline constexpr TypeId ScrollContentPresenter =
    MakeTypeId(Base::StringView("ScrollContentPresenter"));
inline constexpr TypeId ScrollViewer =
    MakeTypeId(Base::StringView("ScrollViewer"));
inline constexpr TypeId ScrollBar =
    MakeTypeId(Base::StringView("ScrollBar"));
inline constexpr TypeId Track =
    MakeTypeId(Base::StringView("Track"));
inline constexpr TypeId Thumb =
    MakeTypeId(Base::StringView("Thumb"));
inline constexpr TypeId ItemsControl =
    MakeTypeId(Base::StringView("ItemsControl"));
inline constexpr TypeId ItemsPresenter =
    MakeTypeId(Base::StringView("ItemsPresenter"));
inline constexpr TypeId Selector =
    MakeTypeId(Base::StringView("Selector"));
inline constexpr TypeId ListBox =
    MakeTypeId(Base::StringView("ListBox"));
inline constexpr TypeId ListBoxItem =
    MakeTypeId(Base::StringView("ListBoxItem"));
inline constexpr TypeId VirtualizingStackPanel =
    MakeTypeId(Base::StringView("VirtualizingStackPanel"));

inline constexpr TypeId StackPanel = MakeTypeId(Base::StringView("StackPanel"));
inline constexpr TypeId Canvas = MakeTypeId(Base::StringView("Canvas"));
inline constexpr TypeId Grid = MakeTypeId(Base::StringView("Grid"));
inline constexpr TypeId Border = MakeTypeId(Base::StringView("Border"));
inline constexpr TypeId TextBlock = MakeTypeId(Base::StringView("TextBlock"));
inline constexpr TypeId ContentPresenter =
    MakeTypeId(Base::StringView("ContentPresenter"));

inline constexpr TypeId Boolean = MakeTypeId(Base::StringView("Boolean"));
inline constexpr TypeId UnsignedInteger = MakeTypeId(Base::StringView("UInt32"));
inline constexpr TypeId Double = MakeTypeId(Base::StringView("Double"));
inline constexpr TypeId String = MakeTypeId(Base::StringView("String"));
inline constexpr TypeId Length = MakeTypeId(Base::StringView("Length"));
inline constexpr TypeId Thickness = MakeTypeId(Base::StringView("Thickness"));
inline constexpr TypeId Color = MakeTypeId(Base::StringView("Color"));
inline constexpr TypeId HorizontalAlignment =
    MakeTypeId(Base::StringView("HorizontalAlignment"));
inline constexpr TypeId VerticalAlignment =
    MakeTypeId(Base::StringView("VerticalAlignment"));
inline constexpr TypeId Orientation = MakeTypeId(Base::StringView("Orientation"));

inline constexpr TypeId EventArgs = MakeTypeId(Base::StringView("EventArgs"));
inline constexpr TypeId RoutedEventArgs =
    MakeTypeId(Base::StringView("RoutedEventArgs"));
inline constexpr TypeId InputEventArgs =
    MakeTypeId(Base::StringView("InputEventArgs"));
inline constexpr TypeId MouseEventArgs =
    MakeTypeId(Base::StringView("MouseEventArgs"));
inline constexpr TypeId MouseButtonEventArgs =
    MakeTypeId(Base::StringView("MouseButtonEventArgs"));
inline constexpr TypeId MouseWheelEventArgs =
    MakeTypeId(Base::StringView("MouseWheelEventArgs"));
inline constexpr TypeId KeyEventArgs =
    MakeTypeId(Base::StringView("KeyEventArgs"));
inline constexpr TypeId TextCompositionEventArgs =
    MakeTypeId(Base::StringView("TextCompositionEventArgs"));
inline constexpr TypeId KeyboardFocusChangedEventArgs =
    MakeTypeId(Base::StringView("KeyboardFocusChangedEventArgs"));
inline constexpr TypeId ScrollChangedEventArgs =
    MakeTypeId(Base::StringView("ScrollChangedEventArgs"));

} // namespace Aero::Meta::BuiltinTypes



namespace Aero::Meta {

} // namespace Aero::Meta

namespace Aero::Meta::Detail {

// Module population is an implementation callback; hosts register through the
// Meta::Registry overload below.
Base::Result<void> PopulateCoreMetadata(
    Meta::Registration& context) noexcept;

} // namespace Aero::Meta::Detail

namespace Aero::Meta {

inline constexpr Base::StringView CoreMetadataModuleName() noexcept {
    return "Aero.Core";
}

inline Base::Result<void> RegisterCoreMetadata(
    Meta::Registry& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 1U;
    const Base::StringView name = CoreMetadataModuleName();
    return domain.RegisterModule({
        MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::PopulateCoreMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero::Meta

#include <Aero/Base/Result.hpp>

namespace Aero {

using namespace ::Aero::Meta;

Base::Result<void> PopulateEnumMetadata(
    ::Aero::Meta::Registration& context) noexcept;

// Registers the complete built-in UI schema through the typed
// Fluent metadata DSL. The function leaves all stores mutable for host modules.
// Module population is an implementation callback; hosts register through the
// Meta::Registry overload below.
Base::Result<void> PopulateUiMetadata(
    ::Aero::Meta::Registration& context) noexcept;

inline constexpr Base::StringView UiMetadataModuleName() noexcept {
    return "Aero.UI";
}

inline Base::Result<void> RegisterUiMetadata(
    ::Aero::Meta::Registry& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 12U;
    const Base::StringView name = UiMetadataModuleName();
    return domain.RegisterModule({
        ::Aero::Meta::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &PopulateUiMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero


namespace Aero {

class MetadataPrivate {
public:
    static ::Aero::Meta::DependencyPropertyRegistry& DependencyProperties(
        ::Aero::Meta::Registry& domain) noexcept {
        return domain.DependencyProperties();
    }

    static void* RoutedEventState(
        ::Aero::Meta::Registry& domain) noexcept {
        return domain.RoutedEventState();
    }
};

} // namespace Aero

#include <Aero/Base/Config.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
// ===== Behavior facets =====
#include <Aero/Base/Allocator.hpp>


#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Meta {

} // namespace Aero::Meta

namespace Aero::Meta::Detail {
class MetaTable;
class MetadataAuthoringSession;
} // namespace Aero::Meta::Detail

namespace Aero::Meta {
template<class T>
class TypeBuilder;

// Mutable registration storage for executable type/member behavior.
//
// TypeRegistry owns callback-free structural metadata only. Registration code
// enters through RegistrationTypes, which commits structural records to
// TypeRegistry and executable records to this store as one registration step.
class BehaviorTable {
public:
    explicit BehaviorTable(TypeRegistry& types) noexcept
        : types_(&types) {}
    ~BehaviorTable() noexcept;

    BehaviorTable(
        const BehaviorTable&) = delete;
    BehaviorTable& operator=(
        const BehaviorTable&) = delete;
    BehaviorTable(
        BehaviorTable&&) = delete;
    BehaviorTable& operator=(
        BehaviorTable&&) = delete;

    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    const TypeRegistry& Types() const noexcept { return *types_; }

private:
    friend class ::Aero::Meta::Registry;
    friend class ::Aero::MetaTable;
    friend class Detail::MetaTable;
    friend class Detail::MetadataAuthoringSession;
    friend class Detail::RegistrationTypes;
    friend class TypeRegistry;

    struct OwnedBehaviorData {
        Base::IAllocator* allocator = nullptr;
        void* value = nullptr;
        void (*destroy)(OwnedBehaviorData&) noexcept = nullptr;
        void (*destroyValue)(void*) noexcept = nullptr;
        std::size_t size = 0U;
        std::size_t alignment = 0U;
    };

    template<class TContext>
    Base::Result<std::decay_t<TContext>*> OwnContext(
        TContext&& value) noexcept {
        using Stored = std::decay_t<TContext>;
        Stored temporary(
            std::forward<TContext>(value));
        Base::Result<void*> stored = OwnContextRaw(
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
    Base::Result<void*> OwnContextRaw(
        std::size_t size,
        std::size_t alignment,
        void* source,
        void (*construct)(void*, void*) noexcept,
        void (*destroyValue)(void*) noexcept) noexcept;
    void ReleaseLastContext(void* value) noexcept;
    Base::Result<void> AdoptOwnedContextsFrom(
        BehaviorTable& source) noexcept;

    const TypeFactoryRegistration* FindTypeFactory(
        TypeId type) const noexcept;
    const ContentAccessorRegistration* FindContentAccessor(
        MemberId member) const noexcept;
    const PropertyAccessorRegistration* FindPropertyAccessor(
        MemberId member) const noexcept;
    const ValueMemberAccessorRegistration* FindValueMemberAccessor(
        MemberId member) const noexcept;
    const MethodInvokerRegistration* FindMethodInvoker(
        MemberId member) const noexcept;
    const PropertyChangeNotificationRegistration*
    FindPropertyChangeNotification(TypeId type) const noexcept;
    const CollectionChangeNotificationRegistration*
    FindCollectionChangeNotification(TypeId type) const noexcept;

    TypeRegistry* types_ = nullptr;
    Base::Vector<TypeFactoryRegistration> typeFactories_;
    Base::Vector<ContentAccessorRegistration> contentAccessors_;
    Base::Vector<PropertyAccessorRegistration> propertyAccessors_;
    Base::Vector<ValueMemberAccessorRegistration> valueMemberAccessors_;
    Base::Vector<MethodInvokerRegistration> methodInvokers_;
    Base::Vector<PropertyChangeNotificationRegistration>
        propertyChangeNotifications_;
    Base::Vector<CollectionChangeNotificationRegistration>
        collectionChangeNotifications_;
    Base::Vector<OwnedBehaviorData> ownedContexts_;
    bool frozen_ = false;
};

// Explicit mutable view for structural and executable type registration.
// Read-only consumers continue to use TypeRegistry directly.
namespace Detail {

class RegistrationTypes {
public:
    RegistrationTypes(
        TypeRegistry& types,
        BehaviorTable& behaviors) noexcept
        : types_(&types), behaviors_(&behaviors) {}

    Base::Result<TypeId> RegisterType(
        const TypeRegistration& registration) const noexcept;
    Base::Result<void> RegisterInterface(
        TypeId ownerType,
        TypeId interfaceType) const noexcept;
    Base::Result<MemberId> RegisterProperty(
        TypeId ownerType,
        const PropertyRegistration& registration) const noexcept;
    Base::Result<MemberId> RegisterField(
        TypeId ownerType,
        const FieldRegistration& registration) const noexcept;
    Base::Result<MemberId> RegisterEnumValue(
        TypeId ownerType,
        const EnumValueRegistration& registration) const noexcept;
    Base::Result<MemberId> RegisterEvent(
        TypeId ownerType,
        const EventRegistration& registration) const noexcept;
    Base::Result<MemberId> RegisterMethod(
        TypeId ownerType,
        const MethodRegistration& registration) const noexcept;
    Base::Result<void> SetFactory(
        TypeId type,
        ObjectFactory factory) const noexcept;
    Base::Result<void> SetContentMember(
        TypeId type,
        MemberId member) const noexcept;
    Base::Result<void> SetContentAccessor(
        const ContentAccessorRegistration& registration) const noexcept;
    Base::Result<void> RegisterPropertyChangeNotification(
        const PropertyChangeNotificationRegistration& registration)
        const noexcept;
    Base::Result<void> RegisterCollectionChangeNotification(
        const CollectionChangeNotificationRegistration& registration)
        const noexcept;

    TypeRegistry& Registry() const noexcept { return *types_; }
    BehaviorTable& Behaviors() const noexcept {
        return *behaviors_;
    }

private:
    template<class>
    friend class ::Aero::Meta::TypeBuilder;
    friend class MetadataAuthoringSession;

    template<class TContext>
    Base::Result<std::decay_t<TContext>*> OwnBehaviorContext(
        TContext&& value) const noexcept {
        return behaviors_->OwnContext(
            std::forward<TContext>(value));
    }
    void ReleaseLastBehaviorContext(void* value) const noexcept {
        behaviors_->ReleaseLastContext(value);
    }

    Base::Result<void> ValidateRegistrationPair() const noexcept;

    TypeRegistry* types_ = nullptr;
    BehaviorTable* behaviors_ = nullptr;
};

} // namespace Detail

} // namespace Aero::Meta

// ===== Value facets =====
#include <Aero/Base/Ref.hpp>


namespace Aero::Meta {

} // namespace Aero::Meta

namespace Aero::Meta::Detail {
class MetaTable;
} // namespace Aero::Meta::Detail

namespace Aero::Meta {

// Mutable registration storage for custom value semantics and text converters.
//
// The store is owned beside TypeRegistry by Meta::Registry. It validates type
// identities through the structural registry, but does not make executable
// value behavior part of TypeRegistry's ownership or public API.
class ValueTable {
public:
    explicit ValueTable(TypeRegistry& types) noexcept
        : types_(&types) {}

    ValueTable(const ValueTable&) = delete;
    ValueTable& operator=(
        const ValueTable&) = delete;
    ValueTable(ValueTable&&) = delete;
    ValueTable& operator=(
        ValueTable&&) = delete;

    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    const TypeRegistry& Types() const noexcept { return *types_; }

private:
    friend class ::Aero::Meta::Registry;
    friend class ::Aero::MetaTable;
    friend class Detail::MetaTable;
    friend class Detail::RegistrationValues;

    struct ValueSemanticsEntry {
        TypeId type = InvalidTypeId;
        Base::Ref<ValueTypeSemantics> semantics;
    };

    Base::Result<void> RegisterValueSemantics(
        TypeId type,
        const ValueTypeRegistration& registration) noexcept;
    Base::Result<void> RegisterTextConverter(
        const TextValueConverterRegistration& registration) noexcept;
    const Base::Ref<ValueTypeSemantics>* FindValueSemantics(
        TypeId type) const noexcept;
    const TextValueConverterRegistration* FindTextConverter(
        TypeId type) const noexcept;

    TypeRegistry* types_ = nullptr;
    Base::Vector<ValueSemanticsEntry> valueSemantics_;
    Base::Vector<TextValueConverterRegistration> textConverters_;
    bool frozen_ = false;
};

} // namespace Aero::Meta

#include <Aero/RoutedEvent.hpp>

namespace Aero::Meta {

struct RoutedEventRegistration {
    Base::StringView name;
    TypeId ownerType = InvalidTypeId;
    TypeId eventArgsType = InvalidTypeId;
    RoutingStrategy strategy = RoutingStrategy::Bubble;
};

class RoutedEventTable {
public:
    struct Definition {
        RoutedEventHandle handle;
        TypeId ownerType = InvalidTypeId;
        TypeId eventArgsType = InvalidTypeId;
        RoutingStrategy strategy = RoutingStrategy::Bubble;
        Base::String name;

        Definition() noexcept : name() {}
    };

    RoutedEventTable(
        TypeRegistry& types,
        BehaviorTable& behaviors) noexcept;

    RoutedEventTable(const RoutedEventTable&) = delete;
    RoutedEventTable& operator=(const RoutedEventTable&) = delete;

    Base::Result<RoutedEventHandle> Register(
        const RoutedEventRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    const TypeRegistry& Types() const noexcept { return *types_; }
    const Definition* Find(RoutedEventHandle event) const noexcept;

private:
    friend class ::Aero::Meta::Registry;
    TypeRegistry* types_ = nullptr;
    BehaviorTable* behaviorRegistrations_ = nullptr;
    Base::Vector<Definition> definitions_;
    bool frozen_ = false;
};

} // namespace Aero::Meta


#include <Aero/DependencyProperty.hpp>

namespace Aero {

using namespace ::Aero::Meta;

struct RegistrationState {
    TypeRegistry* types = nullptr;
    BehaviorTable* behaviors = nullptr;
    ValueTable* values = nullptr;
    DependencyPropertyRegistry* properties = nullptr;
    RoutedEventTable* events = nullptr;
};

} // namespace Aero

// Private helpers for sealing value behavior into MetadataFacets.

#include <Aero/Base/Hash.hpp>
// ===== Compact facet index =====
// Private executable metadata storage. TypeRegistry is the public structural
// source of truth; these records never cross the Gui-kernel boundary.

#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Span.hpp>



#include <cstdint>

namespace Aero::Meta {
class BehaviorTable;
class ValueTable;
class DependencyProperty;
class DependencyPropertyRegistry;
}

namespace Aero {

using namespace ::Aero::Meta;

// Shared compact-facet positioning kernel. Meta and Markup own independent
// columns and seal independently; only the mask/rank calculation is shared.
struct CompactFacetIndex {
    template<class TMask, class TKind>
    static constexpr std::uint16_t CountBefore(
        TMask mask,
        TKind kind) noexcept {
        std::uint16_t count = 0U;
        const std::uint8_t end = static_cast<std::uint8_t>(kind);
        for (std::uint8_t index = 0U; index < end; ++index) {
            const TMask bit = static_cast<TMask>(
                static_cast<TMask>(1U) << index);
            if ((mask & bit) != 0U) ++count;
        }
        return count;
    }
};

inline constexpr std::uint32_t MetadataFacetFormatVersion =
    MetadataProgramFormatVersion;

enum class MetadataFacetKind : std::uint8_t {
    TypeFactory = 0U,
    Content,
    PropertyAccessor,
    ValueMemberAccessor,
    MethodInvoker,
    DependencyProperty,
    RoutedEvent,
    ValueSemantics,
    TextConverter,
    PropertyChangeNotification,
    CollectionChangeNotification
};

using MetadataFacetMask = std::uint64_t;

constexpr MetadataFacetMask MetadataFacetBit(
    MetadataFacetKind kind) noexcept {
    return UINT64_C(1) << static_cast<std::uint8_t>(kind);
}

constexpr bool HasMetadataFacet(
    MetadataFacetMask mask,
    MetadataFacetKind kind) noexcept {
    return (mask & MetadataFacetBit(kind)) != 0U;
}

struct TypeFactoryFacet {
    TypeId type = InvalidTypeId;
    ObjectFactory factory = nullptr;
};

struct ContentFacet {
    TypeId type = InvalidTypeId;
    MemberId member = InvalidMemberId;
    ContentKind kind = ContentKind::Single;
    ContentFlags flags = ContentFlags::None;
    ContentWriteCallback write = nullptr;
    ContentClearCallback clear = nullptr;
    void* context = nullptr;
};

struct PropertyAccessorFacet {
    MemberId member = InvalidMemberId;
    PropertyAccessKind access = PropertyAccessKind::External;
    PropertyGetCallback get = nullptr;
    PropertySetCallback set = nullptr;
    PropertyProviderId provider = InvalidPropertyProviderId;
    void* context = nullptr;
};

struct ValueMemberAccessorFacet {
    MemberId member = InvalidMemberId;
    ValueMemberGetCallback get = nullptr;
    ValueMemberSetCallback set = nullptr;
    void* context = nullptr;
};

struct MethodInvokerFacet {
    MemberId member = InvalidMemberId;
    MethodInvokeCallback invoke = nullptr;
    void* context = nullptr;
};

struct DependencyPropertyFacet {
    MemberId member = InvalidMemberId;
    MemberId canonicalMember = InvalidMemberId;
    TypeId registeredOwnerType = InvalidTypeId;
    TypeId valueType = InvalidTypeId;
    std::uint32_t flags = 0U;
    std::uint32_t metadataCount = 0U;
    const DependencyProperty* property = nullptr;
};

struct RoutedEventFacet {
    MemberId member = InvalidMemberId;
    TypeId ownerType = InvalidTypeId;
    TypeId eventArgsType = InvalidTypeId;
    RoutingStrategy strategy = RoutingStrategy::Bubble;
};

struct PropertyChangeNotificationFacet {
    TypeId type = InvalidTypeId;
    PropertyChangeSubscribeCallback subscribe = nullptr;
    PropertyChangeUnsubscribeCallback unsubscribe = nullptr;
    void* context = nullptr;
};

struct CollectionChangeNotificationFacet {
    TypeId type = InvalidTypeId;
    CollectionChangeSubscribeCallback subscribe = nullptr;
    CollectionChangeUnsubscribeCallback unsubscribe = nullptr;
    void* context = nullptr;
};

// Sealed value facets own their runtime registrations. No runtime lookup is
// routed back through TypeRegistry after Meta::Registry::Seal().
struct ValueSemanticsFacet {
    TypeId type = InvalidTypeId;
    Base::Ref<ValueTypeSemantics> semantics;
};

struct TextConverterFacet {
    TypeId type = InvalidTypeId;
    TextValueConverterCallback convert = nullptr;
    void* context = nullptr;
};

class MetaTable {
public:
    MetaTable() noexcept = default;

    MetaTable(const MetaTable&) = delete;
    MetaTable& operator=(const MetaTable&) = delete;
    MetaTable(MetaTable&&) = delete;
    MetaTable& operator=(MetaTable&&) = delete;

    Base::Result<void> Build(
        const TypeRegistry& types,
        const BehaviorTable& behaviors,
        const DependencyPropertyRegistry& dependencyProperties,
        const RoutedEventTable& routedEvents) noexcept;
    Base::Result<void> BuildValueFacets(
        const ValueTable& source,
        const TypeRegistry& types) noexcept;

    bool IsSealed() const noexcept { return sealed_; }
    bool ValueFacetsSealed() const noexcept { return valueFacetsSealed_; }
    MetadataFacetMask TypeFacets(TypeId type) const noexcept;
    MetadataFacetMask MemberFacets(MemberId member) const noexcept;
    bool HasTypeFacet(TypeId type, MetadataFacetKind kind) const noexcept {
        return HasMetadataFacet(TypeFacets(type), kind);
    }
    bool HasMemberFacet(MemberId member, MetadataFacetKind kind) const noexcept {
        return HasMetadataFacet(MemberFacets(member), kind);
    }

    const TypeFactoryFacet* FindTypeFactory(TypeId type) const noexcept;
    const ContentFacet* FindContent(TypeId type) const noexcept;
    const ContentFacet* FindContentByMember(MemberId member) const noexcept;
    MemberId FindContentMember(TypeId type) const noexcept;
    const PropertyAccessorFacet* FindPropertyAccessor(MemberId member) const noexcept;
    const ValueMemberAccessorFacet* FindValueMemberAccessor(MemberId member) const noexcept;
    const MethodInvokerFacet* FindMethodInvoker(MemberId member) const noexcept;
    const DependencyPropertyFacet* FindDependencyProperty(MemberId member) const noexcept;
    const RoutedEventFacet* FindRoutedEvent(MemberId member) const noexcept;
    const PropertyChangeNotificationFacet*
    FindPropertyChangeNotification(TypeId type) const noexcept;
    const CollectionChangeNotificationFacet*
    FindCollectionChangeNotification(TypeId type) const noexcept;
    const ValueSemanticsFacet* FindValueSemantics(TypeId type) const noexcept;
    const TextConverterFacet* FindTextConverter(TypeId type) const noexcept;

    Base::Result<Base::HashCode> ComputeHash() const noexcept;

private:
    inline static constexpr std::uint32_t InvalidFacetIndex = UINT32_MAX;

    struct FacetDraft {
        std::uint64_t key = 0U;
        std::uint32_t facets[11] = {
            InvalidFacetIndex, InvalidFacetIndex, InvalidFacetIndex,
            InvalidFacetIndex, InvalidFacetIndex, InvalidFacetIndex,
            InvalidFacetIndex, InvalidFacetIndex, InvalidFacetIndex,
            InvalidFacetIndex, InvalidFacetIndex};
    };

    struct TypeRecord {
        TypeId id = InvalidTypeId;
        std::uint32_t firstFacetRef = 0U;
        MetadataFacetMask mask = 0U;
        std::uint16_t facetCount = 0U;
        std::uint16_t reserved = 0U;
    };

    struct MemberRecord {
        MemberId id = InvalidMemberId;
        std::uint32_t firstFacetRef = 0U;
        MetadataFacetMask mask = 0U;
        std::uint16_t facetCount = 0U;
        std::uint16_t reserved = 0U;
    };

    static_assert(sizeof(TypeRecord) <= 64U,
        "Metadata TypeRecord must remain compact");
    static_assert(sizeof(MemberRecord) <= 48U,
        "Metadata MemberRecord must remain compact");
    static_assert(sizeof(std::uint32_t) == 4U,
        "Metadata facet references must remain 32-bit");

    const TypeRegistry* types_ = nullptr;
    Base::Vector<TypeFactoryFacet> factories_;
    Base::Vector<ContentFacet> contents_;
    Base::Vector<PropertyAccessorFacet> propertyAccessors_;
    Base::Vector<ValueMemberAccessorFacet> valueMemberAccessors_;
    Base::Vector<MethodInvokerFacet> methodInvokers_;
    Base::Vector<DependencyPropertyFacet> dependencyProperties_;
    Base::Vector<RoutedEventFacet> routedEvents_;
    Base::Vector<PropertyChangeNotificationFacet>
        propertyChangeNotifications_;
    Base::Vector<CollectionChangeNotificationFacet>
        collectionChangeNotifications_;
    Base::Vector<ValueSemanticsFacet> valueSemantics_;
    Base::Vector<TextConverterFacet> textConverters_;

    Base::Vector<FacetDraft> typeDrafts_;
    Base::Vector<FacetDraft> memberDrafts_;
    Base::Vector<TypeRecord> typeRecords_;
    Base::Vector<MemberRecord> memberRecords_;
    Base::Vector<std::uint32_t> facetRefs_;
    Base::HashMap<TypeId, std::uint32_t> typeIndex_;
    Base::HashMap<MemberId, std::uint32_t> memberIndex_;
    bool sealed_ = false;
    bool valueFacetsSealed_ = false;

    static std::uint16_t FacetCountBefore(
        MetadataFacetMask mask,
        MetadataFacetKind kind) noexcept;
    Base::Result<void> SetTypeFacet(
        TypeId type,
        MetadataFacetKind kind,
        std::uint32_t index) noexcept;
    Base::Result<void> SetMemberFacet(
        MemberId member,
        MetadataFacetKind kind,
        std::uint32_t index) noexcept;
    Base::Result<void> SealIndex() noexcept;
    std::uint32_t FindTypeFacet(
        TypeId type,
        MetadataFacetKind kind) const noexcept;
    std::uint32_t FindMemberFacet(
        MemberId member,
        MetadataFacetKind kind) const noexcept;
};

} // namespace Aero


namespace Aero {

using namespace ::Aero::Meta;

// Computes the deterministic structural contribution of value-semantics and
// text-converter facets. Callback and context addresses are never included.
Base::Result<Base::HashCode> ComputeMetadataValueFacetHash(
    const MetaTable& facets,
    const TypeRegistry& descriptors) noexcept;

} // namespace Aero
