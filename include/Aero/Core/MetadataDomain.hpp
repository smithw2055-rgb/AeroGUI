#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/ObjectTree.hpp>
#include <Aero/Core/Presentation.hpp>

#include <cstdint>
#include <new>
#include <utility>

namespace Aero::Core {

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
    MetaRegistrationContext& context,
    void* userContext) noexcept;

struct MetadataModuleRegistration final {
    MetadataModuleId id = InvalidMetadataModuleId;
    Base::StringView name;
    std::uint32_t schemaVersion = 1U;
    MetadataModuleRegisterCallback registerModule = nullptr;
    void* context = nullptr;
};

// Owns one coherent metadata universe. Module registration is transactional:
// registrations are replayed into a private candidate storage and become
// visible only after the complete module succeeds. Registration callbacks must
// therefore be deterministic and must not mutate state outside the supplied
// MetaRegistrationContext.
//
// Registry references and descriptor pointers obtained before the next module
// registration or Seal() are provisional because a successful transaction
// replaces the candidate storage. Addresses become stable after Seal().
class AERO_API MetadataDomain final {
public:
    MetadataDomain() noexcept;
    ~MetadataDomain() noexcept;

    MetadataDomain(const MetadataDomain&) = delete;
    MetadataDomain& operator=(const MetadataDomain&) = delete;
    MetadataDomain(MetadataDomain&&) = delete;
    MetadataDomain& operator=(MetadataDomain&&) = delete;

    bool IsValid() const noexcept { return storage_ != nullptr; }
    bool IsSealed() const noexcept;
    std::uint32_t ModuleCount() const noexcept;

    Base::Result<void> TryRegisterModule(
        const MetadataModuleRegistration& registration) noexcept;
    Base::Result<void> Seal() noexcept;

    TypeRegistry& Types() noexcept;
    const TypeRegistry& Types() const noexcept;
    DependencyPropertyRegistry& DependencyProperties() noexcept;
    const DependencyPropertyRegistry& DependencyProperties() const noexcept;
    RoutedEventRegistry& RoutedEvents() noexcept;
    const RoutedEventRegistry& RoutedEvents() const noexcept;

    Base::Result<Base::HashCode> ComputeSchemaHash() const noexcept;

private:
    struct ModuleRecord final {
        MetadataModuleId id = InvalidMetadataModuleId;
        std::uint32_t schemaVersion = 1U;
        MetadataModuleRegisterCallback registerModule = nullptr;
        void* context = nullptr;
        Base::String name;
    };

    struct Storage final {
        TypeRegistry types;
        DependencyPropertyRegistry dependencyProperties;
        RoutedEventRegistry routedEvents;
        Base::Vector<ModuleRecord> modules;
        bool sealed = false;

        Storage() noexcept
            : types(),
              dependencyProperties(types),
              routedEvents(types),
              modules() {}
    };

    Storage* storage_ = nullptr;

    static Base::Status OutOfMemoryStatus() noexcept;
    static Base::Result<void> ValidateRegistration(
        const MetadataModuleRegistration& registration) noexcept;
    static const ModuleRecord* FindModule(
        const Storage& storage,
        MetadataModuleId id) noexcept;
    static Base::Result<void> ApplyModule(
        Storage& storage,
        const MetadataModuleRegistration& registration) noexcept;
    static Base::Result<void> AppendModule(
        Storage& storage,
        const MetadataModuleRegistration& registration) noexcept;
    Base::Result<Storage*> BuildCandidate(
        const MetadataModuleRegistration* extra,
        bool seal) const noexcept;
};

inline MetadataDomain::MetadataDomain() noexcept
    : storage_(new (std::nothrow) Storage()) {}

inline MetadataDomain::~MetadataDomain() noexcept {
    delete storage_;
    storage_ = nullptr;
}

inline Base::Status MetadataDomain::OutOfMemoryStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory,
        "MetadataDomain storage allocation failed");
}

inline bool MetadataDomain::IsSealed() const noexcept {
    return storage_ != nullptr && storage_->sealed;
}

inline std::uint32_t MetadataDomain::ModuleCount() const noexcept {
    return storage_ != nullptr ? storage_->modules.Size() : 0U;
}

