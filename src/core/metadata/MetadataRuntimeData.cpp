#include "MetadataRuntimeData.hpp"

// Executable metadata behavior is private to MetadataRuntime.
#include "MetadataBehaviorRegistrationStore.hpp"

#include <Aero/Core/Property/DependencyProperty.hpp>

#include <utility>

namespace Aero::Core::Detail {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

template<class Less>
Base::Result<void> BuildOrder(
    std::uint32_t count,
    Base::Vector<std::uint32_t>& order,
    Less less) noexcept {
    Base::Result<void> result = order.TryReserve(count);
    if (!result) return result.GetStatus();
    for (std::uint32_t index = 0U; index < count; ++index) {
        result = order.TryPushBack(index);
        if (!result) return result.GetStatus();
    }
    for (std::uint32_t index = 1U; index < count; ++index) {
        const std::uint32_t value = order[index];
        std::uint32_t cursor = index;
        while (cursor > 0U && less(value, order[cursor - 1U])) {
            order[cursor] = order[cursor - 1U];
            --cursor;
        }
        order[cursor] = value;
    }
    return {};
}

template<class Key, class Value>
Base::Result<void> InsertUnique(
    Base::HashMap<Key, Value>& map,
    Key key,
    const Value& value,
    const char* message) noexcept {
    Base::Result<typename Base::HashMap<Key, Value>::InsertResult> inserted =
        map.TryInsert(key, value);
    if (!inserted) return inserted.GetStatus();
    if (!inserted.Value().inserted) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists, message);
    }
    return {};
}

bool HasEventFlag(EventFlags value, EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

} // namespace

Base::Result<void> MetadataFacetStore::AddTypeMask(
    TypeId type,
    MetadataFacetKind kind) noexcept {
    MetadataFacetMask* mask = typeMasks_.Find(type);
    if (mask != nullptr) {
        *mask |= MetadataFacetBit(kind);
        return {};
    }
    return InsertUnique(typeMasks_, type, MetadataFacetBit(kind),
        "Type facet mask collision");
}

Base::Result<void> MetadataFacetStore::AddMemberMask(
    MemberId member,
    MetadataFacetKind kind) noexcept {
    MetadataFacetMask* mask = memberMasks_.Find(member);
    if (mask != nullptr) {
        *mask |= MetadataFacetBit(kind);
        return {};
    }
    return InsertUnique(memberMasks_, member, MetadataFacetBit(kind),
        "Member facet mask collision");
}

