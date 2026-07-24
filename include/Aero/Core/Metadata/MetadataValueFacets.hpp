#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Metadata/MetadataDescriptors.hpp>

namespace Aero::Core {

// Computes the deterministic structural contribution of value-semantics and
// text-converter facets. Callback and context addresses are never included.
AERO_API Base::Result<Base::HashCode> ComputeMetadataValueFacetHash(
    const MetadataFacetStore& facets,
    const MetadataDescriptorStore& descriptors) noexcept;

} // namespace Aero::Core
