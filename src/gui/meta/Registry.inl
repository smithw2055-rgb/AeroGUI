// ===== Registry =====



#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>

#include <new>

namespace Aero::Meta {

struct Registry::Storage {
    struct ModuleRecord {
        MetadataModuleId id = InvalidMetadataModuleId;
        std::uint32_t schemaVersion = 1U;
        MetadataModuleRegisterCallback registerModule = nullptr;
        MetadataModuleRegisterContextCallback registerModuleWithContext =
            nullptr;
        void* context = nullptr;
        Base::String name;
    };

    TypeRegistry types;
    BehaviorTable behaviorRegistrations;
    ValueTable valueRegistrations;
    DependencyPropertyRegistry dependencyProperties;
    RoutedEventTable routedEvents;
    ::Aero::MetaTable facets;
    Base::Vector<ModuleRecord> modules;
    Base::Vector<MetadataPropertyProviderRegistration> providers;
    bool sealed = false;
    bool ready = false;

    Storage() noexcept
        : types(),
          behaviorRegistrations(types),
          valueRegistrations(types),
          dependencyProperties(types, behaviorRegistrations),
          routedEvents(types, behaviorRegistrations),
          facets(),
          modules(),
          providers() {}
};

Registry::Registry() noexcept
    : storage_(new (std::nothrow) Storage()) {}

Registry::~Registry() noexcept {
    delete storage_;
    storage_ = nullptr;
}

Base::Status Registry::OutOfMemoryStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory,
        "Registry storage allocation failed");
}

bool Registry::IsValid() const noexcept {
    return storage_ != nullptr;
}

bool Registry::IsSealed() const noexcept {
    return storage_ != nullptr && storage_->sealed;
}

std::uint32_t Registry::ModuleCount() const noexcept {
    return storage_ != nullptr ? storage_->modules.Size() : 0U;
}

