#pragma once

#include <Aero/Styling.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero::Controls::Detail {

// Private bridge used by the XAML template compiler. Registry validation is a
// runtime concern and is intentionally absent from FrameworkTemplate's API.
class FrameworkTemplateAccess final {
public:
    static Base::Result<void> Seal(
        FrameworkTemplate& value,
        const Core::DependencyPropertyRegistry& properties) noexcept {
        return value.SealRuntime(&properties);
    }
};

} // namespace Aero::Controls::Detail
