#include "gui/core/EnumRegistration.hpp"

#include <Aero/ElementEnums.hpp>

namespace Aero {

Base::Result<void> PopulateElementEnums(
    Meta::Registration& context) noexcept {


    Base::Result<void> status;

    AERO_REGISTER_ENUM(
        HorizontalAlignment,
        "HorizontalAlignment",
        description
            .Value("Stretch", HorizontalAlignment::Stretch)
            .Value("Left", HorizontalAlignment::Left)
            .Value("Center", HorizontalAlignment::Center)
            .Value("Right", HorizontalAlignment::Right););
    AERO_REGISTER_ENUM(
        VerticalAlignment,
        "VerticalAlignment",
        description
            .Value("Stretch", VerticalAlignment::Stretch)
            .Value("Top", VerticalAlignment::Top)
            .Value("Center", VerticalAlignment::Center)
            .Value("Bottom", VerticalAlignment::Bottom););
    AERO_REGISTER_ENUM(
        Visibility,
        "Visibility",
        description
            .Value("Visible", Visibility::Visible)
            .Value("Hidden", Visibility::Hidden)
            .Value("Collapsed", Visibility::Collapsed););
    AERO_REGISTER_ENUM(
        BlendMode,
        "BlendMode",
        description
            .Value("Normal", BlendMode::Normal)
            .Value("Multiply", BlendMode::Multiply)
            .Value("Screen", BlendMode::Screen)
            .Value("Additive", BlendMode::Additive););
    AERO_REGISTER_ENUM(
        FlowDirection,
        "FlowDirection",
        description
            .Value("LeftToRight", FlowDirection::LeftToRight)
            .Value("RightToLeft", FlowDirection::RightToLeft););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero
