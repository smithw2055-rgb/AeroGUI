#pragma once

#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {

using ::Aero::Meta::TypeId;

enum class ScrollUnit : std::uint8_t { Item = 0U, Pixel };

enum class VirtualizationMode : std::uint8_t { Standard = 0U, Recycling };

// WPF attached-property owner shared by all virtualizing panels. The current
// panel implementation is pixel-based; exposing this owner preserves the
// authored contract while item-unit realization is added.
class AERO_GUI_API VirtualizingPanel : public Panel {
    AERO_DECLARE_TYPE(VirtualizingPanel, Panel)
public:
    inline static constexpr AttachedProperty<ScrollUnit> ScrollUnitProperty{"ScrollUnit"};
    inline static constexpr AttachedProperty<VirtualizationMode> VirtualizationModeProperty{"VirtualizationMode"};

protected:
    explicit VirtualizingPanel(TypeId runtimeType) noexcept
        : Panel(runtimeType) {}
    ~VirtualizingPanel() override = default;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::ScrollUnit)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::VirtualizationMode)
