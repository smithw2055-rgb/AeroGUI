// ===== MetaTable =====

// Executable metadata behavior is private to Meta::Registry.

#include <Aero/DependencyProperty.hpp>

namespace Aero {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

template<class Less>
Base::Result<void> BuildOrder(
    std::uint32_t count,
    Base::Vector<std::uint32_t>& order,
    Less less) noexcept {
    Base::Result<void> result = order.Reserve(count);
    if (!result) return result.GetStatus();
    for (std::uint32_t index = 0U; index < count; ++index) {
        result = order.PushBack(index);
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

bool HasEventFlag(EventFlags value, EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool IsValueType(const TypeInfo& type) noexcept {
    return (static_cast<std::uint32_t>(type.Flags()) &
        static_cast<std::uint32_t>(TypeFlags::ValueType)) != 0U;
}

} // namespace

Base::Result<void> MetaTable::SetTypeFacet(
    TypeId type,
    MetadataFacetKind kind,
    std::uint32_t index) noexcept {
    if (type == InvalidTypeId || index == InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Type facet reference is invalid");
    }
    const std::uint32_t* draftPos = typeDraftIndex_.Find(type);
    FacetDraft* draft = nullptr;
    if (draftPos != nullptr && *draftPos < typeDrafts_.Size()) {
        draft = &typeDrafts_[*draftPos];
    } else {
        const std::uint32_t newPos = typeDrafts_.Size();
        Base::Result<FacetDraft*> added = typeDrafts_.EmplaceBack();
        if (!added) return added.GetStatus();
        added.Value()->key = type;
        draft = added.Value();
        Base::Result<Base::HashMap<TypeId, std::uint32_t>::InsertResult> inserted =
            typeDraftIndex_.Insert(type, newPos);
        if (!inserted) return inserted.GetStatus();
    }
    const std::uint8_t slot = static_cast<std::uint8_t>(kind);
    if (draft->facets[slot] != InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Type facet is already registered");
    }
    draft->facets[slot] = index;
    return {};
}

Base::Result<void> MetaTable::SetMemberFacet(
    MemberId member,
    MetadataFacetKind kind,
    std::uint32_t index) noexcept {
    if (member == InvalidMemberId || index == InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Member facet reference is invalid");
    }
    const std::uint32_t* draftPos = memberDraftIndex_.Find(member);
    FacetDraft* draft = nullptr;
    if (draftPos != nullptr && *draftPos < memberDrafts_.Size()) {
        draft = &memberDrafts_[*draftPos];
    } else {
        const std::uint32_t newPos = memberDrafts_.Size();
        Base::Result<FacetDraft*> added = memberDrafts_.EmplaceBack();
        if (!added) return added.GetStatus();
        added.Value()->key = member;
        draft = added.Value();
        Base::Result<Base::HashMap<MemberId, std::uint32_t>::InsertResult> inserted =
            memberDraftIndex_.Insert(member, newPos);
        if (!inserted) return inserted.GetStatus();
    }
    const std::uint8_t slot = static_cast<std::uint8_t>(kind);
    if (draft->facets[slot] != InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Member facet is already registered");
    }
    draft->facets[slot] = index;
    return {};
}

Base::Result<void> MetaTable::Build(
    const TypeRegistry& types,
    const BehaviorTable& behaviors,
    const DependencyPropertyRegistry& dependencyProperties,
    const RoutedEventTable& routedEvents) noexcept {
    if (sealed_) return InvalidState("MetaTable is already sealed");
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
            result = factories_.PushBack({type.Id(), factory->factory});
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::TypeFactory, index);
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
            result = contents_.PushBack({
                type.Id(), type.ContentMember(), kind,
                content != nullptr ? content->flags : ContentFlags::None,
                content != nullptr ? content->write : nullptr,
                content != nullptr ? content->clear : nullptr,
                content != nullptr ? content->context : nullptr});
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::Content, index);
            if (!result) return result.GetStatus();
            result = SetMemberFacet(
                type.ContentMember(), MetadataFacetKind::Content, index);
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
            result = contents_.PushBack({
                type.Id(),
                property.Id(),
                content->kind,
                content->flags,
                content->write,
                content->clear,
                content->context});
            if (!result) return result.GetStatus();
            result = SetMemberFacet(
                property.Id(), MetadataFacetKind::Content, index);
            if (!result) return result.GetStatus();
        }
        const PropertyChangeNotificationRegistration* notification =
            behaviors.FindPropertyChangeNotification(type.Id());
        if (notification != nullptr) {
            const std::uint32_t index =
                propertyChangeNotifications_.Size();
            result = propertyChangeNotifications_.PushBack({
                type.Id(),
                notification->subscribe,
                notification->unsubscribe,
                notification->context});
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::PropertyChangeNotification,
                index);
            if (!result) return result.GetStatus();
        }
        const CollectionChangeNotificationRegistration*
            collectionNotification =
                behaviors.FindCollectionChangeNotification(type.Id());
        if (collectionNotification != nullptr) {
            const std::uint32_t index =
                collectionChangeNotifications_.Size();
            result = collectionChangeNotifications_.PushBack({
                type.Id(),
                collectionNotification->subscribe,
                collectionNotification->unsubscribe,
                collectionNotification->context});
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::CollectionChangeNotification,
                index);
            if (!result) return result.GetStatus();
        }

        for (const PropertyInfo& property : type.Properties()) {
            const PropertyAccessorRegistration* accessor =
                behaviors.FindPropertyAccessor(property.Id());
            if (accessor != nullptr &&
                accessor->access != PropertyAccessKind::External) {
                const std::uint32_t index = propertyAccessors_.Size();
                result = propertyAccessors_.PushBack({
                    property.Id(), accessor->access, accessor->get,
                    accessor->set, accessor->provider, accessor->context});
                if (!result) return result.GetStatus();
                result = SetMemberFacet(
                    property.Id(), MetadataFacetKind::PropertyAccessor,
                    index);
                if (!result) return result.GetStatus();
            }

            const DependencyProperty* dependency = dependencyProperties.Find(
                DependencyPropertyHandle{property.Id()});
            if (dependency != nullptr) {
                const std::uint32_t index = dependencyProperties_.Size();
                result = dependencyProperties_.PushBack({
                    property.Id(), dependency->Handle().value,
                    dependency->RegisteredOwnerType(), dependency->ValueType(),
                    static_cast<std::uint32_t>(dependency->Flags()),
                    dependency->MetadataCount(), dependency});
                if (!result) return result.GetStatus();
                result = SetMemberFacet(
                    property.Id(), MetadataFacetKind::DependencyProperty,
                    index);
                if (!result) return result.GetStatus();
            }
        }

        for (const EventInfo& event : type.Events()) {
            if (!HasEventFlag(event.Flags(), EventFlags::Routed)) continue;
            const RoutedEventTable::Definition* definition =
                routedEvents.Find({event.Id()});
            if (definition == nullptr) {
                return InvalidState(
                    "Routed event metadata has no catalog definition");
            }
            const std::uint32_t index = routedEvents_.Size();
            result = routedEvents_.PushBack({
                event.Id(), event.OwnerType(), event.EventArgsType(),
                definition->strategy});
            if (!result) return result.GetStatus();
            result = SetMemberFacet(
                event.Id(), MetadataFacetKind::RoutedEvent, index);
            if (!result) return result.GetStatus();
        }
    }

    sealed_ = true;
    return {};
}

