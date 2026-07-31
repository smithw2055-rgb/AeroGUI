#pragma once

#include <Aero/Controls/Images.hpp>

namespace Aero::Detail {

class ImageControlAccess final {
public:
    static Base::Result<void> SetRuntimeImage(
        Controls::Image& image,
        Render::RenderImageId renderImage,
        std::uint32_t pixelWidth,
        std::uint32_t pixelHeight) noexcept {
        const bool measureChanged =
            image.pixelWidth_ != pixelWidth ||
            image.pixelHeight_ != pixelHeight;
        const bool renderChanged =
            image.renderImage_ != renderImage;
        image.renderImage_ = renderImage;
        image.pixelWidth_ = pixelWidth;
        image.pixelHeight_ = pixelHeight;
        if (measureChanged) {
            return image.InvalidateMeasure();
        }
        return renderChanged
            ? image.InvalidateRender()
            : Base::Result<void>();
    }
};

} // namespace Aero::Detail
