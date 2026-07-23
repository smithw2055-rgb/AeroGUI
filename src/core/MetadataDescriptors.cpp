#include <Aero/Core/MetadataDescriptors.hpp>

#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/ObjectTree.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Core {
namespace {

Base::Status AlreadyBuilt(const char* name) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        name);
}

Base::Status SourceNotFrozen(const char* name) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        name);
}

template<class Less>
Base::Result<void> BuildOrder(
    std::uint32_t count,
    Base::Vector<std::uint32_t>& order,
    Less less) noexcept {
    Base::Result<void> reserved = order.TryReserve(count);
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U; index < count; ++index) {
        Base::Result<void> appended = order.TryPushBack(index);
        if (!appended) return appended.GetStatus();
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

bool HasPropertyFlag(PropertyFlags value, PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool HasEventFlag(EventFlags value, EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Result<void> InsertTypeIndex(
    Base::HashMap<TypeId, std::uint32_t>& index,
    TypeId id,
    std::uint32_t value) noexcept {
    Base::Result<Base::HashMap<TypeId, std::uint32_t>::InsertResult> inserted =
        index.TryInsert(id, value);
    if (!inserted) return inserted.GetStatus();
    if (!inserted.Value().inserted) {
        return Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Descriptor type id collision");
    }
    return {};
}

Base::Result<void> InsertMemberIndex(
    Base::HashMap<MemberId, MetadataDescriptorStore::MemberLocation>& index,
    MemberId id,
    MetadataDescriptorStore::MemberLocation value) noexcept {
    Base::Result<Base::HashMap<MemberId,
        MetadataDescriptorStore::MemberLocation>::InsertResult> inserted =
        index.TryInsert(id, value);
    if (!inserted) return inserted.GetStatus();
    if (!inserted.Value().inserted) {
        return Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Descriptor member id collision");
    }
    return {};
}

template<class Key>
Base::Result<void> InsertFacetIndex(
    Base::HashMap<Key, std::uint32_t>& index,
    Key key,
    std::uint32_t value) noexcept {
    Base::Result<typename Base::HashMap<Key, std::uint32_t>::InsertResult> inserted =
        index.TryInsert(key, value);
    if (!inserted) return inserted.GetStatus();
    if (!inserted.Value().inserted) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Metadata facet is already registered");
    }
    return {};
}

} // namespace

Base::Result<void> MetadataDescriptorStore::Build(
    const TypeRegistry& source) noexcept {
    if (sealed_) {
        return AlreadyBuilt("MetadataDescriptorStore is already sealed");
    }
    if (!source.IsFrozen()) {
        return SourceNotFrozen(
            "TypeRegistry must be frozen before building descriptors");
    }

    Base::Result<void> reserved = types_.TryReserve(source.TypeCount());
    if (!reserved) return reserved.GetStatus();
    reserved = typeIndex_.TryReserve(source.TypeCount());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t propertyCount = 0U;
    std::uint32_t eventCount = 0U;
    std::uint32_t methodCount = 0U;
    for (const TypeInfo& type : source.Types()) {
        propertyCount += type.Properties().Size();
        eventCount += type.Events().Size();
        methodCount += type.Methods().Size();
    }
    reserved = properties_.TryReserve(propertyCount);
    if (!reserved) return reserved.GetStatus();
    reserved = events_.TryReserve(eventCount);
    if (!reserved) return reserved.GetStatus();
    reserved = methods_.TryReserve(methodCount);
    if (!reserved) return reserved.GetStatus();
    reserved = memberIndex_.TryReserve(
        propertyCount + eventCount + methodCount);
    if (!reserved) return reserved.GetStatus();

    for (const TypeInfo& sourceType : source.Types()) {
        MetadataTypeDescriptor type;
        type.id_ = sourceType.Id();
        type.baseType_ = sourceType.BaseType();
        type.flags_ = sourceType.Flags();
        Base::Result<void> copied = type.xamlNamespace_.TryAssign(
            sourceType.XamlNamespace());
        if (!copied) return copied.GetStatus();
        copied = type.name_.TryAssign(sourceType.Name());
        if (!copied) return copied.GetStatus();
        copied = type.properties_.TryReserve(sourceType.Properties().Size());
        if (!copied) return copied.GetStatus();
        copied = type.events_.TryReserve(sourceType.Events().Size());
        if (!copied) return copied.GetStatus();
        copied = type.methods_.TryReserve(sourceType.Methods().Size());
        if (!copied) return copied.GetStatus();

        for (const PropertyInfo& sourceProperty : sourceType.Properties()) {
            MetadataPropertyDescriptor property;
            property.id_ = sourceProperty.Id();
            property.ownerType_ = sourceProperty.OwnerType();
            property.valueType_ = sourceProperty.ValueType();
            property.flags_ = sourceProperty.Flags();
            copied = property.name_.TryAssign(sourceProperty.Name());
            if (!copied) return copied.GetStatus();
            const std::uint32_t index = properties_.Size();
            copied = properties_.TryPushBack(std::move(property));
            if (!copied) return copied.GetStatus();
            copied = type.properties_.TryPushBack(sourceProperty.Id());
            if (!copied) return copied.GetStatus();
            copied = InsertMemberIndex(memberIndex_, sourceProperty.Id(),
                {MetadataDescriptorKind::Property, index});
            if (!copied) return copied.GetStatus();
        }

        for (const EventInfo& sourceEvent : sourceType.Events()) {
            MetadataEventDescriptor event;
            event.id_ = sourceEvent.Id();
            event.ownerType_ = sourceEvent.OwnerType();
            event.eventArgsType_ = sourceEvent.EventArgsType();
            event.flags_ = sourceEvent.Flags();
            copied = event.name_.TryAssign(sourceEvent.Name());
            if (!copied) return copied.GetStatus();
            const std::uint32_t index = events_.Size();
            copied = events_.TryPushBack(std::move(event));
            if (!copied) return copied.GetStatus();
            copied = type.events_.TryPushBack(sourceEvent.Id());
            if (!copied) return copied.GetStatus();
            copied = InsertMemberIndex(memberIndex_, sourceEvent.Id(),
                {MetadataDescriptorKind::Event, index});
            if (!copied) return copied.GetStatus();
        }

        for (const MethodInfo& sourceMethod : sourceType.Methods()) {
            MetadataMethodDescriptor method;
            method.id_ = sourceMethod.Id();
            method.ownerType_ = sourceMethod.OwnerType();
            method.returnType_ = sourceMethod.ReturnType();
            method.flags_ = sourceMethod.Flags();
            copied = method.name_.TryAssign(sourceMethod.Name());
            if (!copied) return copied.GetStatus();
            copied = method.parameters_.TryReserve(
                sourceMethod.Parameters().Size());
            if (!copied) return copied.GetStatus();
            for (const MethodParameterInfo& sourceParameter :
                 sourceMethod.Parameters()) {
                MetadataMethodParameterDescriptor parameter;
                parameter.type_ = sourceParameter.Type();
                copied = parameter.name_.TryAssign(sourceParameter.Name());
                if (!copied) return copied.GetStatus();
                copied = method.parameters_.TryPushBack(std::move(parameter));
                if (!copied) return copied.GetStatus();
            }
            const std::uint32_t index = methods_.Size();
            copied = methods_.TryPushBack(std::move(method));
            if (!copied) return copied.GetStatus();
            copied = type.methods_.TryPushBack(sourceMethod.Id());
            if (!copied) return copied.GetStatus();
            copied = InsertMemberIndex(memberIndex_, sourceMethod.Id(),
                {MetadataDescriptorKind::Method, index});
            if (!copied) return copied.GetStatus();
        }

        const std::uint32_t typeIndex = types_.Size();
        copied = types_.TryPushBack(std::move(type));
        if (!copied) return copied.GetStatus();
        copied = InsertTypeIndex(typeIndex_, sourceType.Id(), typeIndex);
        if (!copied) return copied.GetStatus();
    }

    sealed_ = true;
    return {};
}

const MetadataTypeDescriptor* MetadataDescriptorStore::FindType(
    TypeId id) const noexcept {
    const std::uint32_t* index = typeIndex_.Find(id);
    return index != nullptr && *index < types_.Size()
        ? &types_[*index] : nullptr;
}

const MetadataTypeDescriptor* MetadataDescriptorStore::FindType(
    Base::StringView xamlNamespace,
    Base::StringView name) const noexcept {
    const MetadataTypeDescriptor* type = FindType(
        MakeTypeId(xamlNamespace, name));
    return type != nullptr && type->XamlNamespace() == xamlNamespace &&
        type->Name() == name ? type : nullptr;
}

const MetadataPropertyDescriptor* MetadataDescriptorStore::FindProperty(
    MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr &&
        location->kind == MetadataDescriptorKind::Property &&
        location->index < properties_.Size()
        ? &properties_[location->index] : nullptr;
}

const MetadataPropertyDescriptor* MetadataDescriptorStore::FindProperty(
    TypeId ownerType,
    Base::StringView name,
    bool includeBaseTypes) const noexcept {
    TypeId current = ownerType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        const MetadataPropertyDescriptor* property = FindProperty(
            MakeMemberId(current, MemberKind::Property, name));
        if (property != nullptr && property->OwnerType() == current &&
            property->Name() == name) return property;
        if (!includeBaseTypes) return nullptr;
        const MetadataTypeDescriptor* type = FindType(current);
        if (type == nullptr) return nullptr;
        current = type->BaseType();
    }
    return nullptr;
}