Base::Result<void> Registry::ValidateRegistration(
    const MetadataModuleRegistration& registration) noexcept {
    if (registration.id == InvalidMetadataModuleId ||
        registration.name.Empty() || registration.schemaVersion == 0U ||
        (registration.registerModule == nullptr &&
         registration.registerModuleWithContext == nullptr) ||
        (registration.registerModule != nullptr &&
         registration.registerModuleWithContext != nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata module registration is incomplete");
    }
    if (registration.id != MakeMetadataModuleId(registration.name)) {
        return Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Metadata module id does not match its stable name");
    }
    return {};
}

Base::Result<Registry::Storage*> Registry::BuildCandidate(
    const MetadataModuleRegistration* extra,
    bool seal) const noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();

    Storage* candidate = new (std::nothrow) Storage();
    if (candidate == nullptr) return OutOfMemoryStatus();

    auto fail = [candidate](const Base::Status& status) noexcept
        -> Base::Result<Storage*> {
        delete candidate;
        return status;
    };
    RegistrationTypes registrations(
        candidate->types,
        candidate->behaviorRegistrations);

    // Clone the committed authoring state directly. Module callbacks are
    // intentionally not replayed: each callback executes exactly once and a
    // failed candidate leaves the current storage untouched.
    for (const TypeInfo& type : storage_->types.Types()) {
        const TypeFactoryRegistration* factory =
            storage_->behaviorRegistrations.FindTypeFactory(type.Id());
        const TypeRegistration registration{
            type.XamlNamespace(),
            type.Name(),
            type.BaseType(),
            type.Flags(),
            factory != nullptr ? factory->factory : nullptr,
            type.Kind(),
            type.UnderlyingType(),
            type.Interfaces()};
        Base::Result<TypeId> registered =
            registrations.RegisterType(registration);
        if (!registered) return fail(registered.GetStatus());
    }

    // Enum values are structural prerequisites for dependency-property
    // default-value validation, so restore them before cloning properties.
    for (const TypeInfo& type : storage_->types.Types()) {
        for (const EnumValueInfo& value : type.EnumValues()) {
            Base::Result<MemberId> registered =
                registrations.RegisterEnumValue(type.Id(), {
                    value.Name(), value.RawValue()});
            if (!registered) return fail(registered.GetStatus());
        }
    }

    // Value semantics are prerequisites for cloning any property default or
    // behavior payload that stores a non-inline value.  Restore them before
    // property registrations so a second registry candidate observes the
    // same value codec domain as the committed registry.
    for (const ValueTable::ValueSemanticsEntry& semantics :
         storage_->valueRegistrations.valueSemantics_) {
        Base::Result<void> cloned =
            candidate->valueRegistrations.RegisterValueSemantics(
                semantics.type,
                semantics.semantics->Registration());
        if (!cloned) return fail(cloned.GetStatus());
    }
    for (const TextValueConverterRegistration& converter :
         storage_->valueRegistrations.textConverters_) {
        Base::Result<void> cloned =
            candidate->valueRegistrations.RegisterTextConverter(converter);
        if (!cloned) return fail(cloned.GetStatus());
    }

    // Dependency properties create both their catalog entry and structural
    // property descriptor, so they are cloned before ordinary members.
    for (const Meta::DependencyProperty& property :
         storage_->dependencyProperties.properties_) {
        const Meta::DependencyProperty::MetadataEntry* ownerMetadata = nullptr;
        for (const Meta::DependencyProperty::MetadataEntry& entry :
             property.metadata_) {
            if (entry.owner &&
                entry.forType == property.registeredOwnerType_) {
                ownerMetadata = &entry;
                break;
            }
        }
        if (ownerMetadata == nullptr) {
            return fail(Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Dependency property clone is missing owner metadata"));
        }
        Base::Result<DependencyPropertyRegistrationResult> registered =
            candidate->dependencyProperties.Register({
                property.Name(),
                property.registeredOwnerType_,
                property.valueType_,
                property.flags_,
                ownerMetadata->metadata});
        if (!registered) return fail(registered.GetStatus());
        const DependencyPropertyHandle handle = registered.Value().property;
        for (const Meta::DependencyProperty::MetadataEntry& entry :
             property.metadata_) {
            if (&entry == ownerMetadata) continue;
            Base::Result<void> metadata = entry.owner
                ? candidate->dependencyProperties.AddOwner(
                      handle, entry.forType, entry.metadata)
                : candidate->dependencyProperties.OverrideMetadata(
                      handle, entry.forType, entry.metadata);
            if (!metadata) return fail(metadata.GetStatus());
        }
    }

    // Routed-event registration likewise owns the structural event record.
    for (const RoutedEventTable::Definition& definition :
         storage_->routedEvents.definitions_) {
        Base::Result<RoutedEventHandle> registered =
            candidate->routedEvents.Register({
                definition.name.View(),
                definition.ownerType,
                definition.eventArgsType,
                definition.strategy});
        if (!registered) return fail(registered.GetStatus());
    }

    for (const TypeInfo& type : storage_->types.Types()) {
        for (const PropertyInfo& property : type.Properties()) {
            if (candidate->types.FindProperty(property.Id()) != nullptr) {
                continue;
            }
            const PropertyAccessorRegistration* accessor =
                storage_->behaviorRegistrations.FindPropertyAccessor(
                    property.Id());
            PropertyRegistration registration;
            registration.name = property.Name();
            registration.valueType = property.ValueType();
            registration.flags = property.Flags();
            if (accessor != nullptr) {
                registration.access = accessor->access;
                registration.get = accessor->get;
                registration.set = accessor->set;
                registration.provider = accessor->provider;
                registration.context = accessor->context;
            }
            Base::Result<MemberId> registered =
                registrations.RegisterProperty(type.Id(), registration);
            if (!registered) return fail(registered.GetStatus());
        }
        for (const EventInfo& event : type.Events()) {
            if (candidate->types.FindEvent(event.Id()) != nullptr) continue;
            Base::Result<MemberId> registered =
                registrations.RegisterEvent(type.Id(), {
                    event.Name(), event.EventArgsType(), event.Flags()});
            if (!registered) return fail(registered.GetStatus());
        }
        for (const EventHandlerDescriptor& handler : type.EventHandlers()) {
            Base::Result<void> registered =
                registrations.RegisterEventHandler(
                    type.Id(), handler.name.View(), handler.thunk);
            if (!registered) return fail(registered.GetStatus());
        }
    }

    for (const TypeInfo& type : storage_->types.Types()) {
        if (type.ContentMember() == InvalidMemberId) continue;
        Base::Result<void> content = registrations.SetContentMember(
            type.Id(), type.ContentMember());
        if (!content) return fail(content.GetStatus());
    }
    for (const ContentAccessorRegistration& content :
         storage_->behaviorRegistrations.contentAccessors_) {
        Base::Result<void> cloned =
            registrations.SetContentAccessor(content);
        if (!cloned) return fail(cloned.GetStatus());
    }
    for (const PropertyChangeNotificationRegistration& notification :
         storage_->behaviorRegistrations.propertyChangeNotifications_) {
        Base::Result<void> cloned =
            registrations.RegisterPropertyChangeNotification(notification);
        if (!cloned) return fail(cloned.GetStatus());
    }
    for (const CollectionChangeNotificationRegistration& notification :
         storage_->behaviorRegistrations.collectionChangeNotifications_) {
        Base::Result<void> cloned =
            registrations.RegisterCollectionChangeNotification(
                notification);
        if (!cloned) return fail(cloned.GetStatus());
    }

    for (const Storage::ModuleRecord& source : storage_->modules) {
        Storage::ModuleRecord record;
        record.id = source.id;
        record.schemaVersion = source.schemaVersion;
        Base::Result<void> assigned = record.name.Assign(source.name.View());
        if (!assigned) return fail(assigned.GetStatus());
        Base::Result<void> added =
            candidate->modules.PushBack(std::move(record));
        if (!added) return fail(added.GetStatus());
    }

    auto applyAndAppend = [candidate](
        const MetadataModuleRegistration& registration) noexcept
        -> Base::Result<void> {
        ::Aero::RegistrationState contextState{
            &candidate->types,
            &candidate->behaviorRegistrations,
            &candidate->valueRegistrations,
            &candidate->dependencyProperties,
            &candidate->routedEvents};
        Registration context(&contextState);
        Base::Result<void> applied = registration.registerModule != nullptr
            ? registration.registerModule(context)
            : registration.registerModuleWithContext(
                  context, registration.context);
        if (!applied) return applied.GetStatus();

        Storage::ModuleRecord record;
        record.id = registration.id;
        record.schemaVersion = registration.schemaVersion;
        Base::Result<void> assigned = record.name.Assign(registration.name);
        if (!assigned) return assigned.GetStatus();
        return candidate->modules.PushBack(std::move(record));
    };

    if (extra != nullptr) {
        Base::Result<void> applied = applyAndAppend(*extra);
        if (!applied) return fail(applied.GetStatus());
    }

    if (seal) {
        Base::Result<void> frozen = candidate->types.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->behaviorRegistrations.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->valueRegistrations.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->dependencyProperties.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->routedEvents.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->facets.Build(
            candidate->types,
            candidate->behaviorRegistrations,
            candidate->dependencyProperties,
            candidate->routedEvents);
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->facets.BuildValueFacets(
            candidate->valueRegistrations,
            candidate->types);
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        candidate->sealed = true;
    }

    return candidate;
}

Base::Result<void> Registry::RegisterModule(
    const MetadataModuleRegistration& registration) noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry is sealed");
    }
    Base::Result<void> valid = ValidateRegistration(registration);
    if (!valid) return valid.GetStatus();
    for (const Storage::ModuleRecord& module : storage_->modules) {
        if (module.id == registration.id) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata module is already registered");
        }
    }

    // Modules are accumulated only while the registry is mutable. Applying
    // directly keeps behavior contexts owned by their registering table and
    // avoids rebuilding the value-semantic store once per module. Seal() then
    // freezes this single registry into the immutable runtime snapshot.
    ::Aero::RegistrationState contextState{
        &storage_->types,
        &storage_->behaviorRegistrations,
        &storage_->valueRegistrations,
        &storage_->dependencyProperties,
        &storage_->routedEvents};
    Registration context(&contextState);
    Base::Result<void> applied = registration.registerModule != nullptr
        ? registration.registerModule(context)
        : registration.registerModuleWithContext(
              context, registration.context);
    if (!applied) return applied.GetStatus();

    Storage::ModuleRecord record;
    record.id = registration.id;
    record.schemaVersion = registration.schemaVersion;
    Base::Result<void> assigned = record.name.Assign(registration.name);
    if (!assigned) return assigned.GetStatus();
    return storage_->modules.PushBack(std::move(record));
}

