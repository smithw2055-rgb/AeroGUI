#pragma once

#include <Aero/Style.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero::Detail {

// Private bridge used by XAML and runtime style compilation. Dependency-property
// registries are implementation state and never appear in the Style SDK.
class StyleAccess final {
public:
    static Base::Result<void> Seal(
        Aero::Style& style,
        const Core::DependencyPropertyRegistry& properties) noexcept {
        return style.SealRuntime(&properties);
    }
};

} // namespace Aero::Detail
