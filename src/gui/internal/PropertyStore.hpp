#pragma once

#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/MetadataId.hpp>
#include <Aero/Diagnostics/PropertyValueSource.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero {

// Opaque per-object DP store. Defined only in src; the installed
// DependencyObject header keeps a void* handle so the six-copy entry layout
// is not part of the public ABI.
struct StoredValueEntry {
    PropertyProviderSet baseProviders;
    PropertyExpression localExpression;
    PropertyValue localValue;
    PropertyValue currentValue;
    PropertyValue inheritedValue;
    PropertyValue animationValue;
    PropertyValue baseValue;
    PropertyValue effectiveValue;
    PropertyValueSourceInfo sourceInfo;
    std::uint64_t queueSequence = 0U;
    bool hasLocal = false;
    bool hasCurrent = false;
    bool hasExpression = false;
    bool hasInherited = false;
    bool hasAnimation = false;
    bool queued = false;
};

struct PropertyStore {
    Base::HashMap<MemberId, StoredValueEntry> entries;
};

} // namespace Aero
