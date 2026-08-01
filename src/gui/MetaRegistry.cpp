#include <Aero/Meta/Registry.hpp>

#include "MetaInternals.hpp"
#include "BehaviorTable.hpp"
#include "ValueTable.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>

#include <new>
#include <utility>

namespace Aero::Core {

struct MetaRegistry::Storage final {
    struct ModuleRecord final {
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
    Detail::MetaTable facets;
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

MetaRegistry::MetaRegistry() noexcept
    : storage_(new (std::nothrow) Storage()) {}

MetaRegistry::~MetaRegistry() noexcept {
    delete storage_;
    storage_ = nullptr;
}

Base::Status MetaRegistry::OutOfMemoryStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory,
        "MetaRegistry storage allocation failed");
}

bool MetaRegistry::IsValid() const noexcept {
    return storage_ != nullptr;
}

bool MetaRegistry::IsSealed() const noexcept {
    return storage_ != nullptr && storage_->sealed;
}

std::uint32_t MetaRegistry::ModuleCount() const noexcept {
    return storage_ != nullptr ? storage_->modules.Size() : 0U;
}

Base::Result<void> MetaRegistry::ValidateRegistration(
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

Base::Result<MetaRegistry::Storage*> MetaRegistry::BuildCandidate(
    const MetadataModuleRegistration* extra,
    bool seal) const noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();

    Storage* candidate = new (std::nothrow) Storage();
    if (candidate == nullptr) return OutOfMemoryStatus();

    auto applyAndAppend = [candidate](
        const MetadataModuleRegistration& registration) noexcept
        -> Base::Result<void> {
        Detail::MetaRegistrationState contextState{
            &candidate->types,
            &candidate->behaviorRegistrations,
            &candidate->valueRegistrations,
            &candidate->dependencyProperties,
            &candidate->routedEvents};
        MetaRegistration context(&contextState);
        Base::Result<void> applied = registration.registerModule != nullptr
            ? registration.registerModule(context)
            : registration.registerModuleWithContext(
                  context, registration.context);
        if (!applied) return applied.GetStatus();

        Storage::ModuleRecord record;
        record.id = registration.id;
        record.schemaVersion = registration.schemaVersion;
        record.registerModule = registration.registerModule;
        record.registerModuleWithContext =
            registration.registerModuleWithContext;
        record.context = registration.context;
        Base::Result<void> assigned = record.name.TryAssign(registration.name);
        if (!assigned) return assigned.GetStatus();
        return candidate->modules.TryPushBack(std::move(record));
    };

    for (const Storage::ModuleRecord& module : storage_->modules) {
        const MetadataModuleRegistration replay{
            module.id,
            module.name.View(),
            module.schemaVersion,
            module.registerModule,
            module.registerModuleWithContext,
            module.context};
        Base::Result<void> applied = applyAndAppend(replay);
        if (!applied) {
            delete candidate;
            return applied.GetStatus();
        }
    }

    if (extra != nullptr) {
        Base::Result<void> applied = applyAndAppend(*extra);
        if (!applied) {
            delete candidate;
            return applied.GetStatus();
        }
    }

    if (seal) {
        Base::Result<void> frozen = candidate->types.Freeze();
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        frozen = candidate->behaviorRegistrations.Freeze();
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        frozen = candidate->valueRegistrations.Freeze();
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        frozen = candidate->dependencyProperties.Freeze();
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        frozen = candidate->routedEvents.Freeze();
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        frozen = candidate->facets.Build(
            candidate->types,
            candidate->behaviorRegistrations,
            candidate->dependencyProperties,
            candidate->routedEvents);
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        frozen = candidate->facets.BuildValueFacets(
            candidate->valueRegistrations,
            candidate->types);
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        candidate->sealed = true;
    }

    return candidate;
}

Base::Result<void> MetaRegistry::TryRegisterModule(
    const MetadataModuleRegistration& registration) noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetaRegistry is sealed");
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

    Base::Result<Storage*> candidate = BuildCandidate(&registration, false);
    if (!candidate) return candidate.GetStatus();
    delete storage_;
    storage_ = candidate.Value();
    return {};
}

Base::Result<void> MetaRegistry::Seal() noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) return {};
    Base::Result<Storage*> candidate = BuildCandidate(nullptr, true);
    if (!candidate) return candidate.GetStatus();
    delete storage_;
    storage_ = candidate.Value();
    return {};
}

const TypeRegistry& MetaRegistry::Types() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->types;
}

DependencyPropertyRegistry& MetaRegistry::DependencyProperties() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

const DependencyPropertyRegistry& MetaRegistry::DependencyProperties() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

void* MetaRegistry::RoutedEventState() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return &storage_->routedEvents;
}

const Detail::MetaTable& MetaRegistry::RuntimeData() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->facets;
}