inline Base::Result<void> MetadataDomain::ValidateRegistration(
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

inline const MetadataDomain::ModuleRecord* MetadataDomain::FindModule(
    const Storage& storage,
    MetadataModuleId id) noexcept {
    for (const ModuleRecord& module : storage.modules) {
        if (module.id == id) {
            return &module;
        }
    }
    return nullptr;
}

inline Base::Result<void> MetadataDomain::ApplyModule(
    Storage& storage,
    const MetadataModuleRegistration& registration) noexcept {
    MetaRegistrationContext context(
        storage.types,
        storage.dependencyProperties,
        &storage.routedEvents);
    return registration.registerModule(context, registration.context);
}

inline Base::Result<void> MetadataDomain::AppendModule(
    Storage& storage,
    const MetadataModuleRegistration& registration) noexcept {
    ModuleRecord record;
    record.id = registration.id;
    record.schemaVersion = registration.schemaVersion;
    record.registerModule = registration.registerModule;
    record.context = registration.context;
    Base::Result<void> assigned = record.name.TryAssign(registration.name);
    if (!assigned) {
        return assigned.GetStatus();
    }
    return storage.modules.TryPushBack(std::move(record));
}

inline Base::Result<MetadataDomain::Storage*> MetadataDomain::BuildCandidate(
    const MetadataModuleRegistration* extra,
    bool seal) const noexcept {
    if (storage_ == nullptr) {
        return OutOfMemoryStatus();
    }

    Storage* candidate = new (std::nothrow) Storage();
    if (candidate == nullptr) {
        return OutOfMemoryStatus();
    }

    for (const ModuleRecord& module : storage_->modules) {
        const MetadataModuleRegistration replay{
            module.id,
            module.name.View(),
            module.schemaVersion,
            module.registerModule,
            module.context};
        Base::Result<void> applied = ApplyModule(*candidate, replay);
        if (!applied) {
            delete candidate;
            return applied.GetStatus();
        }
        Base::Result<void> appended = AppendModule(*candidate, replay);
        if (!appended) {
            delete candidate;
            return appended.GetStatus();
        }
    }

    if (extra != nullptr) {
        Base::Result<void> applied = ApplyModule(*candidate, *extra);
        if (!applied) {
            delete candidate;
            return applied.GetStatus();
        }
        Base::Result<void> appended = AppendModule(*candidate, *extra);
        if (!appended) {
            delete candidate;
            return appended.GetStatus();
        }
    }

    if (seal) {
        Base::Result<void> frozen = candidate->types.Freeze();
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
        candidate->sealed = true;
    }

    return candidate;
}

inline Base::Result<void> MetadataDomain::TryRegisterModule(
    const MetadataModuleRegistration& registration) noexcept {
    if (storage_ == nullptr) {
        return OutOfMemoryStatus();
    }
    if (storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetadataDomain is sealed");
    }
    Base::Result<void> valid = ValidateRegistration(registration);
    if (!valid) {
        return valid.GetStatus();
    }
    if (FindModule(*storage_, registration.id) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Metadata module is already registered");
    }

    Base::Result<Storage*> candidate = BuildCandidate(&registration, false);
    if (!candidate) {
        return candidate.GetStatus();
    }
    delete storage_;
    storage_ = candidate.Value();
    return {};
}

inline Base::Result<void> MetadataDomain::Seal() noexcept {
    if (storage_ == nullptr) {
        return OutOfMemoryStatus();
    }
    if (storage_->sealed) {
        return {};
    }

    Base::Result<Storage*> candidate = BuildCandidate(nullptr, true);
    if (!candidate) {
        return candidate.GetStatus();
    }
    delete storage_;
    storage_ = candidate.Value();
    return {};
}

inline TypeRegistry& MetadataDomain::Types() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->types;
}

inline const TypeRegistry& MetadataDomain::Types() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->types;
}

inline DependencyPropertyRegistry&
MetadataDomain::DependencyProperties() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

inline const DependencyPropertyRegistry&
MetadataDomain::DependencyProperties() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

inline RoutedEventRegistry& MetadataDomain::RoutedEvents() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->routedEvents;
}

inline const RoutedEventRegistry& MetadataDomain::RoutedEvents() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->routedEvents;
}

inline Base::Result<Base::HashCode>
MetadataDomain::ComputeSchemaHash() const noexcept {
    if (storage_ == nullptr) {
        return OutOfMemoryStatus();
    }
    if (!storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetadataDomain schema hash requires a sealed domain");
    }

    Base::Result<Base::HashCode> typeHash =
        storage_->types.ComputeSnapshotHash();
    if (!typeHash) {
        return typeHash.GetStatus();
    }

    Base::HashCode hash = typeHash.Value();
    hash = Base::MixHash64(
        hash ^ static_cast<Base::HashCode>(
            storage_->dependencyProperties.PropertyCount()));
    for (const ModuleRecord& module : storage_->modules) {
        hash = Base::MixHash64(hash ^ module.id);
        hash = Base::MixHash64(
            hash ^ static_cast<Base::HashCode>(module.schemaVersion));
    }
    return hash;
}

} // namespace Aero::Core
