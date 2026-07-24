#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Events/RoutedEventCatalog.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>

#include <cstdint>

namespace Aero::Core {

class MetadataBehaviorRegistrationStore;
class MetadataValueRegistrationStore;

class DependencyProperty;
class DependencyPropertyRegistry;

inline constexpr std::uint32_t MetadataDescriptorFormatVersion = 2U;
inline constexpr std::uint32_t MetadataFacetFormatVersion = 6U;

enum class MetadataDescriptorKind : std::uint8_t {
    Type = 0U,
    Property,
    Field,
    EnumValue,
    Event,
    Method
};

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
    Activation,
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

class AERO_API MetadataPropertyDescriptor final {
public:
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId ValueType() const noexcept { return valueType_; }
    PropertyFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }

private:
    friend class MetadataDescriptorStore;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId valueType_ = InvalidTypeId;
    PropertyFlags flags_ = PropertyFlags::None;
    Base::String name_;
};

class AERO_API MetadataFieldDescriptor final {
public:
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId ValueType() const noexcept { return valueType_; }
    FieldFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }

private:
    friend class MetadataDescriptorStore;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId valueType_ = InvalidTypeId;
    FieldFlags flags_ = FieldFlags::None;
    Base::String name_;
};

class AERO_API MetadataEnumValueDescriptor final {
public:
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    std::uint64_t RawValue() const noexcept { return rawValue_; }
    Base::StringView Name() const noexcept { return name_.View(); }

private:
    friend class MetadataDescriptorStore;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    std::uint64_t rawValue_ = 0U;
    Base::String name_;
};

class AERO_API MetadataEventDescriptor final {
public:
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId EventArgsType() const noexcept { return eventArgsType_; }
    EventFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }

private:
    friend class MetadataDescriptorStore;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId eventArgsType_ = InvalidTypeId;
    EventFlags flags_ = EventFlags::None;
    Base::String name_;
};

class AERO_API MetadataMethodParameterDescriptor final {
public:
    TypeId Type() const noexcept { return type_; }
    Base::StringView Name() const noexcept { return name_.View(); }

private:
    friend class MetadataDescriptorStore;
    TypeId type_ = InvalidTypeId;
    Base::String name_;
};

class AERO_API MetadataMethodDescriptor final {
public:
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId ReturnType() const noexcept { return returnType_; }
    MethodFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }
    Base::Span<const MetadataMethodParameterDescriptor> Parameters() const noexcept {
        return {parameters_.Data(), parameters_.Size()};
    }

private:
    friend class MetadataDescriptorStore;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId returnType_ = InvalidTypeId;
    MethodFlags flags_ = MethodFlags::None;
    Base::String name_;
    Base::Vector<MetadataMethodParameterDescriptor> parameters_;
};

class AERO_API MetadataTypeDescriptor final {
public:
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
    Base::Span<const TypeId> Interfaces() const noexcept {
        return {interfaces_.Data(), interfaces_.Size()};
    }
    Base::Span<const MemberId> DeclaredProperties() const noexcept {
        return {properties_.Data(), properties_.Size()};
    }
    Base::Span<const MemberId> DeclaredFields() const noexcept {
        return {fields_.Data(), fields_.Size()};
    }
    Base::Span<const MemberId> EnumValues() const noexcept {
        return {enumValues_.Data(), enumValues_.Size()};
    }
    Base::Span<const MemberId> DeclaredEvents() const noexcept {
        return {events_.Data(), events_.Size()};
    }
    Base::Span<const MemberId> DeclaredMethods() const noexcept {
        return {methods_.Data(), methods_.Size()};
    }

private:
    friend class MetadataDescriptorStore;
    TypeId id_ = InvalidTypeId;
    TypeId baseType_ = InvalidTypeId;
    TypeId underlyingType_ = InvalidTypeId;
    MetadataTypeKind kind_ = MetadataTypeKind::Object;
    TypeFlags flags_ = TypeFlags::None;
    Base::String xamlNamespace_;
    Base::String name_;
    Base::Vector<TypeId> interfaces_;
    Base::Vector<MemberId> properties_;
    Base::Vector<MemberId> fields_;
    Base::Vector<MemberId> enumValues_;
    Base::Vector<MemberId> events_;
    Base::Vector<MemberId> methods_;
};