Base::Result<Base::HashCode> MetaRegistry::ComputeSchemaHash() const noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (!storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetaRegistry schema hash requires a sealed domain");
    }

    Base::Result<Base::HashCode> descriptorHash =
        storage_->types.ComputeHash();
    if (!descriptorHash) return descriptorHash.GetStatus();
    Base::Result<Base::HashCode> facetHash = storage_->facets.ComputeHash();
    if (!facetHash) return facetHash.GetStatus();
    Base::Result<Base::HashCode> valueFacetHash =
        Detail::ComputeMetadataValueFacetHash(
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

} // namespace Aero::Core

// Executable metadata operations share the MetaRegistry storage and lifetime.
#include "MetaTable.hpp"
#include <Aero/Meta/ValueConversion.hpp>
#include <cstdio>

namespace Aero::Core {
Base::Result<void> MetaRegistry::TryRegisterPropertyProvider(
    const MetadataPropertyProviderRegistration& registration) noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->ready) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetaRegistry is complete");
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
    return storage_->providers.TryPushBack(registration);
}

Base::Result<void> MetaRegistry::Complete() noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->ready) return {};
    if (!IsSealed() ||
        !Types().IsFrozen() ||
        !RuntimeData().IsSealed() ||
        !RuntimeData().ValueFacetsSealed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetaRegistry and all typed behavior must be sealed before MetaRegistry completion");
    }
    storage_->ready = true;
    return {};
}


bool MetaRegistry::IsReady() const noexcept {
    return storage_ != nullptr && storage_->ready && storage_->sealed;
}

bool MetaRegistry::CanReadProperty(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const Detail::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    return accessor != nullptr &&
        (accessor->access == PropertyAccessKind::Provider ||
         (accessor->access == PropertyAccessKind::Ordinary &&
          accessor->get != nullptr));
}

bool MetaRegistry::CanWriteProperty(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const Detail::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    return accessor != nullptr &&
        (accessor->access == PropertyAccessKind::Provider ||
         (accessor->access == PropertyAccessKind::Ordinary &&
          accessor->set != nullptr));
}

bool MetaRegistry::CanReadValueMember(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const Detail::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    return accessor != nullptr && accessor->get != nullptr;
}

bool MetaRegistry::CanWriteValueMember(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const Detail::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    return accessor != nullptr && accessor->set != nullptr;
}

MemberId MetaRegistry::FindContentMember(
    TypeId type) const noexcept {
    return IsReady()
        ? Types().FindContentMember(type)
        : InvalidMemberId;
}

Base::Result<ContentInfo> MetaRegistry::GetContentInfo(
    MemberId member) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const Detail::ContentFacet* content =
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

Base::Result<void> MetaRegistry::WriteContent(
    Base::Object& owner,
    MemberId member,
    const Base::Ref<Base::Object>& value) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const Detail::ContentFacet* content =
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
    return content->write(owner, value, content->context);
}

Base::Result<void> MetaRegistry::ClearContent(
    Base::Object& owner,
    MemberId member) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const Detail::ContentFacet* content =
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
    return content->clear(owner, content->context);
}

Base::Result<std::uint64_t>
MetaRegistry::SubscribePropertyChanged(
    Base::Object& object,
    MetadataPropertyChangedCallback callback,
    void* callbackContext) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const Detail::PropertyChangeNotificationFacet* notification =
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

Base::Result<bool> MetaRegistry::UnsubscribePropertyChanged(
    Base::Object& object,
    std::uint64_t subscription) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    if (subscription == 0U) return false;
    const Detail::PropertyChangeNotificationFacet* notification =
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

Base::Result<Base::Ref<Base::Object>>
MetaRegistry::CreateObject(TypeId type) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    const Detail::TypeFactoryFacet* factory =
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
        std::fprintf(
            stderr,
            "Metadata type '%.*s' is not constructible (kind=%u, abstract=%u, factory=%u)\n",
            static_cast<int>(descriptor->Name().SizeBytes()),
            descriptor->Name().Data(),
            static_cast<unsigned>(descriptor->Kind()),
            HasTypeFlag(descriptor->Flags(), TypeFlags::Abstract) ? 1U : 0U,
            factory != nullptr && factory->factory != nullptr ? 1U : 0U);
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

Base::Result<Value> MetaRegistry::TryCreateValue(
    TypeId type,
    const void* source) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    const Detail::ValueSemanticsFacet* behavior =
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

Base::Result<Value> MetaRegistry::TryConvertText(
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
        return TryConvertEnumText(*descriptor, text);
    }
    const Detail::TextConverterFacet* converter =
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

Base::Result<Value> MetaRegistry::GetValueMember(
    const Value& owner,
    MemberId member) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const FieldInfo* field = Types().FindField(member);
    const Detail::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    if (field == nullptr ||
        accessor == nullptr ||
        accessor->get == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata value field or accessor was not found");
    }
    if (owner.Kind() != ValueKind::Custom ||
        owner.Type() != field->OwnerType() ||
        owner.AsCustom() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field target is incompatible");
    }
    Base::Result<Value> result = accessor->get(
        owner.AsCustom(),
        const_cast<MetaRegistry&>(*this),
        accessor->context);
    if (!result) return result.GetStatus();
    if (result.Value().IsUnset() ||
        result.Value().Type() != field->ValueType()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field getter returned an incompatible value");
    }
    return result;
}

