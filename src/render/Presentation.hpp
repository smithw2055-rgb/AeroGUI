#pragma once

#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::Render {

// Platform-neutral value passed into a concrete desktop RenderContext. It is
// deliberately only geometry: native ownership, capabilities and frame state
// belong to the selected context itself.
struct PresentationSize {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

inline Base::Result<void> ValidatePresentationSize(
    PresentationSize size,
    std::uint32_t maximumDimension = 16384U) noexcept {
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

} // namespace Aero::Render