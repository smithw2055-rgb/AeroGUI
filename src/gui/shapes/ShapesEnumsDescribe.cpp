#include "gui/core/EnumRegistration.hpp"

#include <Aero/Shapes.hpp>

namespace Aero {

Base::Result<void> PopulateShapesEnums(
    Meta::Registration& context) noexcept {
    using namespace Shapes;

    Base::Result<void> status;

    AERO_REGISTER_ENUM(
        FillRule,
        "FillRule",
        description
            .Value("EvenOdd", FillRule::EvenOdd)
            .Value("Nonzero", FillRule::Nonzero););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero
