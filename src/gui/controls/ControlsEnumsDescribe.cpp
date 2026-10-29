#include "gui/core/EnumRegistration.hpp"

#include <Aero/Controls.hpp>

namespace Aero {

void PopulateControlsEnums(
    Meta::Registration& context) noexcept {
    using namespace Controls;
    using namespace Controls::Primitives;

    AERO_AUTO_REGISTER_ENUM(Orientation);
    AERO_AUTO_REGISTER_ENUM(Dock);
    AERO_AUTO_REGISTER_ENUM(MenuItemRole);
    AERO_AUTO_REGISTER_ENUM(ClickMode);
    AERO_AUTO_REGISTER_ENUM(TickPlacement);
    AERO_AUTO_REGISTER_ENUM(TickBarPlacement);
    AERO_AUTO_REGISTER_ENUM(ScrollBarVisibility);
    AERO_AUTO_REGISTER_ENUM(PanningMode);
    AERO_AUTO_REGISTER_ENUM(GridResizeDirection);
    AERO_AUTO_REGISTER_ENUM(GridResizeBehavior);
    AERO_AUTO_REGISTER_ENUM(SelectionMode);
    AERO_AUTO_REGISTER_ENUM(ExpandDirection);
    AERO_AUTO_REGISTER_ENUM(PlacementMode);
    AERO_AUTO_REGISTER_ENUM(PopupAnimation);
    AERO_AUTO_REGISTER_ENUM(GridViewColumnHeaderRole);
    AERO_AUTO_REGISTER_ENUM(ScrollUnit);
    AERO_AUTO_REGISTER_ENUM(VirtualizationMode);
    AERO_AUTO_REGISTER_ENUM(VirtualizationCacheLengthUnit);
}

} // namespace Aero