Base::Result<void> MetaRegistry::SetValueMember(
    Value& owner,
    MemberId member,
    const Value& value) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const FieldInfo* field = Types().FindField(member);
    const Detail::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    if (field == nullptr || accessor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata value field or accessor was not found");
    }
    if (owner.Kind() != ValueKind::Custom ||
        owner.Type() != field->OwnerType() ||
        owner.AsCustom() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field target is incompatible");
    }
    if (HasFieldFlag(field->Flags(), FieldFlags::ReadOnly) ||
        accessor->set == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Read-only metadata value field cannot be written");
    }
    if (value.IsUnset() ||
        value.Type() != field->ValueType()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field value type does not match");
    }
    Base::Result<Value> clone =
        TryCreateValue(owner.Type(), owner.AsCustom());
    if (!clone) return clone.GetStatus();
    owner = std::move(clone).Value();
    return accessor->set(
        owner.MutableCustom(),
        value,
        const_cast<MetaRegistry&>(*this),
        accessor->context);
}

Base::Result<Value> MetaRegistry::GetProperty(
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
    const Detail::PropertyAccessorFacet* accessor =
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

Base::Result<void> MetaRegistry::SetProperty(
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
    const Detail::PropertyAccessorFacet* accessor =
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
        return accessor->set != nullptr
            ? accessor->set(
                  object, value, accessor->context)
            : Base::Result<void>(UnsupportedProperty());
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

Base::Result<Value> MetaRegistry::InvokeMethod(
    Base::Object& object,
    MemberId member,
    Base::Span<const Value> arguments) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const MethodInfo* method = Types().FindMethod(member);
    const Detail::MethodInvokerFacet* invoker =
        RuntimeData().FindMethodInvoker(member);
    if (method == nullptr ||
        invoker == nullptr ||
        invoker->invoke == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata method or invoker was not found");
    }
    if (!Types().IsAssignableFrom(
            method->OwnerType(), object.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Object type is incompatible with the metadata method");
    }
    if (arguments.Size() !=
        method->Parameters().Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata method argument count does not match");
    }
    for (std::uint32_t index = 0U;
         index < arguments.Size(); ++index) {
        if (arguments[index].IsUnset() ||
            arguments[index].Type() !=
                method->Parameters()[index].Type()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata method argument type does not match");
        }
    }
    Base::Result<Value> result = invoker->invoke(
        object, arguments, invoker->context);
    if (!result) return result.GetStatus();
    if ((method->ReturnType() == InvalidTypeId &&
         !result.Value().IsUnset()) ||
        (method->ReturnType() != InvalidTypeId &&
         (result.Value().IsUnset() ||
          result.Value().Type() !=
              method->ReturnType()))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata method returned an incompatible value");
    }
    return result;
}

bool MetaRegistry::HasPropertyFlag(
    PropertyFlags value,
    PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status MetaRegistry::MetadataNotReady() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "MetaRegistry is not complete");
}

Base::Status MetaRegistry::UnsupportedProperty() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Metadata property has no usable accessor");
}

bool MetaRegistry::IsRegisteredEnumValue(
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

Base::Result<Value> MetaRegistry::TryConvertEnumText(
    const TypeInfo& type,
    Base::StringView input) const noexcept {
    Base::StringView remaining =
        ValueConversion::Trim(input);
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
            ValueConversion::Trim(
                remaining.Substr(0U, split));
        if (token.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Enum text contains an empty value");
        }
        const EnumValueInfo* match = nullptr;
        for (const EnumValueInfo& candidate :
             type.EnumValues()) {
            if (ValueConversion::EqualsAsciiInsensitive(
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
        remaining = ValueConversion::Trim(
            remaining.Substr(split + 1U));
    }
    return HasTypeFlag(
               type.Flags(), TypeFlags::SignedEnum)
        ? Value::FromSignedInteger(
              type.Id(),
              static_cast<std::int64_t>(raw))
        : Value::FromUnsignedInteger(type.Id(), raw);
}

Base::Result<void> MetaRegistry::ValidatePropertyTarget(
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

Base::Result<Value> MetaRegistry::GetDependencyProperty(
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
    if (&dependencyObject.PropertyRegistry() !=
        &registry) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property registry does not match metadata domain");
    }
    return dependencyObject.GetValue(handle);
}

Base::Result<void> MetaRegistry::SetDependencyProperty(
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
    if (&dependencyObject.PropertyRegistry() !=
        &registry) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property registry does not match metadata domain");
    }
    return dependencyObject.SetValue(handle, value);
}

const MetadataPropertyProviderRegistration*
MetaRegistry::FindProvider(
    PropertyProviderId id) const noexcept {
    for (const MetadataPropertyProviderRegistration& provider :
         storage_->providers) {
        if (provider.id == id) return &provider;
    }
    return nullptr;
}


} // namespace Aero::Core
