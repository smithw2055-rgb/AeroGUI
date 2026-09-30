#pragma once

#include <Aero/Base/Result.hpp>

namespace Aero::Meta {
class Registration;
class Registry;
}

namespace Aero::Controls {

Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept;

Base::Result<void> RegisterControlsMetadata(
    ::Aero::Meta::Registry& domain) noexcept;

} // namespace Aero::Controls
