#include "gui/core/EnumRegistration.hpp"

#include <Aero/Shapes.hpp>

namespace Aero {

void PopulateShapesEnums(
    Meta::Registration& context) noexcept {
    using namespace Shapes;

    AERO_AUTO_REGISTER_ENUM(FillRule);
}

} // namespace Aero