Base::Result<void> MetadataFacetStore::Build(
    const TypeRegistry& types,
    const MetadataBehaviorRegistrationStore& behaviors,
    const DependencyPropertyRegistry& dependencyProperties,
    const RoutedEventCatalog& routedEvents) noexcept {
    if (sealed_) return InvalidState("MetadataFacetStore is already sealed");
    if (!types.IsFrozen() || !behaviors.IsFrozen() ||
        !dependencyProperties.IsFrozen() || !routedEvents.IsFrozen()) {
        return InvalidState(
            "Metadata sources must be sealed before building facets");
    }
    types_ = &types;

    for (const TypeInfo& type : types.Types()) {
        Base::Result<void> result;
        const TypeFactoryRegistration* factory =
            behaviors.FindTypeFactory(type.Id());
        if (factory != nullptr && factory->factory != nullptr) {
            const std::uint32_t index = factories_.Size();
            result = factories_.TryPushBack({type.Id(), factory->factory});
            if (!result) return result.GetStatus();
            result = InsertUnique(factoryIndex_, type.Id(), index,
                "Type factory facet collision");
            if (!result) return result.GetStatus();
            result = AddTypeMask(type.Id(), MetadataFacetKind::TypeFactory);
            if (!result) return result.GetStatus();
        }
        if (type.ContentMember() != InvalidMemberId) {
            const ContentAccessorRegistration* content =
                behaviors.FindContentAccessor(
                    type.ContentMember());
            const PropertyInfo* descriptor =
                types.FindProperty(type.ContentMember());
            if (descriptor == nullptr) {
                return InvalidState(
                    "Content metadata references a missing property");
            }
            const bool collection =
                (static_cast<std::uint32_t>(descriptor->Flags()) &
                 static_cast<std::uint32_t>(PropertyFlags::Collection)) != 0U;
            const ContentKind kind = content != nullptr
                ? content->kind
                : (collection ? ContentKind::Collection : ContentKind::Single);
            const std::uint32_t index = contents_.Size();
            result = contents_.TryPushBack({
                type.Id(), type.ContentMember(), kind,
                content != nullptr ? content->flags : ContentFlags::None,
                content != nullptr ? content->write : nullptr,
                content != nullptr ? content->clear : nullptr,
                content != nullptr ? content->context : nullptr});
            if (!result) return result.GetStatus();
            result = InsertUnique(contentIndex_, type.Id(), index,
                "Content facet collision");
            if (!result) return result.GetStatus();
            result = InsertUnique(
                contentMemberIndex_, type.ContentMember(), index,
                "Content member facet collision");
            if (!result) return result.GetStatus();
            result = AddTypeMask(type.Id(), MetadataFacetKind::Content);
            if (!result) return result.GetStatus();
            result = AddMemberMask(
                type.ContentMember(), MetadataFacetKind::Content);
            if (!result) return result.GetStatus();
        }
        for (const PropertyInfo& property :
             type.Properties()) {
            if (property.Id() == type.ContentMember()) {
                continue;
            }
            const ContentAccessorRegistration* content =
                behaviors.FindContentAccessor(
                    property.Id());
            if (content == nullptr) continue;
            const std::uint32_t index = contents_.Size();
            result = contents_.TryPushBack({
                type.Id(),
                property.Id(),
                content->kind,
                content->flags,
                content->write,
                content->clear,
                content->context});
            if (!result) return result.GetStatus();
            result = InsertUnique(
                contentMemberIndex_,
                property.Id(),
                index,
                "Structural member facet collision");
            if (!result) return result.GetStatus();
            result = AddMemberMask(
                property.Id(),
                MetadataFacetKind::Content);
            if (!result) return result.GetStatus();
        }
        const PropertyChangeNotificationRegistration* notification =
            behaviors.FindPropertyChangeNotification(type.Id());
        if (notification != nullptr) {
            const std::uint32_t index =
                propertyChangeNotifications_.Size();
            result = propertyChangeNotifications_.TryPushBack({
                type.Id(),
                notification->subscribe,
                notification->unsubscribe,
                notification->context});
            if (!result) return result.GetStatus();
            result = InsertUnique(
                propertyChangeNotificationIndex_,
                type.Id(),
                index,
                "Property-change notification facet collision");
            if (!result) return result.GetStatus();
            result = AddTypeMask(
                type.Id(),
                MetadataFacetKind::PropertyChangeNotification);
            if (!result) return result.GetStatus();
        }
        const CollectionChangeNotificationRegistration*
            collectionNotification =
                behaviors.FindCollectionChangeNotification(type.Id());
        if (collectionNotification != nullptr) {
            const std::uint32_t index =
                collectionChangeNotifications_.Size();
            result = collectionChangeNotifications_.TryPushBack({
                type.Id(),
                collectionNotification->subscribe,
                collectionNotification->unsubscribe,
                collectionNotification->context});
            if (!result) return result.GetStatus();
            result = InsertUnique(
                collectionChangeNotificationIndex_,
                type.Id(),
                index,
                "Collection-change notification facet collision");
            if (!result) return result.GetStatus();
            result = AddTypeMask(
                type.Id(),
                MetadataFacetKind::CollectionChangeNotification);
            if (!result) return result.GetStatus();
        }

        for (const PropertyInfo& property : type.Properties()) {
            const PropertyAccessorRegistration* accessor =
                behaviors.FindPropertyAccessor(property.Id());
            if (accessor != nullptr &&
                accessor->access != PropertyAccessKind::External) {
                const std::uint32_t index = propertyAccessors_.Size();
                result = propertyAccessors_.TryPushBack({
                    property.Id(), accessor->access, accessor->get,
                    accessor->set, accessor->provider, accessor->context});
                if (!result) return result.GetStatus();
                result = InsertUnique(
                    propertyAccessorIndex_, property.Id(), index,
                    "Property accessor facet collision");
                if (!result) return result.GetStatus();
                result = AddMemberMask(
                    property.Id(), MetadataFacetKind::PropertyAccessor);
                if (!result) return result.GetStatus();
            }

            const DependencyProperty* dependency = dependencyProperties.Find(
                DependencyPropertyHandle{property.Id()});
            if (dependency != nullptr) {
                const std::uint32_t index = dependencyProperties_.Size();
                result = dependencyProperties_.TryPushBack({
                    property.Id(), dependency->Handle().value,
                    dependency->RegisteredOwnerType(), dependency->ValueType(),
                    static_cast<std::uint32_t>(dependency->Flags()),
                    dependency->MetadataCount(), dependency});
                if (!result) return result.GetStatus();
                result = InsertUnique(
                    dependencyPropertyIndex_, property.Id(), index,
                    "Dependency property facet collision");
                if (!result) return result.GetStatus();
                result = AddMemberMask(
                    property.Id(), MetadataFacetKind::DependencyProperty);
                if (!result) return result.GetStatus();
            }
        }

        for (const FieldInfo& field : type.Fields()) {
            const ValueMemberAccessorRegistration* accessor =
                behaviors.FindValueMemberAccessor(field.Id());
            if (accessor == nullptr || accessor->get == nullptr) continue;
            const std::uint32_t index = valueMemberAccessors_.Size();
            result = valueMemberAccessors_.TryPushBack({
                field.Id(), accessor->get, accessor->set, accessor->context});
            if (!result) return result.GetStatus();
            result = InsertUnique(
                valueMemberAccessorIndex_, field.Id(), index,
                "Value member accessor facet collision");
            if (!result) return result.GetStatus();
            result = AddMemberMask(
                field.Id(), MetadataFacetKind::ValueMemberAccessor);
            if (!result) return result.GetStatus();
        }

        for (const MethodInfo& method : type.Methods()) {
            const MethodInvokerRegistration* invoker =
                behaviors.FindMethodInvoker(method.Id());
            if (invoker == nullptr || invoker->invoke == nullptr) continue;
            const std::uint32_t index = methodInvokers_.Size();
            result = methodInvokers_.TryPushBack({
                method.Id(), invoker->invoke, invoker->context});
            if (!result) return result.GetStatus();
            result = InsertUnique(methodInvokerIndex_, method.Id(), index,
                "Method invoker facet collision");
            if (!result) return result.GetStatus();
            result = AddMemberMask(
                method.Id(), MetadataFacetKind::MethodInvoker);
            if (!result) return result.GetStatus();
        }

        for (const EventInfo& event : type.Events()) {
            if (!HasEventFlag(event.Flags(), EventFlags::Routed)) continue;
            const RoutedEventCatalog::Definition* definition =
                routedEvents.Find({event.Id()});
            if (definition == nullptr) {
                return InvalidState(
                    "Routed event metadata has no catalog definition");
            }
            const std::uint32_t index = routedEvents_.Size();
            result = routedEvents_.TryPushBack({
                event.Id(), event.OwnerType(), event.EventArgsType(),
                definition->strategy});
            if (!result) return result.GetStatus();
            result = InsertUnique(routedEventIndex_, event.Id(), index,
                "Routed event facet collision");
            if (!result) return result.GetStatus();
            result = AddMemberMask(event.Id(), MetadataFacetKind::RoutedEvent);
            if (!result) return result.GetStatus();
        }
    }

    sealed_ = true;
    return {};
}