class AERO_API MetadataDescriptorStore final {
public:
    struct MemberLocation final {
        MetadataDescriptorKind kind = MetadataDescriptorKind::Property;
        std::uint32_t index = 0U;
    };

    MetadataDescriptorStore() noexcept = default;

    MetadataDescriptorStore(const MetadataDescriptorStore&) = delete;
    MetadataDescriptorStore& operator=(const MetadataDescriptorStore&) = delete;
    MetadataDescriptorStore(MetadataDescriptorStore&&) = delete;
    MetadataDescriptorStore& operator=(MetadataDescriptorStore&&) = delete;

    Base::Result<void> Build(const TypeRegistry& source) noexcept;

    bool IsSealed() const noexcept { return sealed_; }
    std::uint32_t TypeCount() const noexcept { return types_.Size(); }
    std::uint32_t PropertyCount() const noexcept { return properties_.Size(); }
    std::uint32_t FieldCount() const noexcept { return fields_.Size(); }
    std::uint32_t EnumValueCount() const noexcept { return enumValues_.Size(); }
    std::uint32_t EventCount() const noexcept { return events_.Size(); }
    std::uint32_t MethodCount() const noexcept { return methods_.Size(); }

    Base::Span<const MetadataTypeDescriptor> Types() const noexcept {
        return {types_.Data(), types_.Size()};
    }

    const MetadataTypeDescriptor* FindType(TypeId id) const noexcept;
    const MetadataTypeDescriptor* FindType(
        Base::StringView xamlNamespace,
        Base::StringView name) const noexcept;
    const MetadataPropertyDescriptor* FindProperty(MemberId id) const noexcept;
    const MetadataPropertyDescriptor* FindProperty(
        TypeId ownerType,
        Base::StringView name,
        bool includeBaseTypes = true) const noexcept;
    const MetadataFieldDescriptor* FindField(MemberId id) const noexcept;
    const MetadataFieldDescriptor* FindField(
        TypeId ownerType,
        Base::StringView name) const noexcept;
    const MetadataEnumValueDescriptor* FindEnumValue(MemberId id) const noexcept;
    const MetadataEnumValueDescriptor* FindEnumValue(
        TypeId ownerType,
        Base::StringView name) const noexcept;
    const MetadataEnumValueDescriptor* FindEnumValue(
        TypeId ownerType,
        std::uint64_t rawValue) const noexcept;
    const MetadataEventDescriptor* FindEvent(MemberId id) const noexcept;
    const MetadataEventDescriptor* FindEvent(
        TypeId ownerType,
        Base::StringView name,
        bool includeBaseTypes = true) const noexcept;
    const MetadataMethodDescriptor* FindMethod(MemberId id) const noexcept;
    const MetadataMethodDescriptor* FindMethod(
        TypeId ownerType,
        Base::StringView name,
        Base::Span<const TypeId> parameterTypes,
        bool includeBaseTypes = true) const noexcept;

    bool IsDerivedFrom(TypeId type, TypeId expectedBase) const noexcept;
    bool Implements(TypeId type, TypeId interfaceType) const noexcept;
    bool IsAssignableFrom(TypeId targetType, TypeId sourceType) const noexcept;
    Base::Result<Base::HashCode> ComputeHash() const noexcept;

private:
    Base::Vector<MetadataTypeDescriptor> types_;
    Base::Vector<MetadataPropertyDescriptor> properties_;
    Base::Vector<MetadataFieldDescriptor> fields_;
    Base::Vector<MetadataEnumValueDescriptor> enumValues_;
    Base::Vector<MetadataEventDescriptor> events_;
    Base::Vector<MetadataMethodDescriptor> methods_;
    Base::HashMap<TypeId, std::uint32_t> typeIndex_;
    Base::HashMap<MemberId, MemberLocation> memberIndex_;
    bool sealed_ = false;
};

struct TypeFactoryFacet final {
    TypeId type = InvalidTypeId;
    ObjectFactory factory = nullptr;
};

struct ContentFacet final {
    TypeId type = InvalidTypeId;
    MemberId member = InvalidMemberId;
};

struct PropertyAccessorFacet final {
    MemberId member = InvalidMemberId;
    PropertyAccessKind access = PropertyAccessKind::External;
    PropertyGetCallback get = nullptr;
    PropertySetCallback set = nullptr;
    PropertyProviderId provider = InvalidPropertyProviderId;
    void* context = nullptr;
};

