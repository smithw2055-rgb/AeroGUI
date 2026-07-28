#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/RoutedEvent.hpp>
#include <Aero/Core/Metadata/MetadataContext.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>

#include <cstdint>

namespace Aero::Core {

class MetadataRuntime;
namespace Detail {
class MetadataFacetStore;
class MetadataDomainAccess;
}

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
    MetadataContext& context) noexcept;
using MetadataModuleRegisterContextCallback = Base::Result<void> (*)(
    MetadataContext& context,
    void* userContext) noexcept;

struct MetadataModuleRegistration final {
    MetadataModuleId id = InvalidMetadataModuleId;
    Base::StringView name;
    std::uint32_t schemaVersion = 1U;
    MetadataModuleRegisterCallback registerModule = nullptr;
    MetadataModuleRegisterContextCallback registerModuleWithContext = nullptr;
    void* context = nullptr;
};

// A MetadataDomain has two explicit phases:
//
// 1. Registration phase: deterministic module callbacks populate mutable
//    TypeRegistry, dependency/routed registries, and registration value services.
// 2. Runtime phase: Seal() freezes the structural registry and materializes
//    internal executable runtime tables.
//
// Runtime structural lookup uses Types(). Registry references and
// registration-value views obtained before the next module
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
    // is confined to module callbacks and their MetadataContext.
    const TypeRegistry& Types() const noexcept;
    const DependencyPropertyRegistry& DependencyProperties() const noexcept;
    Base::Result<Base::HashCode> ComputeSchemaHash() const noexcept;

private:
    friend class MetadataRuntime;
    friend class Detail::MetadataDomainAccess;

    struct Storage;
    Storage* storage_ = nullptr;

    DependencyPropertyRegistry& DependencyProperties() noexcept;
    void* RoutedEventState() noexcept;
    const Detail::MetadataFacetStore& RuntimeData() const noexcept;

    static Base::Status OutOfMemoryStatus() noexcept;
    static Base::Result<void> ValidateRegistration(
        const MetadataModuleRegistration& registration) noexcept;
    Base::Result<Storage*> BuildCandidate(
        const MetadataModuleRegistration* extra,
        bool seal) const noexcept;
};

} // namespace Aero::Core