MetadataFacetMask MetadataFacetStore::TypeFacets(TypeId type) const noexcept {
    const MetadataFacetMask* mask = typeMasks_.Find(type);
    return mask != nullptr ? *mask : 0U;
}

MetadataFacetMask MetadataFacetStore::MemberFacets(
    MemberId member) const noexcept {
    const MetadataFacetMask* mask = memberMasks_.Find(member);
    return mask != nullptr ? *mask : 0U;
}

const TypeFactoryFacet* MetadataFacetStore::FindTypeFactory(
    TypeId type) const noexcept {
    const std::uint32_t* index = factoryIndex_.Find(type);
    return index != nullptr && *index < factories_.Size()
        ? &factories_[*index] : nullptr;
}

const ContentFacet* MetadataFacetStore::FindContent(TypeId type) const noexcept {
    const std::uint32_t* index = contentIndex_.Find(type);
    return index != nullptr && *index < contents_.Size()
        ? &contents_[*index] : nullptr;
}

const ContentFacet* MetadataFacetStore::FindContentByMember(
    MemberId member) const noexcept {
    const std::uint32_t* index = contentMemberIndex_.Find(member);
    return index != nullptr && *index < contents_.Size()
        ? &contents_[*index] : nullptr;
}

MemberId MetadataFacetStore::FindContentMember(TypeId type) const noexcept {
    if (types_ == nullptr) return InvalidMemberId;
    TypeId current = type;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_->TypeCount(); ++depth) {
        const ContentFacet* content = FindContent(current);
        if (content != nullptr) return content->member;
        const TypeInfo* descriptor = types_->FindType(current);
        if (descriptor == nullptr) return InvalidMemberId;
        current = descriptor->BaseType();
    }
    return InvalidMemberId;
}

