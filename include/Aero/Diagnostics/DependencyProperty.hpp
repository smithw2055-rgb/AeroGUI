#pragma once

#include <Aero/DependencyObject.hpp>
#include <Aero/Diagnostics/PropertyValueSource.hpp>

namespace Aero::Diagnostics {

using PropertyValueRank = Meta::PropertyValueRank;
using PropertyValueSourceInfo = Meta::PropertyValueSourceInfo;
using PropertyProviderToken = Meta::PropertyProviderToken;
using PropertyExpressionKind = Meta::PropertyExpressionKind;

inline Base::Result<PropertyValueSourceInfo> GetValueSource(const DependencyObject& object, DependencyPropertyHandle property) noexcept {
    return object.GetValueSourceInfo(property);
}

} // namespace Aero::Diagnostics
