#include "gui/core/EnumRegistration.hpp"

#include <Aero/ElementEnums.hpp>

namespace Aero {

void PopulateElementEnums(
    Meta::Registration& context) noexcept {
    AERO_AUTO_REGISTER_ENUM(HorizontalAlignment);
    AERO_AUTO_REGISTER_ENUM(VerticalAlignment);
    AERO_AUTO_REGISTER_ENUM(Visibility);
    AERO_AUTO_REGISTER_ENUM(BlendMode);
    AERO_AUTO_REGISTER_ENUM(FlowDirection);
}

} // namespace Aero