const PropertyAccessorFacet* MetadataFacetStore::FindPropertyAccessor(
    MemberId member) const noexcept {
    const std::uint32_t* index = propertyAccessorIndex_.Find(member);
    return index != nullptr && *index < propertyAccessors_.Size()
        ? &propertyAccessors_[*index] : nullptr;
}

const ValueMemberAccessorFacet* MetadataFacetStore::FindValueMemberAccessor(
    MemberId member) const noexcept {
    const std::uint32_t* index = valueMemberAccessorIndex_.Find(member);
    return index != nullptr && *index < valueMemberAccessors_.Size()
        ? &valueMemberAccessors_[*index] : nullptr;
}

const MethodInvokerFacet* MetadataFacetStore::FindMethodInvoker(
    MemberId member) const noexcept {
    const std::uint32_t* index = methodInvokerIndex_.Find(member);
    return index != nullptr && *index < methodInvokers_.Size()
        ? &methodInvokers_[*index] : nullptr;
}

const DependencyPropertyFacet* MetadataFacetStore::FindDependencyProperty(
    MemberId member) const noexcept {
    const std::uint32_t* index = dependencyPropertyIndex_.Find(member);
    return index != nullptr && *index < dependencyProperties_.Size()
        ? &dependencyProperties_[*index] : nullptr;
}

const RoutedEventFacet* MetadataFacetStore::FindRoutedEvent(
    MemberId member) const noexcept {
    const std::uint32_t* index = routedEventIndex_.Find(member);
    return index != nullptr && *index < routedEvents_.Size()
        ? &routedEvents_[*index] : nullptr;
}

const PropertyChangeNotificationFacet*
MetadataFacetStore::FindPropertyChangeNotification(
    TypeId type) const noexcept {
    if (types_ == nullptr) return nullptr;
    const auto find = [this](
        const auto& self,
        TypeId current,
        std::uint32_t depth) noexcept
        -> const PropertyChangeNotificationFacet* {
        if (current == InvalidTypeId ||
            depth > types_->TypeCount()) {
            return nullptr;
        }
        const std::uint32_t* index =
            propertyChangeNotificationIndex_.Find(current);
        if (index != nullptr &&
            *index < propertyChangeNotifications_.Size()) {
            return &propertyChangeNotifications_[*index];
        }
        const TypeInfo* descriptor =
            types_->FindType(current);
        if (descriptor == nullptr) return nullptr;
        for (TypeId interfaceType : descriptor->Interfaces()) {
            const PropertyChangeNotificationFacet* inherited =
                self(self, interfaceType, depth + 1U);
            if (inherited != nullptr) return inherited;
        }
        return self(self, descriptor->BaseType(), depth + 1U);
    };
    return find(find, type, 0U);
}

