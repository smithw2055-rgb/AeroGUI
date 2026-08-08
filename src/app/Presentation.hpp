#pragma once

#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::App {

// Platform-neutral value passed into a concrete desktop RenderContext. It is
// deliberately only geometry: native ownership, capabilities and frame state
// belong to the selected context itself.
struct PresentationSize {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

Base::Result<void> ValidatePresentationSize(
    PresentationSize size,
    std::uint32_t maximumDimension = 16384U) noexcept;

} // namespace Aero::App
