#include "Presentation.hpp"

namespace Aero::App {

Base::Result<void> ValidatePresentationSize(
    PresentationSize size,
    std::uint32_t maximumDimension) noexcept {
    if (size.width == 0U || size.height == 0U ||
        maximumDimension == 0U ||
        size.width > maximumDimension ||
        size.height > maximumDimension) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Presentation dimensions are outside the backend limits");
    }
    return {};
}

} // namespace Aero::App
