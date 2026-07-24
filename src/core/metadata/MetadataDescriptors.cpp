#include <Aero/Core/Metadata/MetadataDescriptors.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>

#include <Aero/Core/Property/DependencyProperty.hpp>

#include <utility>

namespace Aero::Core {
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

bool InterfaceReachable(
    const MetadataDescriptorStore& store,
    TypeId current,
    TypeId target,
    std::uint32_t depth) noexcept {
    if (current == target) return true;
    if (depth > store.TypeCount()) return false;
    const MetadataTypeDescriptor* info = store.FindType(current);
    if (info == nullptr) return false;
    for (TypeId direct : info->Interfaces()) {
        if (InterfaceReachable(store, direct, target, depth + 1U)) {
            return true;
        }
    }
    return false;
}

bool TypeImplements(
    const MetadataDescriptorStore& store,
    TypeId current,
    TypeId target,
    std::uint32_t depth) noexcept {
    if (current == InvalidTypeId || depth > store.TypeCount()) return false;
    const MetadataTypeDescriptor* info = store.FindType(current);
    if (info == nullptr) return false;
    for (TypeId direct : info->Interfaces()) {
        if (direct == target ||
            InterfaceReachable(store, direct, target, depth + 1U)) {
            return true;
        }
    }
    return TypeImplements(store, info->BaseType(), target, depth + 1U);
}

} // namespace

