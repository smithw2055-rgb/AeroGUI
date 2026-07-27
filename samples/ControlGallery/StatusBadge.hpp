#pragma once

#include <Aero/Controls/Controls.hpp>
#include <Aero/Module.hpp>
#include <Aero/Type.hpp>

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

    inline static constexpr auto AccentProperty =
        Members::Property<Presentation::Color>{"Accent"};

protected:
    Base::Result<void> BuildDisplayList(
        Presentation::DisplayListBuilder& builder) noexcept override;
};

Aero::ModuleRegistration
MakeStatusBadgeModuleManifest() noexcept;
Base::Result<void> RegisterControlGalleryModules(
    Aero::ModuleCatalog& modules) noexcept;

} // namespace Aero::Samples::ControlGallery
