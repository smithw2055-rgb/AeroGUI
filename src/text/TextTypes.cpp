#include "TextTypes.hpp"

namespace Aero::Text {

Base::Result<void> Typeface::SetFamily(
    Base::StringView family) noexcept {
    if (family.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Typeface family must not be empty");
    }
    return family_.Assign(family);
}

Base::Result<void> Typeface::SetLanguage(
    Base::StringView language) noexcept {
    return language_.Assign(language);
}

Base::Result<void> Typeface::SetWeight(
    std::uint16_t weight) noexcept {
    if (weight < 1U || weight > 1000U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Typeface weight must be in the range 1 through 1000");
    }
    weight_ = weight;
    return {};
}

} // namespace Aero::Text
