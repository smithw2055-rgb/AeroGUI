#pragma once

#include <Aero/Controls/Controls.hpp>
#include <Aero/Metadata.hpp>
#include <Aero/Module.hpp>

namespace Aero {
class RuntimeEnvironment;
}

namespace Aero::Samples::ControlGallery {

inline constexpr Base::StringView GalleryNamespace() noexcept {
    return Base::StringView("urn:aero-control-gallery");
}

class StatusBadge final
    : public Controls::ContentControl {
    AERO_DECLARE_TYPE_NAMED(
        StatusBadge,
        Controls::ContentControl,
        "urn:aero-control-gallery",
        "StatusBadge")
public:
    StatusBadge() noexcept
        : ContentControl(StaticTypeId()) {}

    Presentation::Color Accent() const noexcept;
    Base::Result<void> SetAccent(
        Presentation::Color value) noexcept;

    inline static constexpr Members::Property<Presentation::Color>
        AccentProperty{"Accent"};

protected:
    Base::Result<void> BuildDisplayList(
        Presentation::DisplayListBuilder& builder) noexcept override;
};

Aero::ModuleRegistration
MakeStatusBadgeModuleManifest() noexcept;

} // namespace Aero::Samples::ControlGallery