struct ValueMemberAccessorFacet final {
    MemberId member = InvalidMemberId;
    ValueMemberGetCallback get = nullptr;
    ValueMemberSetCallback set = nullptr;
    void* context = nullptr;
};

struct MethodInvokerFacet final {
    MemberId member = InvalidMemberId;
    MethodInvokeCallback invoke = nullptr;
    void* context = nullptr;
};

struct DependencyPropertyFacet final {
    MemberId member = InvalidMemberId;
    MemberId canonicalMember = InvalidMemberId;
    TypeId registeredOwnerType = InvalidTypeId;
    TypeId valueType = InvalidTypeId;
    std::uint32_t flags = 0U;
    std::uint32_t metadataCount = 0U;
    const DependencyProperty* property = nullptr;
};

struct RoutedEventFacet final {
    MemberId member = InvalidMemberId;
    TypeId ownerType = InvalidTypeId;
    TypeId eventArgsType = InvalidTypeId;
    RoutingStrategy strategy = RoutingStrategy::Bubble;
};

struct PropertyChangeNotificationFacet final {
    TypeId type = InvalidTypeId;
    PropertyChangeSubscribeCallback subscribe = nullptr;
    PropertyChangeUnsubscribeCallback unsubscribe = nullptr;
    void* context = nullptr;
};

struct CollectionChangeNotificationFacet final {
    TypeId type = InvalidTypeId;
    CollectionChangeSubscribeCallback subscribe = nullptr;
    CollectionChangeUnsubscribeCallback unsubscribe = nullptr;
    void* context = nullptr;
};

// Sealed value facets own their runtime registrations. No runtime lookup is
// routed back through TypeRegistry after MetadataDomain::Seal().
struct ValueSemanticsFacet final {
    TypeId type = InvalidTypeId;
    Base::Ref<ValueTypeSemantics> semantics;
};

struct TextConverterFacet final {
    TypeId type = InvalidTypeId;
    TextValueConverterCallback convert = nullptr;
    void* context = nullptr;
};

class AERO_API MetadataFacetStore final {
public:
    MetadataFacetStore() noexcept = default;

    MetadataFacetStore(const MetadataFacetStore&) = delete;
    MetadataFacetStore& operator=(const MetadataFacetStore&) = delete;
    MetadataFacetStore(MetadataFacetStore&&) = delete;
    MetadataFacetStore& operator=(MetadataFacetStore&&) = delete;

    Base::Result<void> Build(
        const TypeRegistry& source,
        const MetadataBehaviorRegistrationStore& behaviors,
        const MetadataDescriptorStore& descriptors,
        const DependencyPropertyRegistry& dependencyProperties,
        const RoutedEventCatalog& routedEvents) noexcept;
    Base::Result<void> BuildValueFacets(
        const MetadataValueRegistrationStore& source,
        const MetadataDescriptorStore& descriptors) noexcept;

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
    const MetadataDescriptorStore* descriptors_ = nullptr;
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

    Base::HashMap<TypeId, std::uint32_t> factoryIndex_;
    Base::HashMap<TypeId, std::uint32_t> contentIndex_;
    Base::HashMap<MemberId, std::uint32_t> propertyAccessorIndex_;
    Base::HashMap<MemberId, std::uint32_t> valueMemberAccessorIndex_;
    Base::HashMap<MemberId, std::uint32_t> methodInvokerIndex_;
    Base::HashMap<MemberId, std::uint32_t> dependencyPropertyIndex_;
    Base::HashMap<MemberId, std::uint32_t> routedEventIndex_;
    Base::HashMap<TypeId, std::uint32_t>
        propertyChangeNotificationIndex_;
    Base::HashMap<TypeId, std::uint32_t>
        collectionChangeNotificationIndex_;
    Base::HashMap<TypeId, std::uint32_t> valueSemanticsIndex_;
    Base::HashMap<TypeId, std::uint32_t> textConverterIndex_;
    Base::HashMap<TypeId, MetadataFacetMask> typeMasks_;
    Base::HashMap<MemberId, MetadataFacetMask> memberMasks_;
    bool sealed_ = false;
    bool valueFacetsSealed_ = false;

    Base::Result<void> AddTypeMask(
        TypeId type,
        MetadataFacetKind kind) noexcept;
    Base::Result<void> AddMemberMask(
        MemberId member,
        MetadataFacetKind kind) noexcept;
};

} // namespace Aero::Core