Base::Result<void> Registry::Seal() noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) return {};
    Base::Result<void> frozen = storage_->types.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->behaviorRegistrations.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->valueRegistrations.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->dependencyProperties.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->routedEvents.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->facets.Build(
        storage_->types,
        storage_->behaviorRegistrations,
        storage_->dependencyProperties,
        storage_->routedEvents);
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->facets.BuildValueFacets(
        storage_->valueRegistrations, storage_->types);
    if (!frozen) return frozen.GetStatus();
    storage_->sealed = true;
    return {};
}

const TypeRegistry& Registry::Types() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->types;
}

DependencyPropertyRegistry& Registry::DependencyProperties() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

const DependencyPropertyRegistry& Registry::DependencyProperties() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

void* Registry::RoutedEventState() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return &storage_->routedEvents;
}

const ::Aero::MetaTable& Registry::RuntimeData() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->facets;
}

Base::Result<Base::HashCode> Registry::ComputeSchemaHash() const noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (!storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry schema hash requires a sealed domain");
    }

    Base::Result<Base::HashCode> descriptorHash =
        storage_->types.ComputeHash();
    if (!descriptorHash) return descriptorHash.GetStatus();
    Base::Result<Base::HashCode> facetHash = storage_->facets.ComputeHash();
    if (!facetHash) return facetHash.GetStatus();
    Base::Result<Base::HashCode> valueFacetHash =
        ::Aero::ComputeMetadataValueFacetHash(
            storage_->facets, storage_->types);
    if (!valueFacetHash) return valueFacetHash.GetStatus();

    Base::HashCode hash = Base::MixHash64(
        descriptorHash.Value() ^ facetHash.Value());
    hash = Base::MixHash64(hash ^ valueFacetHash.Value());
    for (const Storage::ModuleRecord& module : storage_->modules) {
        hash = Base::MixHash64(hash ^ module.id);
        hash = Base::MixHash64(
            hash ^ static_cast<Base::HashCode>(module.schemaVersion));
    }
    return hash;
}

} // namespace Aero::Meta

