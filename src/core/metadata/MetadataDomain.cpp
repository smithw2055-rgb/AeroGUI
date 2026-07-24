#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Core/Metadata/MetadataValueFacets.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>

#include <new>
#include <utility>

namespace Aero::Core {

struct MetadataDomain::Storage final {
    struct ModuleRecord final {
        MetadataModuleId id = InvalidMetadataModuleId;
        std::uint32_t schemaVersion = 1U;
        MetadataModuleRegisterCallback registerModule = nullptr;
        void* context = nullptr;
        Base::String name;
    };

    TypeRegistry types;
    MetadataBehaviorRegistrationStore behaviorRegistrations;
    MetadataValueRegistrationStore valueRegistrations;
    DependencyPropertyRegistry dependencyProperties;
    RoutedEventCatalog routedEvents;
    MetadataDescriptorStore descriptors;
    MetadataFacetStore facets;
    Base::Vector<ModuleRecord> modules;
    bool sealed = false;

    Storage() noexcept
        : types(),
          behaviorRegistrations(types),
          valueRegistrations(types),
          dependencyProperties(types, behaviorRegistrations),
          routedEvents(types, behaviorRegistrations),
          descriptors(),
          facets(),
          modules() {}
};

MetadataDomain::MetadataDomain() noexcept
    : storage_(new (std::nothrow) Storage()) {}

MetadataDomain::~MetadataDomain() noexcept {
    delete storage_;
    storage_ = nullptr;
}

Base::Status MetadataDomain::OutOfMemoryStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory,
        "MetadataDomain storage allocation failed");
}

bool MetadataDomain::IsValid() const noexcept {
    return storage_ != nullptr;
}

bool MetadataDomain::IsSealed() const noexcept {
    return storage_ != nullptr && storage_->sealed;
}

std::uint32_t MetadataDomain::ModuleCount() const noexcept {
    return storage_ != nullptr ? storage_->modules.Size() : 0U;
}

Base::Result<void> MetadataDomain::ValidateRegistration(
    const MetadataModuleRegistration& registration) noexcept {
    if (registration.id == InvalidMetadataModuleId ||
        registration.name.Empty() || registration.schemaVersion == 0U ||
        registration.registerModule == nullptr) {
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

Base::Result<MetadataDomain::Storage*> MetadataDomain::BuildCandidate(
    const MetadataModuleRegistration* extra,
    bool seal) const noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();

    Storage* candidate = new (std::nothrow) Storage();
    if (candidate == nullptr) return OutOfMemoryStatus();

    auto applyAndAppend = [candidate](
        const MetadataModuleRegistration& registration) noexcept
        -> Base::Result<void> {
        MetaRegistrationContext context(
            candidate->types,
            candidate->behaviorRegistrations,
            candidate->valueRegistrations,
            candidate->dependencyProperties,
            &candidate->routedEvents);
        Base::Result<void> applied = registration.registerModule(
            context, registration.context);
        if (!applied) return applied.GetStatus();

        Storage::ModuleRecord record;
        record.id = registration.id;
        record.schemaVersion = registration.schemaVersion;
        record.registerModule = registration.registerModule;
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
        frozen = candidate->descriptors.Build(candidate->types);
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        frozen = candidate->facets.Build(
            candidate->types,
            candidate->behaviorRegistrations,
            candidate->descriptors,
            candidate->dependencyProperties,
            candidate->routedEvents);
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        frozen = candidate->facets.BuildValueFacets(
            candidate->valueRegistrations,
            candidate->descriptors);
        if (!frozen) {
            delete candidate;
            return frozen.GetStatus();
        }
        candidate->sealed = true;
    }

    return candidate;
}

Base::Result<void> MetadataDomain::TryRegisterModule(
    const MetadataModuleRegistration& registration) noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetadataDomain is sealed");
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

Base::Result<void> MetadataDomain::Seal() noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) return {};
    Base::Result<Storage*> candidate = BuildCandidate(nullptr, true);
    if (!candidate) return candidate.GetStatus();
    delete storage_;
    storage_ = candidate.Value();
    return {};
}

const TypeRegistry& MetadataDomain::Types() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->types;
}

DependencyPropertyRegistry& MetadataDomain::DependencyProperties() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

const DependencyPropertyRegistry& MetadataDomain::DependencyProperties() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

RoutedEventCatalog& MetadataDomain::RoutedEvents() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->routedEvents;
}

const RoutedEventCatalog& MetadataDomain::RoutedEvents() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->routedEvents;
}

const MetadataDescriptorStore& MetadataDomain::Descriptors() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->descriptors;
}

const MetadataFacetStore& MetadataDomain::Facets() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->facets;
}

Base::Result<Base::HashCode> MetadataDomain::ComputeSchemaHash() const noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (!storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetadataDomain schema hash requires a sealed domain");
    }

    Base::Result<Base::HashCode> descriptorHash =
        storage_->descriptors.ComputeHash();
    if (!descriptorHash) return descriptorHash.GetStatus();
    Base::Result<Base::HashCode> facetHash = storage_->facets.ComputeHash();
    if (!facetHash) return facetHash.GetStatus();
    Base::Result<Base::HashCode> valueFacetHash =
        ComputeMetadataValueFacetHash(storage_->facets, storage_->descriptors);
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
