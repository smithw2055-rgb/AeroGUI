#include <Aero/Text/TextTypes.hpp>

namespace Aero::Text {

Base::Result<void> Typeface::TrySetFamily(
    Base::StringView family) noexcept {
    if (family.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Typeface family must not be empty");
    }
    return family_.TryAssign(family);
}

Base::Result<void> Typeface::TrySetLanguage(
    Base::StringView language) noexcept {
    return language_.TryAssign(language);
}

Base::Result<void> Typeface::TrySetWeight(
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

// Unicode analysis is compiled into the existing AeroText target without
// changing target topology.
#include "UnicodeRuntime.cpp"
