#pragma once

#include <Aero/DependencyObject.hpp>
#include <Aero/Diagnostics/PropertyValueSource.hpp>

namespace Aero::Diagnostics {

using PropertyValueRank = Core::PropertyValueRank;
using PropertyValueSourceInfo = Core::PropertyValueSourceInfo;
using PropertyProviderToken = Core::PropertyProviderToken;
using PropertyExpressionKind = Core::PropertyExpressionKind;

inline Base::Result<PropertyValueSourceInfo> GetValueSource(const DependencyObject& object, DependencyPropertyHandle property) noexcept {
    return object.GetValueSourceInfo(property);
}

} // namespace Aero::Diagnostics