// Executable metadata operations share the Registry storage and lifetime.

namespace Aero::Meta {

Base::Result<void> Registry::RegisterPropertyProvider(
    const MetadataPropertyProviderRegistration& registration) noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->ready) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry is complete");
    }
    if (registration.id == InvalidPropertyProviderId ||
        registration.objectType == InvalidTypeId ||
        (registration.get == nullptr && registration.set == nullptr) ||
        !IsSealed() ||
        Types().FindType(registration.objectType) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property provider registration is invalid");
    }
    if (FindProvider(registration.id) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Metadata property provider is already registered");
    }
    return storage_->providers.PushBack(registration);
}

Base::Result<void> Registry::Complete() noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->ready) return {};
    if (!IsSealed() ||
        !Types().IsFrozen() ||
        !RuntimeData().IsSealed() ||
        !RuntimeData().ValueFacetsSealed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry and all typed behavior must be sealed before Registry completion");
    }
    storage_->ready = true;
    return {};
}


bool Registry::IsReady() const noexcept {
    return storage_ != nullptr && storage_->ready && storage_->sealed;
}

bool Registry::CanReadProperty(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    return accessor != nullptr &&
        (accessor->access == PropertyAccessKind::Provider ||
         (accessor->access == PropertyAccessKind::Ordinary &&
          accessor->get != nullptr));
}