Base::Result<void> MetaTable::SealIndex() noexcept {
    typeRecords_.Clear();
    memberRecords_.Clear();
    typeIndex_.Clear();
    memberIndex_.Clear();

    Base::Result<void> result = typeRecords_.Reserve(typeDrafts_.Size());
    if (!result) return result.GetStatus();
    result = memberRecords_.Reserve(memberDrafts_.Size());
    if (!result) return result.GetStatus();
    result = typeIndex_.Reserve(typeDrafts_.Size());
    if (!result) return result.GetStatus();
    result = memberIndex_.Reserve(memberDrafts_.Size());
    if (!result) return result.GetStatus();

    for (const FacetDraft& draft : typeDrafts_) {
        TypeRecord record;
        record.id = static_cast<TypeId>(draft.key);
        for (std::uint8_t kind = 0U; kind < 11U; ++kind) {
            if (draft.facets[kind] != InvalidFacetIndex) {
                record.mask |= UINT64_C(1) << kind;
            }
        }
        record.factoryIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::TypeFactory)];
        record.valueSemanticsIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::ValueSemantics)];
        record.textConverterIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::TextConverter)];
        record.propertyChangeIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::PropertyChangeNotification)];
        record.collectionChangeIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::CollectionChangeNotification)];

        const std::uint32_t position = typeRecords_.Size();
        result = typeRecords_.PushBack(record);
        if (!result) return result.GetStatus();
        Base::Result<Base::HashMap<TypeId, std::uint32_t>::InsertResult> inserted =
            typeIndex_.Insert(record.id, position);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata type facet record is duplicated");
        }
    }

    for (const FacetDraft& draft : memberDrafts_) {
        MemberRecord record;
        record.id = static_cast<MemberId>(draft.key);
        for (std::uint8_t kind = 0U; kind < 11U; ++kind) {
            if (draft.facets[kind] != InvalidFacetIndex) {
                record.mask |= UINT64_C(1) << kind;
            }
        }
        record.propertyAccessorIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::PropertyAccessor)];
        record.contentIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::Content)];
        record.valueMemberAccessorIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::ValueMemberAccessor)];
        record.methodInvokerIndex = draft.facets[static_cast<std::uint8_t>(MetadataFacetKind::MethodInvoker)];

        const std::uint32_t position = memberRecords_.Size();
        result = memberRecords_.PushBack(record);
        if (!result) return result.GetStatus();
        Base::Result<Base::HashMap<MemberId, std::uint32_t>::InsertResult> inserted =
            memberIndex_.Insert(record.id, position);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata member facet record is duplicated");
        }
    }

    typeDrafts_.Clear();
    memberDrafts_.Clear();
    typeDraftIndex_.Clear();
    memberDraftIndex_.Clear();
    return {};
}