const CollectionChangeNotificationFacet*
MetadataFacetStore::FindCollectionChangeNotification(
    TypeId type) const noexcept {
    if (types_ == nullptr) return nullptr;
    const auto find = [this](
        const auto& self,
        TypeId current,
        std::uint32_t depth) noexcept
        -> const CollectionChangeNotificationFacet* {
        if (current == InvalidTypeId ||
            depth > types_->TypeCount()) {
            return nullptr;
        }
        const std::uint32_t* index =
            collectionChangeNotificationIndex_.Find(current);
        if (index != nullptr &&
            *index < collectionChangeNotifications_.Size()) {
            return &collectionChangeNotifications_[*index];
        }
        const TypeInfo* descriptor =
            types_->FindType(current);
        if (descriptor == nullptr) return nullptr;
        for (TypeId interfaceType : descriptor->Interfaces()) {
            const CollectionChangeNotificationFacet* inherited =
                self(self, interfaceType, depth + 1U);
            if (inherited != nullptr) return inherited;
        }
        return self(self, descriptor->BaseType(), depth + 1U);
    };
    return find(find, type, 0U);
}

Base::Result<Base::HashCode> MetadataFacetStore::ComputeHash() const noexcept {
    if (!sealed_) return InvalidState("Facet hash requires a sealed store");
    Base::Detail::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.FACETS.V4";
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(MetadataFacetFormatVersion);

    Base::Vector<std::uint32_t> order;
    Base::Result<void> result = BuildOrder(factories_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return factories_[left].type < factories_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) builder.AddU64(factories_[index].type);

    order.Clear();
    result = BuildOrder(contents_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return contents_[left].type < contents_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const ContentFacet& facet = contents_[index];
        builder.AddU64(facet.type);
        builder.AddU64(facet.member);
        builder.AddByte(static_cast<std::uint8_t>(facet.kind));
        builder.AddByte(static_cast<std::uint8_t>(facet.flags));
        builder.AddByte(facet.write != nullptr ? 1U : 0U);
        builder.AddByte(facet.clear != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(propertyAccessors_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return propertyAccessors_[left].member <
                propertyAccessors_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const PropertyAccessorFacet& facet = propertyAccessors_[index];
        builder.AddU64(facet.member);
        builder.AddByte(static_cast<std::uint8_t>(facet.access));
        builder.AddU64(facet.provider);
        builder.AddByte(facet.get != nullptr ? 1U : 0U);
        builder.AddByte(facet.set != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(valueMemberAccessors_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return valueMemberAccessors_[left].member <
                valueMemberAccessors_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const ValueMemberAccessorFacet& facet = valueMemberAccessors_[index];
        builder.AddU64(facet.member);
        builder.AddByte(facet.get != nullptr ? 1U : 0U);
        builder.AddByte(facet.set != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(methodInvokers_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return methodInvokers_[left].member < methodInvokers_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        builder.AddU64(methodInvokers_[index].member);
    }

    order.Clear();
    result = BuildOrder(dependencyProperties_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return dependencyProperties_[left].member <
                dependencyProperties_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const DependencyPropertyFacet& facet = dependencyProperties_[index];
        builder.AddU64(facet.member);
        builder.AddU64(facet.canonicalMember);
        builder.AddU64(facet.registeredOwnerType);
        builder.AddU64(facet.valueType);
        builder.AddU32(facet.flags);
        builder.AddU32(facet.metadataCount);
    }

    order.Clear();
    result = BuildOrder(routedEvents_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return routedEvents_[left].member < routedEvents_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const RoutedEventFacet& facet = routedEvents_[index];
        builder.AddU64(facet.member);
        builder.AddU64(facet.ownerType);
        builder.AddU64(facet.eventArgsType);
    }

    order.Clear();
    result = BuildOrder(propertyChangeNotifications_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return propertyChangeNotifications_[left].type <
                propertyChangeNotifications_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const PropertyChangeNotificationFacet& facet =
            propertyChangeNotifications_[index];
        builder.AddU64(facet.type);
        builder.AddByte(facet.subscribe != nullptr ? 1U : 0U);
        builder.AddByte(facet.unsubscribe != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(collectionChangeNotifications_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return collectionChangeNotifications_[left].type <
                collectionChangeNotifications_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const CollectionChangeNotificationFacet& facet =
            collectionChangeNotifications_[index];
        builder.AddU64(facet.type);
        builder.AddByte(facet.subscribe != nullptr ? 1U : 0U);
        builder.AddByte(facet.unsubscribe != nullptr ? 1U : 0U);
    }
    return builder.Finish();
}

} // namespace Aero::Core::Detail
