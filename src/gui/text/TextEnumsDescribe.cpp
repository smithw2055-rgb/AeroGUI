#include "gui/core/EnumRegistration.hpp"

#include <Aero/TextFormatting.hpp>

namespace Aero {

void PopulateTextEnums(
    Meta::Registration& context) noexcept {
    using namespace Controls;

    AERO_AUTO_REGISTER_ENUM(TextWrapping);
    AERO_AUTO_REGISTER_ENUM(TextTrimming);
    AERO_AUTO_REGISTER_ENUM(TextAlignment);
    AERO_AUTO_REGISTER_ENUM(FontStyle);
    AERO_AUTO_REGISTER_ENUM(FontWeight);
    AERO_AUTO_REGISTER_ENUM(TextDecorations);
}

} // namespace Aero