bool Registry::CanWriteProperty(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    return accessor != nullptr &&
        (accessor->access == PropertyAccessKind::Provider ||
         (accessor->access == PropertyAccessKind::Ordinary &&
          accessor->set != nullptr));
}

bool Registry::CanReadValueMember(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    return accessor != nullptr && accessor->get != nullptr;
}

bool Registry::CanWriteValueMember(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    return accessor != nullptr && accessor->set != nullptr;
}

MemberId Registry::FindContentMember(
    TypeId type) const noexcept {
    return IsReady()
        ? Types().FindContentMember(type)
        : InvalidMemberId;
}

Base::Result<ContentInfo> Registry::GetContentInfo(
    MemberId member) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::ContentFacet* content =
        RuntimeData().FindContentByMember(member);
    if (content == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata content member was not found");
    }
    return ContentInfo{
        content->type,
        content->member,
        content->kind,
        content->flags,
        content->write != nullptr,
        content->clear != nullptr};
}

Base::Result<void> Registry::WriteContent(
    Base::Object& owner,
    MemberId member,
    const Base::Ref<Base::Object>& value) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::ContentFacet* content =
        RuntimeData().FindContentByMember(member);
    if (content == nullptr || content->write == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata content member is not writable");
    }
    const PropertyInfo* property =
        Types().FindProperty(member);
    const bool attached =
        property != nullptr &&
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::Attached);
    if (!attached &&
        !Types().IsAssignableFrom(
            content->type, owner.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata content owner type is incompatible");
    }
    const bool acceptsAnyValue =
        property != nullptr &&
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::AnyValue);
    if (!value || property == nullptr ||
        (!acceptsAnyValue &&
         !Types().IsAssignableFrom(
             property->ValueType(),
             value->RuntimeType()))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata content value type is incompatible");
    }
    content->write(owner, value, content->context);
    return {};
}

Base::Result<void> Registry::ClearContent(
    Base::Object& owner,
    MemberId member) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::ContentFacet* content =
        RuntimeData().FindContentByMember(member);
    if (content == nullptr || content->clear == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata content member cannot be cleared");
    }
    const PropertyInfo* property =
        Types().FindProperty(member);
    const bool attached =
        property != nullptr &&
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::Attached);
    if (!attached &&
        !Types().IsAssignableFrom(
            content->type, owner.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata content owner type is incompatible");
    }
    content->clear(owner, content->context);
    return {};
}

Base::Result<std::uint64_t>
Registry::SubscribePropertyChanged(
    Base::Object& object,
    MetadataPropertyChangedCallback callback,
    void* callbackContext) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::PropertyChangeNotificationFacet* notification =
        RuntimeData().FindPropertyChangeNotification(
            object.RuntimeType());
    if (notification == nullptr ||
        notification->subscribe == nullptr) {
        return UINT64_C(0);
    }
    return notification->subscribe(
        object,
        callback,
        callbackContext,
        notification->context);
}

Base::Result<bool> Registry::UnsubscribePropertyChanged(
    Base::Object& object,
    std::uint64_t subscription) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    if (subscription == 0U) return false;
    const ::Aero::PropertyChangeNotificationFacet* notification =
        RuntimeData().FindPropertyChangeNotification(
            object.RuntimeType());
    if (notification == nullptr ||
        notification->unsubscribe == nullptr) {
        return false;
    }
    return notification->unsubscribe(
        object,
        subscription,
        notification->context);
}

void* Registry::TryCastToInterface(
    Base::Object& object,
    TypeId interfaceType) const noexcept {
    if (!IsReady()) {
        return nullptr;
    }
    return Types().TryCastToInterface(object, interfaceType);
}

Base::Result<Base::Ref<Base::Object>>
Registry::CreateObject(TypeId type) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    const ::Aero::TypeFactoryFacet* factory =
        RuntimeData().FindTypeFactory(type);
    if (descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata type descriptor was not found");
    }
    if (descriptor->Kind() != MetadataTypeKind::Object ||
        HasTypeFlag(descriptor->Flags(), TypeFlags::Abstract) ||
        factory == nullptr ||
        factory->factory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata type has no constructible factory");
    }
    Base::Result<Base::Ref<Base::Object>> created =
        factory->factory();
    if (!created) return created.GetStatus();
    if (!created.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Metadata factory returned a null object");
    }
    if (created.Value()->RuntimeType() != type) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata factory returned the wrong runtime type");
    }
    return created;
}

Base::Result<Value> Registry::TryCreateValue(
    TypeId type,
    const void* source) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    const ::Aero::ValueSemanticsFacet* behavior =
        RuntimeData().FindValueSemantics(type);
    if (descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata value type descriptor was not found");
    }
    if (!HasTypeFlag(descriptor->Flags(), TypeFlags::ValueType) ||
        behavior == nullptr ||
        !behavior->semantics) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata type has no value semantics");
    }
    return Value::TryFromCustom(
        type, source, behavior->semantics);
}

Base::Result<Value> Registry::TryConvertText(
    TypeId type,
    Base::StringView text) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    if (descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata text target descriptor was not found");
    }
    if (descriptor->Kind() == MetadataTypeKind::Enum) {
        return ConvertEnumText(*descriptor, text);
    }
    const ::Aero::TextConverterFacet* converter =
        RuntimeData().FindTextConverter(type);
    if (converter == nullptr ||
        converter->convert == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata type has no text converter");
    }
    Base::Result<Value> converted =
        converter->convert(type, text, converter->context);
    if (!converted) return converted.GetStatus();
    if (converted.Value().IsUnset() ||
        converted.Value().Type() != type) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata text converter returned an incompatible value");
    }
    return converted;
}



Base::Result<Value> Registry::GetProperty(
    const Base::Object& object,
    MemberId member) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const PropertyInfo* property =
        Types().FindProperty(member);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata property descriptor was not found");
    }
    Base::Result<void> target =
        ValidatePropertyTarget(object, *property);
    if (!target) return target.GetStatus();
    if (HasPropertyFlag(
            property->Flags(), PropertyFlags::WriteOnly)) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Write-only metadata property cannot be read");
    }
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    if (accessor == nullptr) return UnsupportedProperty();

    Base::Result<Value> value = UnsupportedProperty();
    if (accessor->access == PropertyAccessKind::Ordinary) {
        if (accessor->get == nullptr) {
            return UnsupportedProperty();
        }
        value = accessor->get(object, accessor->context);
    } else if (accessor->access ==
               PropertyAccessKind::Provider) {
        if (accessor->provider ==
            DependencyPropertyProviderId) {
            value = GetDependencyProperty(
                object, *property);
        } else {
            const MetadataPropertyProviderRegistration* provider =
                FindProvider(accessor->provider);
            if (provider == nullptr ||
                provider->get == nullptr ||
                !Types().IsAssignableFrom(
                    provider->objectType,
                    object.RuntimeType())) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Readable metadata property provider was not found");
            }
            value = provider->get(
                object, *property, provider->context);
        }
    }
    if (!value) return value.GetStatus();
    if (value.Value().IsUnset() ||
        (!HasPropertyFlag(
             property->Flags(),
             PropertyFlags::AnyValue) &&
         value.Value().Type() != property->ValueType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata getter returned an incompatible value");
    }
    return value;
}

Base::Result<void> Registry::SetProperty(
    Base::Object& object,
    MemberId member,
    const Value& value) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const PropertyInfo* property =
        Types().FindProperty(member);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata property descriptor was not found");
    }
    Base::Result<void> target =
        ValidatePropertyTarget(object, *property);
    if (!target) return target.GetStatus();
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property value is unset");
    }
    if (HasPropertyFlag(
            property->Flags(), PropertyFlags::ReadOnly)) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Read-only metadata property cannot be written");
    }
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    if (accessor == nullptr) return UnsupportedProperty();

    const bool acceptsAnyValue =
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::AnyValue);
    const bool objectAssignment =
        value.Kind() == ValueKind::Object &&
        Types().IsAssignableFrom(
            property->ValueType(), value.Type());
    if (!acceptsAnyValue &&
        value.Type() != property->ValueType() &&
        !objectAssignment) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property value type does not match the descriptor");
    }
    if (!acceptsAnyValue &&
        !IsRegisteredEnumValue(
            property->ValueType(), value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Metadata enum property value is not registered");
    }
    if (accessor->access ==
        PropertyAccessKind::Ordinary) {
        if (accessor->set == nullptr) {
            return UnsupportedProperty();
        }
        accessor->set(object, value, accessor->context);
        return {};
    }
    if (accessor->access ==
        PropertyAccessKind::Provider) {
        if (accessor->provider ==
            DependencyPropertyProviderId) {
            return SetDependencyProperty(
                object, *property, value);
        }
        const MetadataPropertyProviderRegistration* provider =
            FindProvider(accessor->provider);
        if (provider == nullptr ||
            provider->set == nullptr ||
            !Types().IsAssignableFrom(
                provider->objectType,
                object.RuntimeType())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Writable metadata property provider was not found");
        }
        return provider->set(
            object, *property, value, provider->context);
    }
    return UnsupportedProperty();
}

EventHandlerThunk Registry::FindEventHandler(
    TypeId ownerType,
    Base::StringView name,
    bool includeBaseTypes) const noexcept {
    return Types().FindEventHandler(ownerType, name, includeBaseTypes);
}

EventHandlerThunk Registry::FindEventHandlerThunk(
    MemberId member) const noexcept {
    if (!storage_->ready) return nullptr;
    const ::Aero::MethodInvokerFacet* invoker =
        RuntimeData().FindMethodInvoker(member);
    return invoker != nullptr ? reinterpret_cast<EventHandlerThunk>(invoker->context) : nullptr;
}

bool Registry::HasPropertyFlag(
    PropertyFlags value,
    PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status Registry::MetadataNotReady() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Registry is not complete");
}

Base::Status Registry::UnsupportedProperty() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Metadata property has no usable accessor");
}

bool Registry::IsRegisteredEnumValue(
    TypeId type,
    const Value& value) const noexcept {
    const TypeInfo* info = Types().FindType(type);
    if (info == nullptr ||
        info->Kind() != MetadataTypeKind::Enum) {
        return true;
    }
    if (HasTypeFlag(
            info->Flags(), TypeFlags::SignedEnum)) {
        return value.Kind() ==
                ValueKind::SignedInteger &&
            Types().IsEnumValue(
                type,
                static_cast<std::uint64_t>(
                    value.AsSignedInteger()));
    }
    return value.Kind() ==
            ValueKind::UnsignedInteger &&
        Types().IsEnumValue(
            type, value.AsUnsignedInteger());
}