MetadataFacetMask MetaTable::TypeFacets(TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    return position != nullptr && *position < typeRecords_.Size()
        ? typeRecords_[*position].mask
        : 0U;
}

MetadataFacetMask MetaTable::MemberFacets(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    return position != nullptr && *position < memberRecords_.Size()
        ? memberRecords_[*position].mask
        : 0U;
}

const TypeFactoryFacet* MetaTable::FindTypeFactory(
    TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].factoryIndex;
    return index < factories_.Size() ? &factories_[index] : nullptr;
}

const ContentFacet* MetaTable::FindContentByMember(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    if (position == nullptr || *position >= memberRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = memberRecords_[*position].contentIndex;
    return index < contents_.Size() ? &contents_[index] : nullptr;
}

const PropertyAccessorFacet* MetaTable::FindPropertyAccessor(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    if (position == nullptr || *position >= memberRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = memberRecords_[*position].propertyAccessorIndex;
    return index < propertyAccessors_.Size()
        ? &propertyAccessors_[index] : nullptr;
}

const ValueMemberAccessorFacet* MetaTable::FindValueMemberAccessor(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    if (position == nullptr || *position >= memberRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = memberRecords_[*position].valueMemberAccessorIndex;
    return index < valueMemberAccessors_.Size()
        ? &valueMemberAccessors_[index] : nullptr;
}

const MethodInvokerFacet* MetaTable::FindMethodInvoker(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    if (position == nullptr || *position >= memberRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = memberRecords_[*position].methodInvokerIndex;
    return index < methodInvokers_.Size()
        ? &methodInvokers_[index] : nullptr;
}

const PropertyChangeNotificationFacet*
MetaTable::FindPropertyChangeNotification(TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].propertyChangeIndex;
    return index < propertyChangeNotifications_.Size()
        ? &propertyChangeNotifications_[index] : nullptr;
}

const CollectionChangeNotificationFacet*
MetaTable::FindCollectionChangeNotification(TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].collectionChangeIndex;
    return index < collectionChangeNotifications_.Size()
        ? &collectionChangeNotifications_[index] : nullptr;
}

Base::Result<Base::HashCode> MetaTable::ComputeHash() const noexcept {
    if (!sealed_) return InvalidState("Facet hash requires a sealed store");
    Base::StableMetadataIdBuilder builder;
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

Base::Result<void> MetaTable::BuildValueFacets(
    const ValueTable& source,
    const TypeRegistry& types) noexcept {
    if (!sealed_ || valueFacetsSealed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            valueFacetsSealed_
                ? "Metadata value facets are already sealed"
                : "Core metadata facets must be built before value facets");
    }
    if (!source.IsFrozen() || !types.IsFrozen() ||
        types_ != &types) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata value facet sources are not sealed or do not match");
    }

    std::uint32_t valueTypeCount = 0U;
    for (const TypeInfo& type : types.Types()) {
        if (IsValueType(type)) ++valueTypeCount;
    }
    Base::Result<void> result = valueSemantics_.Reserve(valueTypeCount);
    if (!result) return result.GetStatus();
    result = textConverters_.Reserve(valueTypeCount);
    if (!result) return result.GetStatus();
    for (const TypeInfo& type : types.Types()) {
        if (IsValueType(type)) {
            const Base::Ref<ValueTypeSemantics>* semantics =
                source.FindValueSemantics(type.Id());
            if (semantics != nullptr &&
                semantics->Get() != nullptr) {
                ValueSemanticsFacet facet;
                facet.type = type.Id();
                facet.semantics = *semantics;
                const std::uint32_t index =
                    valueSemantics_.Size();
                result = valueSemantics_.PushBack(
                    std::move(facet));
                if (!result) return result.GetStatus();
                result = SetTypeFacet(
                    type.Id(), MetadataFacetKind::ValueSemantics, index);
                if (!result) return result.GetStatus();
            }
        }

        const TextValueConverterRegistration* converter =
            source.FindTextConverter(type.Id());
        if (converter != nullptr && converter->convert != nullptr) {
            TextConverterFacet facet;
            facet.type = type.Id();
            facet.convert = converter->convert;
            facet.context = converter->context;
            const std::uint32_t index = textConverters_.Size();
            result = textConverters_.PushBack(facet);
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::TextConverter, index);
            if (!result) return result.GetStatus();
        }
    }

    result = SealIndex();
    if (!result) return result.GetStatus();
    valueFacetsSealed_ = true;
    return {};
}

const ValueSemanticsFacet* MetaTable::FindValueSemantics(
    TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].valueSemanticsIndex;
    return index < valueSemantics_.Size()
        ? &valueSemantics_[index] : nullptr;
}

const TextConverterFacet* MetaTable::FindTextConverter(
    TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return nullptr;
    }
    const std::uint32_t index = typeRecords_[*position].textConverterIndex;
    return index < textConverters_.Size()
        ? &textConverters_[index] : nullptr;
}

Base::Result<Base::HashCode> ComputeMetadataValueFacetHash(
    const MetaTable& facets,
    const TypeRegistry& descriptors) noexcept {
    if (!facets.IsSealed() || !facets.ValueFacetsSealed() ||
        !descriptors.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Value facet hash requires sealed descriptors and facets");
    }

    Base::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.VALUE.FACETS.V1";
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(MetadataFacetFormatVersion);

    std::uint32_t semanticsCount = 0U;
    std::uint32_t converterCount = 0U;
    for (const TypeInfo& type : descriptors.Types()) {
        if (facets.FindValueSemantics(type.Id()) != nullptr) ++semanticsCount;
        if (facets.FindTextConverter(type.Id()) != nullptr) ++converterCount;
    }
    builder.AddU32(semanticsCount);
    for (const TypeInfo& type : descriptors.Types()) {
        const ValueSemanticsFacet* facet =
            facets.FindValueSemantics(type.Id());
        if (facet == nullptr || !facet->semantics) continue;
        const ValueTypeRegistration& registration =
            facet->semantics->Registration();
        builder.AddU64(type.Id());
        builder.AddU32(registration.size);
        builder.AddU32(registration.alignment);
        builder.AddByte(registration.copy != nullptr ? 1U : 0U);
        builder.AddByte(registration.destroy != nullptr ? 1U : 0U);
        builder.AddByte(registration.equals != nullptr ? 1U : 0U);
        builder.AddByte(registration.inlineSafe ? 1U : 0U);
    }

    builder.AddU32(converterCount);
    for (const TypeInfo& type : descriptors.Types()) {
        const TextConverterFacet* facet =
            facets.FindTextConverter(type.Id());
        if (facet == nullptr) continue;
        builder.AddU64(type.Id());
        builder.AddByte(facet->convert != nullptr ? 1U : 0U);
    }
    return builder.Finish();
}

} // namespace Aero
