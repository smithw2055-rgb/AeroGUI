#pragma once

// Private executable metadata storage. TypeRegistry is the public structural
// source of truth; these records never cross the GUI-kernel boundary.

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include "MetaInternals.hpp"
#include <Aero/Meta/TypeRegistry.hpp>

#include <cstdint>

namespace Aero::Core {
class BehaviorTable;
class ValueTable;
class DependencyProperty;
class DependencyPropertyRegistry;
}

namespace Aero::Core::Detail {

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

struct TypeFactoryFacet final {
    TypeId type = InvalidTypeId;
    ObjectFactory factory = nullptr;
};

struct ContentFacet final {
    TypeId type = InvalidTypeId;
    MemberId member = InvalidMemberId;
    ContentKind kind = ContentKind::Single;
    ContentFlags flags = ContentFlags::None;
    ContentWriteCallback write = nullptr;
    ContentClearCallback clear = nullptr;
    void* context = nullptr;
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
// routed back through TypeRegistry after MetaRegistry::Seal().
struct ValueSemanticsFacet final {
    TypeId type = InvalidTypeId;
    Base::Ref<ValueTypeSemantics> semantics;
};

struct TextConverterFacet final {
    TypeId type = InvalidTypeId;
    TextValueConverterCallback convert = nullptr;
    void* context = nullptr;
};

class MetaTable final {
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

    Base::HashMap<TypeId, std::uint32_t> factoryIndex_;
    Base::HashMap<TypeId, std::uint32_t> contentIndex_;
    Base::HashMap<MemberId, std::uint32_t> contentMemberIndex_;
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

} // namespace Aero::Core::Detail
