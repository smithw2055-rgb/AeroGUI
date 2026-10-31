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
