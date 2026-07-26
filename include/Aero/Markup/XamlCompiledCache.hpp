#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>

#include <cstdint>

namespace Aero::Markup {

// Increment only when the compiled-XAML cache header or IR interpretation
// changes. Metadata graph changes are tracked separately by metadataSchemaHash.
inline constexpr std::uint32_t XamlCompiledCacheFormatVersion = 4U;

struct XamlCompiledCacheIdentity final {
    std::uint32_t cacheFormatVersion = XamlCompiledCacheFormatVersion;
    std::uint32_t typeIdAlgorithmVersion = Core::TypeIdAlgorithmVersion;
    std::uint32_t descriptorFormatVersion =
        Core::MetadataDescriptorFormatVersion;
    std::uint32_t facetFormatVersion = Core::MetadataFacetFormatVersion;
    Base::HashCode metadataSchemaHash = 0U;
};

enum class XamlCompiledCacheCompatibility : std::uint8_t {
    Compatible = 0U,
    CacheFormatMismatch,
    TypeIdAlgorithmMismatch,
    DescriptorFormatMismatch,
    FacetFormatMismatch,
    MetadataSchemaMismatch
};

AERO_API Base::Result<XamlCompiledCacheIdentity>
BuildXamlCompiledCacheIdentity(
    const Core::MetadataDomain& domain) noexcept;

AERO_API XamlCompiledCacheCompatibility
CompareXamlCompiledCacheIdentity(
    const XamlCompiledCacheIdentity& cached,
    const XamlCompiledCacheIdentity& current) noexcept;

AERO_API Base::Result<void> ValidateXamlCompiledCacheIdentity(
    const XamlCompiledCacheIdentity& cached,
    const Core::MetadataDomain& currentDomain) noexcept;

} // namespace Aero::Markup
