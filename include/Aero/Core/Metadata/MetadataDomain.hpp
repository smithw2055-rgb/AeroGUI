#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Events/RoutedEventCatalog.hpp>
#include <Aero/Core/Metadata/MetadataDescriptors.hpp>
#include <Aero/Core/Metadata/MetaRegistrationContext.hpp>

#include <cstdint>

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

// A MetadataDomain has two explicit phases:
//
// 1. Registration phase: deterministic module callbacks populate mutable
//    TypeRegistry, dependency/routed registries, and registration value services.
// 2. Runtime phase: Seal() freezes those registration sources and materializes
//    immutable MetadataDescriptorStore and MetadataFacetStore instances.
//
// Runtime lookup should use Descriptors(), Facets(), and MetadataRuntime. Registry
// references and registration-value views obtained before the next module
// transaction are provisional because a successful transaction replaces the
// complete candidate storage.
class AERO_API MetadataDomain final {
public:
    MetadataDomain() noexcept;
    ~MetadataDomain() noexcept;

    MetadataDomain(const MetadataDomain&) = delete;
    MetadataDomain& operator=(const MetadataDomain&) = delete;
    MetadataDomain(MetadataDomain&&) = delete;
    MetadataDomain& operator=(MetadataDomain&&) = delete;

    bool IsValid() const noexcept;
    bool IsSealed() const noexcept;
    std::uint32_t ModuleCount() const noexcept;

    Base::Result<void> TryRegisterModule(
        const MetadataModuleRegistration& registration) noexcept;
    Base::Result<void> Seal() noexcept;

    // Structural registration data is exposed read-only. Mutable registration
    // is confined to module callbacks and their MetaRegistrationContext.
    const TypeRegistry& Types() const noexcept;
    DependencyPropertyRegistry& DependencyProperties() noexcept;
    const DependencyPropertyRegistry& DependencyProperties() const noexcept;
    RoutedEventCatalog& RoutedEvents() noexcept;
    const RoutedEventCatalog& RoutedEvents() const noexcept;

    const MetadataDescriptorStore& Descriptors() const noexcept;
    const MetadataFacetStore& Facets() const noexcept;

    Base::Result<Base::HashCode> ComputeSchemaHash() const noexcept;

private:
    struct Storage;
    Storage* storage_ = nullptr;

    static Base::Status OutOfMemoryStatus() noexcept;
    static Base::Result<void> ValidateRegistration(
        const MetadataModuleRegistration& registration) noexcept;
    Base::Result<Storage*> BuildCandidate(
        const MetadataModuleRegistration* extra,
        bool seal) const noexcept;
};

} // namespace Aero::Core