Base::Result<void> MetadataDescriptorStore::Build(
    const TypeRegistry& source) noexcept {
    if (sealed_) {
        return InvalidState("MetadataDescriptorStore is already sealed");
    }
    if (!source.IsFrozen()) {
        return InvalidState(
            "TypeRegistry must be frozen before building descriptors");
    }

    std::uint32_t propertyCount = 0U;
    std::uint32_t fieldCount = 0U;
    std::uint32_t enumValueCount = 0U;
    std::uint32_t eventCount = 0U;
    std::uint32_t methodCount = 0U;
    for (const TypeInfo& type : source.Types()) {
        propertyCount += type.Properties().Size();
        fieldCount += type.Fields().Size();
        enumValueCount += type.EnumValues().Size();
        eventCount += type.Events().Size();
        methodCount += type.Methods().Size();
    }

    Base::Result<void> result = types_.TryReserve(source.TypeCount());
    if (!result) return result.GetStatus();
    result = typeIndex_.TryReserve(source.TypeCount());
    if (!result) return result.GetStatus();
    result = properties_.TryReserve(propertyCount);
    if (!result) return result.GetStatus();
    result = fields_.TryReserve(fieldCount);
    if (!result) return result.GetStatus();
    result = enumValues_.TryReserve(enumValueCount);
    if (!result) return result.GetStatus();
    result = events_.TryReserve(eventCount);
    if (!result) return result.GetStatus();
    result = methods_.TryReserve(methodCount);
    if (!result) return result.GetStatus();
    result = memberIndex_.TryReserve(
        propertyCount + fieldCount + enumValueCount +
        eventCount + methodCount);
    if (!result) return result.GetStatus();

    for (const TypeInfo& sourceType : source.Types()) {
        MetadataTypeDescriptor type;
        type.id_ = sourceType.Id();
        type.baseType_ = sourceType.BaseType();
        type.underlyingType_ = sourceType.UnderlyingType();
        type.kind_ = sourceType.Kind();
        type.flags_ = sourceType.Flags();
        result = type.xamlNamespace_.TryAssign(sourceType.XamlNamespace());
        if (!result) return result.GetStatus();
        result = type.name_.TryAssign(sourceType.Name());
        if (!result) return result.GetStatus();
        result = type.interfaces_.TryAssign(sourceType.Interfaces());
        if (!result) return result.GetStatus();
        result = type.properties_.TryReserve(sourceType.Properties().Size());
        if (!result) return result.GetStatus();
        result = type.fields_.TryReserve(sourceType.Fields().Size());
        if (!result) return result.GetStatus();
        result = type.enumValues_.TryReserve(sourceType.EnumValues().Size());
        if (!result) return result.GetStatus();
        result = type.events_.TryReserve(sourceType.Events().Size());
        if (!result) return result.GetStatus();
        result = type.methods_.TryReserve(sourceType.Methods().Size());
        if (!result) return result.GetStatus();

        for (const PropertyInfo& sourceProperty : sourceType.Properties()) {
            MetadataPropertyDescriptor property;
            property.id_ = sourceProperty.Id();
            property.ownerType_ = sourceProperty.OwnerType();
            property.valueType_ = sourceProperty.ValueType();
            property.flags_ = sourceProperty.Flags();
            result = property.name_.TryAssign(sourceProperty.Name());
            if (!result) return result.GetStatus();
            const std::uint32_t index = properties_.Size();
            result = properties_.TryPushBack(std::move(property));
            if (!result) return result.GetStatus();
            result = type.properties_.TryPushBack(sourceProperty.Id());
            if (!result) return result.GetStatus();
            result = InsertUnique(memberIndex_, sourceProperty.Id(),
                MemberLocation{MetadataDescriptorKind::Property, index},
                "Metadata descriptor member id collision");
            if (!result) return result.GetStatus();
        }

        for (const FieldInfo& sourceField : sourceType.Fields()) {
            MetadataFieldDescriptor field;
            field.id_ = sourceField.Id();
            field.ownerType_ = sourceField.OwnerType();
            field.valueType_ = sourceField.ValueType();
            field.flags_ = sourceField.Flags();
            result = field.name_.TryAssign(sourceField.Name());
            if (!result) return result.GetStatus();
            const std::uint32_t index = fields_.Size();
            result = fields_.TryPushBack(std::move(field));
            if (!result) return result.GetStatus();
            result = type.fields_.TryPushBack(sourceField.Id());
            if (!result) return result.GetStatus();
            result = InsertUnique(memberIndex_, sourceField.Id(),
                MemberLocation{MetadataDescriptorKind::Field, index},
                "Metadata descriptor field id collision");
            if (!result) return result.GetStatus();
        }

        for (const EnumValueInfo& sourceValue : sourceType.EnumValues()) {
            MetadataEnumValueDescriptor value;
            value.id_ = sourceValue.Id();
            value.ownerType_ = sourceValue.OwnerType();
            value.rawValue_ = sourceValue.RawValue();
            result = value.name_.TryAssign(sourceValue.Name());
            if (!result) return result.GetStatus();
            const std::uint32_t index = enumValues_.Size();
            result = enumValues_.TryPushBack(std::move(value));
            if (!result) return result.GetStatus();
            result = type.enumValues_.TryPushBack(sourceValue.Id());
            if (!result) return result.GetStatus();
            result = InsertUnique(memberIndex_, sourceValue.Id(),
                MemberLocation{MetadataDescriptorKind::EnumValue, index},
                "Metadata descriptor enum value id collision");
            if (!result) return result.GetStatus();
        }

        for (const EventInfo& sourceEvent : sourceType.Events()) {
            MetadataEventDescriptor event;
            event.id_ = sourceEvent.Id();
            event.ownerType_ = sourceEvent.OwnerType();
            event.eventArgsType_ = sourceEvent.EventArgsType();
            event.flags_ = sourceEvent.Flags();
            result = event.name_.TryAssign(sourceEvent.Name());
            if (!result) return result.GetStatus();
            const std::uint32_t index = events_.Size();
            result = events_.TryPushBack(std::move(event));
            if (!result) return result.GetStatus();
            result = type.events_.TryPushBack(sourceEvent.Id());
            if (!result) return result.GetStatus();
            result = InsertUnique(memberIndex_, sourceEvent.Id(),
                MemberLocation{MetadataDescriptorKind::Event, index},
                "Metadata descriptor member id collision");
            if (!result) return result.GetStatus();
        }

        for (const MethodInfo& sourceMethod : sourceType.Methods()) {
            MetadataMethodDescriptor method;
            method.id_ = sourceMethod.Id();
            method.ownerType_ = sourceMethod.OwnerType();
            method.returnType_ = sourceMethod.ReturnType();
            method.flags_ = sourceMethod.Flags();
            result = method.name_.TryAssign(sourceMethod.Name());
            if (!result) return result.GetStatus();
            result = method.parameters_.TryReserve(
                sourceMethod.Parameters().Size());
            if (!result) return result.GetStatus();
            for (const MethodParameterInfo& sourceParameter :
                 sourceMethod.Parameters()) {
                MetadataMethodParameterDescriptor parameter;
                parameter.type_ = sourceParameter.Type();
                result = parameter.name_.TryAssign(sourceParameter.Name());
                if (!result) return result.GetStatus();
                result = method.parameters_.TryPushBack(std::move(parameter));
                if (!result) return result.GetStatus();
            }
            const std::uint32_t index = methods_.Size();
            result = methods_.TryPushBack(std::move(method));
            if (!result) return result.GetStatus();
            result = type.methods_.TryPushBack(sourceMethod.Id());
            if (!result) return result.GetStatus();
            result = InsertUnique(memberIndex_, sourceMethod.Id(),
                MemberLocation{MetadataDescriptorKind::Method, index},
                "Metadata descriptor member id collision");
            if (!result) return result.GetStatus();
        }

        const std::uint32_t index = types_.Size();
        result = types_.TryPushBack(std::move(type));
        if (!result) return result.GetStatus();
        result = InsertUnique(typeIndex_, sourceType.Id(), index,
            "Metadata descriptor type id collision");
        if (!result) return result.GetStatus();
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
        if (property != nullptr && property->Name() == name) return property;
        if (!includeBaseTypes) return nullptr;
        const MetadataTypeDescriptor* type = FindType(current);
        if (type == nullptr) return nullptr;
        current = type->BaseType();
    }
    return nullptr;
}

