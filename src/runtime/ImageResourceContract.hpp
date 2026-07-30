#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <cstdint>

namespace Aero::Detail {

struct ImageBackendServices final {
    std::uint64_t generation = 0U;
    void* context = nullptr;
    Base::Result<Presentation::RenderImageId>
        (*createImage)(
            void* context,
            std::uint32_t width,
            std::uint32_t height,
            Base::Span<const std::uint8_t>
                rgbaPixels) noexcept = nullptr;
    void (*releaseImage)(
        void* context,
        Presentation::RenderImageId image) noexcept =
        nullptr;
};

} // namespace Aero::Detail
