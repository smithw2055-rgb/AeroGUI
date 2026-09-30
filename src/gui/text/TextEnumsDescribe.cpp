#include "gui/core/EnumRegistration.hpp"

#include <Aero/TextFormatting.hpp>

namespace Aero {

Base::Result<void> PopulateTextEnums(
    Meta::Registration& context) noexcept {
    using namespace Controls;

    Base::Result<void> status;

    AERO_REGISTER_ENUM(
        TextWrapping,
        "TextWrapping",
        description
            .Value("NoWrap", TextWrapping::NoWrap)
            .Value("Wrap", TextWrapping::Wrap)
            .Value("WrapWithOverflow", TextWrapping::WrapWithOverflow););
    AERO_REGISTER_ENUM(
        TextTrimming,
        "TextTrimming",
        description
            .Value("None", TextTrimming::None)
            .Value("CharacterEllipsis", TextTrimming::CharacterEllipsis)
            .Value("WordEllipsis", TextTrimming::WordEllipsis););
    AERO_REGISTER_ENUM(
        TextAlignment,
        "TextAlignment",
        description
            .Value("Left", TextAlignment::Left)
            .Value("Center", TextAlignment::Center)
            .Value("Right", TextAlignment::Right)
            .Value("Justify", TextAlignment::Justify););
    AERO_REGISTER_ENUM(
        FontStyle,
        "FontStyle",
        description
            .Value("Normal", FontStyle::Normal)
            .Value("Italic", FontStyle::Italic)
            .Value("Oblique", FontStyle::Oblique););
    AERO_REGISTER_ENUM(
        FontWeight,
        "FontWeight",
        description
            .Value("Normal", FontWeight::Normal)
            .Value("SemiBold", FontWeight::SemiBold)
            .Value("Bold", FontWeight::Bold)
            .Value("Regular", FontWeight::Regular););
    AERO_REGISTER_ENUM(
        TextDecorations,
        "TextDecorations",
        description
            .Value("None", TextDecorations::None)
            .Value("Underline", TextDecorations::Underline););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero
