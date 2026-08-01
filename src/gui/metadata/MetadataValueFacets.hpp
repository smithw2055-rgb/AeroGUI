#pragma once

// Private helpers for sealing value behavior into MetadataRuntimeData.

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include "MetadataRuntimeData.hpp"

namespace Aero::Core::Detail {

// Computes the deterministic structural contribution of value-semantics and
// text-converter facets. Callback and context addresses are never included.
Base::Result<Base::HashCode> ComputeMetadataValueFacetHash(
    const MetadataFacetStore& facets,
    const TypeRegistry& descriptors) noexcept;

} // namespace Aero::Core::Detail
