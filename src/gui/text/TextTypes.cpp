#include "TextTypes.hpp"

namespace Aero::Text {

void Typeface::SetFamily(
    Base::StringView family) noexcept {
    if (family.Empty()) {
        family_.Clear();
        return;
    }
    family_.Assign(family);
}

void Typeface::SetLanguage(
    Base::StringView language) noexcept {
    language_.Assign(language);
}

void Typeface::SetWeight(
    std::uint16_t weight) noexcept {
    if (weight >= 1U && weight <= 1000U) {
        weight_ = weight;
    }
}

} // namespace Aero::Text

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