const MetadataEventDescriptor* MetadataDescriptorStore::FindEvent(
    MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr &&
        location->kind == MetadataDescriptorKind::Event &&
        location->index < events_.Size()
        ? &events_[location->index] : nullptr;
}

const MetadataEventDescriptor* MetadataDescriptorStore::FindEvent(
    TypeId ownerType,
    Base::StringView name,
    bool includeBaseTypes) const noexcept {
    TypeId current = ownerType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        const MetadataEventDescriptor* event = FindEvent(
            MakeMemberId(current, MemberKind::Event, name));
        if (event != nullptr && event->OwnerType() == current &&
            event->Name() == name) return event;
        if (!includeBaseTypes) return nullptr;
        const MetadataTypeDescriptor* type = FindType(current);
        if (type == nullptr) return nullptr;
        current = type->BaseType();
    }
    return nullptr;
}

const MetadataMethodDescriptor* MetadataDescriptorStore::FindMethod(
    MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr &&
        location->kind == MetadataDescriptorKind::Method &&
        location->index < methods_.Size()
        ? &methods_[location->index] : nullptr;
}

const MetadataMethodDescriptor* MetadataDescriptorStore::FindMethod(
    TypeId ownerType,
    Base::StringView name,
    Base::Span<const TypeId> parameterTypes,
    bool includeBaseTypes) const noexcept {
    TypeId current = ownerType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        const MetadataMethodDescriptor* method = FindMethod(
            MakeMethodId(current, name, parameterTypes));
        if (method != nullptr && method->OwnerType() == current &&
            method->Name() == name) return method;
        if (!includeBaseTypes) return nullptr;
        const MetadataTypeDescriptor* type = FindType(current);
        if (type == nullptr) return nullptr;
        current = type->BaseType();
    }
    return nullptr;
}

bool MetadataDescriptorStore::IsDerivedFrom(
    TypeId type,
    TypeId expectedBase) const noexcept {
    if (type == InvalidTypeId || expectedBase == InvalidTypeId) return false;
    TypeId current = type;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        if (current == expectedBase) return true;
        const MetadataTypeDescriptor* descriptor = FindType(current);
        if (descriptor == nullptr) return false;
        current = descriptor->BaseType();
    }
    return false;
}

Base::Result<Base::HashCode> MetadataDescriptorStore::ComputeHash() const noexcept {
    if (!sealed_) {
        return SourceNotFrozen(
            "Descriptor hash requires a sealed descriptor store");
    }
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText("AERO.DESCRIPTORS.V1", 19U);
    builder.AddU32(MetadataDescriptorFormatVersion);

    Base::Vector<std::uint32_t> order;
    Base::Result<void> built = BuildOrder(types_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return types_[left].Id() < types_[right].Id();
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const MetadataTypeDescriptor& type = types_[index];
        builder.AddU64(type.Id());
        builder.AddU64(type.BaseType());
        builder.AddU32(static_cast<std::uint32_t>(type.Flags()));
        builder.AddString(type.XamlNamespace());
        builder.AddString(type.Name());
    }

    order.Clear();
    built = BuildOrder(properties_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return properties_[left].Id() < properties_[right].Id();
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const MetadataPropertyDescriptor& property = properties_[index];
        builder.AddU64(property.Id());
        builder.AddU64(property.OwnerType());
        builder.AddU64(property.ValueType());
        builder.AddU32(static_cast<std::uint32_t>(property.Flags()));
        builder.AddString(property.Name());
    }

    order.Clear();
    built = BuildOrder(events_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return events_[left].Id() < events_[right].Id();
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const MetadataEventDescriptor& event = events_[index];
        builder.AddU64(event.Id());
        builder.AddU64(event.OwnerType());
        builder.AddU64(event.EventArgsType());
        builder.AddU32(static_cast<std::uint32_t>(event.Flags()));
        builder.AddString(event.Name());
    }

    order.Clear();
    built = BuildOrder(methods_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return methods_[left].Id() < methods_[right].Id();
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const MetadataMethodDescriptor& method = methods_[index];
        builder.AddU64(method.Id());
        builder.AddU64(method.OwnerType());
        builder.AddU64(method.ReturnType());
        builder.AddU32(static_cast<std::uint32_t>(method.Flags()));
        builder.AddString(method.Name());
        builder.AddU32(method.Parameters().Size());
        for (const MetadataMethodParameterDescriptor& parameter :
             method.Parameters()) {
            builder.AddU64(parameter.Type());
            builder.AddString(parameter.Name());
        }
    }
    return builder.Finish();
}

Base::Result<void> MetadataFacetStore::AddTypeMask(
    TypeId type,
    MetadataFacetKind kind) noexcept {
    MetadataFacetMask* existing = typeMasks_.Find(type);
    if (existing != nullptr) {
        *existing |= MetadataFacetBit(kind);
        return {};
    }
    Base::Result<Base::HashMap<TypeId, MetadataFacetMask>::InsertResult> inserted =
        typeMasks_.TryInsert(type, MetadataFacetBit(kind));
    if (!inserted) return inserted.GetStatus();
    return inserted.Value().inserted ? Base::Result<void>()
        : Base::Result<void>(Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Type facet mask insertion failed"));
}

Base::Result<void> MetadataFacetStore::AddMemberMask(
    MemberId member,
    MetadataFacetKind kind) noexcept {
    MetadataFacetMask* existing = memberMasks_.Find(member);
    if (existing != nullptr) {
        *existing |= MetadataFacetBit(kind);
        return {};
    }
    Base::Result<Base::HashMap<MemberId, MetadataFacetMask>::InsertResult> inserted =
        memberMasks_.TryInsert(member, MetadataFacetBit(kind));
    if (!inserted) return inserted.GetStatus();
    return inserted.Value().inserted ? Base::Result<void>()
        : Base::Result<void>(Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Member facet mask insertion failed"));
}

Base::Result<void> MetadataFacetStore::Build(
    const TypeRegistry& source,
    const MetadataDescriptorStore& descriptors,
    const DependencyPropertyRegistry& dependencyProperties,
    const RoutedEventRegistry& routedEvents) noexcept {
    if (sealed_) {
        return AlreadyBuilt("MetadataFacetStore is already sealed");
    }
    if (!source.IsFrozen() || !descriptors.IsSealed() ||
        !dependencyProperties.IsFrozen() || !routedEvents.IsFrozen()) {
        return SourceNotFrozen(
            "Metadata registries and descriptors must be sealed before facets");
    }

    for (const TypeInfo& type : source.Types()) {
        Base::Result<void> status;
        if (type.Factory() != nullptr) {
            const std::uint32_t index = factories_.Size();
            status = factories_.TryPushBack({type.Id(), type.Factory()});
            if (!status) return status.GetStatus();
            status = InsertFacetIndex(factoryIndex_, type.Id(), index);
            if (!status) return status.GetStatus();
            status = AddTypeMask(type.Id(), MetadataFacetKind::TypeFactory);
            if (!status) return status.GetStatus();
        }
        if (type.ContentMember() != InvalidMemberId) {
            const std::uint32_t index = contents_.Size();
            status = contents_.TryPushBack({type.Id(), type.ContentMember()});
            if (!status) return status.GetStatus();
            status = InsertFacetIndex(contentIndex_, type.Id(), index);
            if (!status) return status.GetStatus();
            status = AddTypeMask(type.Id(), MetadataFacetKind::Content);
            if (!status) return status.GetStatus();
        }

        for (const PropertyInfo& property : type.Properties()) {
            if (property.Access() != PropertyAccessKind::External) {
                const std::uint32_t index = propertyAccessors_.Size();
                status = propertyAccessors_.TryPushBack({
                    property.Id(), property.Access(), property.Getter(),
                    property.Setter(), property.Provider(), property.Context()});
                if (!status) return status.GetStatus();
                status = InsertFacetIndex(
                    propertyAccessorIndex_, property.Id(), index);
                if (!status) return status.GetStatus();
                status = AddMemberMask(
                    property.Id(), MetadataFacetKind::PropertyAccessor);
                if (!status) return status.GetStatus();
            }

            const DependencyProperty* dependency = dependencyProperties.Find(
                DependencyPropertyHandle{property.Id()});
            if (dependency != nullptr) {
                const std::uint32_t index = dependencyProperties_.Size();
                status = dependencyProperties_.TryPushBack({
                    property.Id(), dependency->Handle().value,
                    dependency->RegisteredOwnerType(), dependency->ValueType(),
                    static_cast<std::uint32_t>(dependency->Flags()),
                    dependency->MetadataCount(), dependency});
                if (!status) return status.GetStatus();
                status = InsertFacetIndex(
                    dependencyPropertyIndex_, property.Id(), index);
                if (!status) return status.GetStatus();
                status = AddMemberMask(
                    property.Id(), MetadataFacetKind::DependencyProperty);
                if (!status) return status.GetStatus();
            }
        }

        for (const MethodInfo& method : type.Methods()) {
            if (method.Invoker() == nullptr) continue;
            const std::uint32_t index = methodInvokers_.Size();
            status = methodInvokers_.TryPushBack({
                method.Id(), method.Invoker(), method.Context()});
            if (!status) return status.GetStatus();
            status = InsertFacetIndex(methodInvokerIndex_, method.Id(), index);
            if (!status) return status.GetStatus();
            status = AddMemberMask(
                method.Id(), MetadataFacetKind::MethodInvoker);
            if (!status) return status.GetStatus();
        }

        for (const EventInfo& event : type.Events()) {
            if (!HasEventFlag(event.Flags(), EventFlags::Routed)) continue;
            const std::uint32_t index = routedEvents_.Size();
            status = routedEvents_.TryPushBack({
                event.Id(), event.OwnerType(), event.EventArgsType(),
                &routedEvents});
            if (!status) return status.GetStatus();
            status = InsertFacetIndex(routedEventIndex_, event.Id(), index);
            if (!status) return status.GetStatus();
            status = AddMemberMask(event.Id(), MetadataFacetKind::RoutedEvent);
            if (!status) return status.GetStatus();
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

MemberId MetadataFacetStore::FindContentMember(TypeId type) const noexcept {
    TypeId current = type;
    for (std::uint32_t depth = 0U; current != InvalidTypeId; ++depth) {
        const ContentFacet* facet = FindContent(current);
        if (facet != nullptr) return facet->member;
        if (depth > typeMasks_.Size() + contentIndex_.Size()) break;
        const MetadataFacetMask mask = TypeFacets(current);
        (void)mask;
        // The type hierarchy is resolved by callers through the descriptor store;
        // exact lookup remains allocation-free here.
        break;
    }
    return InvalidMemberId;
}

const PropertyAccessorFacet* MetadataFacetStore::FindPropertyAccessor(
    MemberId member) const noexcept {
    const std::uint32_t* index = propertyAccessorIndex_.Find(member);
    return index != nullptr && *index < propertyAccessors_.Size()
        ? &propertyAccessors_[*index] : nullptr;
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

Base::Result<Base::HashCode> MetadataFacetStore::ComputeHash() const noexcept {
    if (!sealed_) {
        return SourceNotFrozen("Facet hash requires a sealed facet store");
    }
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText("AERO.FACETS.V1", 14U);
    builder.AddU32(MetadataFacetFormatVersion);

    Base::Vector<std::uint32_t> order;
    Base::Result<void> built = BuildOrder(factories_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return factories_[left].type < factories_[right].type;
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        builder.AddByte(static_cast<std::uint8_t>(MetadataFacetKind::TypeFactory));
        builder.AddU64(factories_[index].type);
    }

    order.Clear();
    built = BuildOrder(contents_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return contents_[left].type < contents_[right].type;
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        builder.AddByte(static_cast<std::uint8_t>(MetadataFacetKind::Content));
        builder.AddU64(contents_[index].type);
        builder.AddU64(contents_[index].member);
    }

    order.Clear();
    built = BuildOrder(propertyAccessors_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return propertyAccessors_[left].member <
                propertyAccessors_[right].member;
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const PropertyAccessorFacet& facet = propertyAccessors_[index];
        builder.AddByte(static_cast<std::uint8_t>(
            MetadataFacetKind::PropertyAccessor));
        builder.AddU64(facet.member);
        builder.AddByte(static_cast<std::uint8_t>(facet.access));
        builder.AddU64(facet.provider);
        builder.AddByte(facet.get != nullptr ? 1U : 0U);
        builder.AddByte(facet.set != nullptr ? 1U : 0U);
    }

    order.Clear();
    built = BuildOrder(methodInvokers_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return methodInvokers_[left].member < methodInvokers_[right].member;
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        builder.AddByte(static_cast<std::uint8_t>(
            MetadataFacetKind::MethodInvoker));
        builder.AddU64(methodInvokers_[index].member);
    }

    order.Clear();
    built = BuildOrder(dependencyProperties_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return dependencyProperties_[left].member <
                dependencyProperties_[right].member;
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const DependencyPropertyFacet& facet = dependencyProperties_[index];
        builder.AddByte(static_cast<std::uint8_t>(
            MetadataFacetKind::DependencyProperty));
        builder.AddU64(facet.member);
        builder.AddU64(facet.canonicalMember);
        builder.AddU64(facet.registeredOwnerType);
        builder.AddU64(facet.valueType);
        builder.AddU32(facet.flags);
        builder.AddU32(facet.metadataCount);
    }

    order.Clear();
    built = BuildOrder(routedEvents_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return routedEvents_[left].member < routedEvents_[right].member;
        });
    if (!built) return built.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const RoutedEventFacet& facet = routedEvents_[index];
        builder.AddByte(static_cast<std::uint8_t>(MetadataFacetKind::RoutedEvent));
        builder.AddU64(facet.member);
        builder.AddU64(facet.ownerType);
        builder.AddU64(facet.eventArgsType);
    }

    return builder.Finish();
}

} // namespace Aero::Core