const MetadataFieldDescriptor* MetadataDescriptorStore::FindField(
    MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr &&
        location->kind == MetadataDescriptorKind::Field &&
        location->index < fields_.Size()
        ? &fields_[location->index] : nullptr;
}

const MetadataFieldDescriptor* MetadataDescriptorStore::FindField(
    TypeId ownerType,
    Base::StringView name) const noexcept {
    const MetadataFieldDescriptor* field = FindField(
        MakeMemberId(ownerType, MemberKind::Field, name));
    return field != nullptr && field->Name() == name ? field : nullptr;
}

const MetadataEnumValueDescriptor* MetadataDescriptorStore::FindEnumValue(
    MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr &&
        location->kind == MetadataDescriptorKind::EnumValue &&
        location->index < enumValues_.Size()
        ? &enumValues_[location->index] : nullptr;
}

const MetadataEnumValueDescriptor* MetadataDescriptorStore::FindEnumValue(
    TypeId ownerType,
    Base::StringView name) const noexcept {
    const MetadataEnumValueDescriptor* value = FindEnumValue(
        MakeMemberId(ownerType, MemberKind::EnumValue, name));
    return value != nullptr && value->Name() == name ? value : nullptr;
}

const MetadataEnumValueDescriptor* MetadataDescriptorStore::FindEnumValue(
    TypeId ownerType,
    std::uint64_t rawValue) const noexcept {
    const MetadataTypeDescriptor* type = FindType(ownerType);
    if (type == nullptr) return nullptr;
    for (MemberId member : type->EnumValues()) {
        const MetadataEnumValueDescriptor* value = FindEnumValue(member);
        if (value != nullptr && value->RawValue() == rawValue) return value;
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
        if (event != nullptr && event->Name() == name) return event;
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
        if (method != nullptr && method->Name() == name) return method;
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

bool MetadataDescriptorStore::Implements(
    TypeId type,
    TypeId interfaceType) const noexcept {
    const MetadataTypeDescriptor* target = FindType(interfaceType);
    return target != nullptr &&
        target->Kind() == MetadataTypeKind::Interface &&
        TypeImplements(*this, type, interfaceType, 0U);
}

bool MetadataDescriptorStore::IsAssignableFrom(
    TypeId targetType,
    TypeId sourceType) const noexcept {
    if (targetType == InvalidTypeId || sourceType == InvalidTypeId) return false;
    if (targetType == sourceType) return true;
    const MetadataTypeDescriptor* target = FindType(targetType);
    if (target == nullptr) return false;
    return target->Kind() == MetadataTypeKind::Interface
        ? Implements(sourceType, targetType)
        : IsDerivedFrom(sourceType, targetType);
}

Base::Result<Base::HashCode> MetadataDescriptorStore::ComputeHash() const noexcept {
    if (!sealed_) {
        return InvalidState("Descriptor hash requires a sealed store");
    }
    Base::Detail::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.DESCRIPTORS.V2";
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(MetadataDescriptorFormatVersion);

    Base::Vector<std::uint32_t> order;
    Base::Result<void> result = BuildOrder(types_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return types_[left].Id() < types_[right].Id();
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const MetadataTypeDescriptor& type = types_[index];
        builder.AddU64(type.Id());
        builder.AddU64(type.BaseType());
        builder.AddU64(type.UnderlyingType());
        builder.AddByte(static_cast<std::uint8_t>(type.Kind()));
        builder.AddU32(static_cast<std::uint32_t>(type.Flags()));
        builder.AddString(type.XamlNamespace());
        builder.AddString(type.Name());
        Base::Vector<std::uint32_t> interfaceOrder;
        result = BuildOrder(type.Interfaces().Size(), interfaceOrder,
            [&type](std::uint32_t left, std::uint32_t right) noexcept {
                return type.Interfaces()[left] < type.Interfaces()[right];
            });
        if (!result) return result.GetStatus();
        builder.AddU32(interfaceOrder.Size());
        for (std::uint32_t interfaceIndex : interfaceOrder) {
            builder.AddU64(type.Interfaces()[interfaceIndex]);
        }
    }

    order.Clear();
    result = BuildOrder(properties_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return properties_[left].Id() < properties_[right].Id();
        });
    if (!result) return result.GetStatus();
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
    result = BuildOrder(fields_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return fields_[left].Id() < fields_[right].Id();
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const MetadataFieldDescriptor& field = fields_[index];
        builder.AddU64(field.Id());
        builder.AddU64(field.OwnerType());
        builder.AddU64(field.ValueType());
        builder.AddU32(static_cast<std::uint32_t>(field.Flags()));
        builder.AddString(field.Name());
    }

    order.Clear();
    result = BuildOrder(enumValues_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return enumValues_[left].Id() < enumValues_[right].Id();
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const MetadataEnumValueDescriptor& value = enumValues_[index];
        builder.AddU64(value.Id());
        builder.AddU64(value.OwnerType());
        builder.AddU64(value.RawValue());
        builder.AddString(value.Name());
    }

    order.Clear();
    result = BuildOrder(events_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return events_[left].Id() < events_[right].Id();
        });
    if (!result) return result.GetStatus();
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
    result = BuildOrder(methods_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return methods_[left].Id() < methods_[right].Id();
        });
    if (!result) return result.GetStatus();
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
    const TypeRegistry& source,
    const MetadataBehaviorRegistrationStore& behaviors,
    const MetadataDescriptorStore& descriptors,
    const DependencyPropertyRegistry& dependencyProperties,
    const RoutedEventCatalog& routedEvents) noexcept {
    if (sealed_) return InvalidState("MetadataFacetStore is already sealed");
    if (!source.IsFrozen() || !behaviors.IsFrozen() || !descriptors.IsSealed() ||
        !dependencyProperties.IsFrozen() || !routedEvents.IsFrozen()) {
        return InvalidState(
            "Metadata sources must be sealed before building facets");
    }
    descriptors_ = &descriptors;

    for (const TypeInfo& type : source.Types()) {
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
            const std::uint32_t index = contents_.Size();
            result = contents_.TryPushBack({type.Id(), type.ContentMember()});
            if (!result) return result.GetStatus();
            result = InsertUnique(contentIndex_, type.Id(), index,
                "Content facet collision");
            if (!result) return result.GetStatus();
            result = AddTypeMask(type.Id(), MetadataFacetKind::Content);
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

MemberId MetadataFacetStore::FindContentMember(TypeId type) const noexcept {
    if (descriptors_ == nullptr) return InvalidMemberId;
    TypeId current = type;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= descriptors_->TypeCount(); ++depth) {
        const ContentFacet* content = FindContent(current);
        if (content != nullptr) return content->member;
        const MetadataTypeDescriptor* descriptor = descriptors_->FindType(current);
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
    if (descriptors_ == nullptr) return nullptr;
    const auto find = [this](
        const auto& self,
        TypeId current,
        std::uint32_t depth) noexcept
        -> const PropertyChangeNotificationFacet* {
        if (current == InvalidTypeId ||
            depth > descriptors_->TypeCount()) {
            return nullptr;
        }
        const std::uint32_t* index =
            propertyChangeNotificationIndex_.Find(current);
        if (index != nullptr &&
            *index < propertyChangeNotifications_.Size()) {
            return &propertyChangeNotifications_[*index];
        }
        const MetadataTypeDescriptor* descriptor =
            descriptors_->FindType(current);
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
    if (descriptors_ == nullptr) return nullptr;
    const auto find = [this](
        const auto& self,
        TypeId current,
        std::uint32_t depth) noexcept
        -> const CollectionChangeNotificationFacet* {
        if (current == InvalidTypeId ||
            depth > descriptors_->TypeCount()) {
            return nullptr;
        }
        const std::uint32_t* index =
            collectionChangeNotificationIndex_.Find(current);
        if (index != nullptr &&
            *index < collectionChangeNotifications_.Size()) {
            return &collectionChangeNotifications_[*index];
        }
        const MetadataTypeDescriptor* descriptor =
            descriptors_->FindType(current);
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
        builder.AddU64(contents_[index].type);
        builder.AddU64(contents_[index].member);
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

} // namespace Aero::Core