Base::Result<Value> Registry::ConvertEnumText(
    const TypeInfo& type,
    Base::StringView input) const noexcept {
    Base::StringView remaining =
        ::Aero::Base::ValueConversion::Trim(input);
    if (remaining.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Enum text is empty");
    }
    std::uint64_t raw = 0U;
    std::uint32_t tokenCount = 0U;
    while (!remaining.Empty()) {
        std::uint32_t split =
            remaining.SizeBytes();
        for (std::uint32_t index = 0U;
             index < remaining.SizeBytes(); ++index) {
            if (remaining[index] == ',' ||
                remaining[index] == '|') {
                split = index;
                break;
            }
        }
        const Base::StringView token =
            ::Aero::Base::ValueConversion::Trim(
                remaining.Substr(0U, split));
        if (token.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Enum text contains an empty value");
        }
        const EnumValueInfo* match = nullptr;
        for (const EnumValueInfo& candidate :
             type.EnumValues()) {
            if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
                    token, candidate.Name())) {
                match = &candidate;
                break;
            }
        }
        if (match == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Enum text value is not registered");
        }
        ++tokenCount;
        if (!type.IsFlagsEnum() &&
            tokenCount > 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Non-flags enum accepts exactly one value");
        }
        raw = type.IsFlagsEnum()
            ? (raw | match->RawValue())
            : match->RawValue();
        if (split == remaining.SizeBytes()) break;
        remaining = ::Aero::Base::ValueConversion::Trim(
            remaining.Substr(split + 1U));
    }
    return HasTypeFlag(
               type.Flags(), TypeFlags::SignedEnum)
        ? Value::FromSignedInteger(
              type.Id(),
              static_cast<std::int64_t>(raw))
        : Value::FromUnsignedInteger(type.Id(), raw);
}

Base::Result<void> Registry::ValidatePropertyTarget(
    const Base::Object& object,
    const PropertyInfo& property) const noexcept {
    if (HasPropertyFlag(
            property.Flags(), PropertyFlags::Attached)) {
        if (object.RuntimeType() != InvalidTypeId &&
            Types().FindType(
                object.RuntimeType()) != nullptr) {
            return {};
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Attached metadata property target has no type information");
    }
    if (Types().IsAssignableFrom(
            property.OwnerType(),
            object.RuntimeType())) {
        return {};
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Object type is incompatible with the metadata property");
}

Base::Result<Value> Registry::GetDependencyProperty(
    const Base::Object& object,
    const PropertyInfo& property) const noexcept {
    const DependencyPropertyHandle handle{property.Id()};
    const DependencyPropertyRegistry& registry =
        DependencyProperties();
    if (!Types().IsAssignableFrom(
            TypeOf<DependencyObject>(),
            object.RuntimeType()) ||
        registry.Find(handle) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property metadata target is invalid");
    }
    const auto& dependencyObject =
        static_cast<const DependencyObject&>(object);
    return dependencyObject.GetValue(handle);
}

Base::Result<void> Registry::SetDependencyProperty(
    Base::Object& object,
    const PropertyInfo& property,
    const Value& value) const noexcept {
    const DependencyPropertyHandle handle{property.Id()};
    const DependencyPropertyRegistry& registry =
        DependencyProperties();
    if (!Types().IsAssignableFrom(
            TypeOf<DependencyObject>(),
            object.RuntimeType()) ||
        registry.Find(handle) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property metadata target is invalid");
    }
    auto& dependencyObject =
        static_cast<DependencyObject&>(object);
    dependencyObject.SetValue(handle, value);
    return {};
}

const MetadataPropertyProviderRegistration*
Registry::FindProvider(
    PropertyProviderId id) const noexcept {
    for (const MetadataPropertyProviderRegistration& provider :
         storage_->providers) {
        if (provider.id == id) return &provider;
    }
    return nullptr;
}

} // namespace Aero::Meta

// ===== Registration =====

namespace Aero::Meta {

namespace {

::Aero::RegistrationState& State(void* value) noexcept {
    return *static_cast<::Aero::RegistrationState*>(value);
}

} // namespace

RegistrationTypes Registration::Types() noexcept {
    ::Aero::RegistrationState& state = State(state_);
    return RegistrationTypes(
        *state.types, *state.behaviors);
}

RegistrationValues Registration::Values() noexcept {
    return RegistrationValues(
        State(state_).values,
        State(state_).values);
}

RegistrationValues Registration::Values() const noexcept {
    return RegistrationValues(
        State(state_).values,
        nullptr);
}

ValueTable&
Registration::ValueRegistrations() noexcept {
    return *State(state_).values;
}

DependencyPropertyRegistry&
Registration::DependencyProperties() noexcept {
    return *State(state_).properties;
}

} // namespace Aero::Meta


