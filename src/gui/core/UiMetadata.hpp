#pragma once

// Ordered installers for builtin UI metadata. Each Describe body lives next
// to the type it registers; these functions only keep the frozen call order.

#include <Aero/Base/Result.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero {

Base::Result<void> PopulateUiInput(
    ::Aero::Meta::Registration& context) noexcept;
Base::Result<void> PopulateUiMedia(
    ::Aero::Meta::Registration& context) noexcept;
Base::Result<void> PopulateUiResources(
    ::Aero::Meta::Registration& context) noexcept;
Base::Result<void> PopulateUiStyling(
    ::Aero::Meta::Registration& context) noexcept;
Base::Result<void> PopulateUiAnimation(
    ::Aero::Meta::Registration& context) noexcept;
Base::Result<void> PopulateInputDevices(
    ::Aero::Meta::Registration& context) noexcept;

} // namespace Aero
