#pragma once

#include <Aero/Presentation/Brushes.hpp>

namespace Aero::Detail {

class ImageBrushAccess final {
public:
    static Presentation::RenderImageId RuntimeImage(
        const Presentation::ImageBrush& brush) noexcept {
        return brush.renderImage_;
    }

    static std::uint32_t PixelWidth(
        const Presentation::ImageBrush& brush) noexcept {
        return brush.pixelWidth_;
    }

    static std::uint32_t PixelHeight(
        const Presentation::ImageBrush& brush) noexcept {
        return brush.pixelHeight_;
    }

    static Base::Result<void> SetRuntimeImage(
        Presentation::ImageBrush& brush,
        Presentation::RenderImageId image,
        std::uint32_t width,
        std::uint32_t height) noexcept {
        const bool changed =
            brush.renderImage_ != image ||
            brush.pixelWidth_ != width ||
            brush.pixelHeight_ != height;
        brush.renderImage_ = image;
        brush.pixelWidth_ = width;
        brush.pixelHeight_ = height;
        Presentation::FrameworkElement* owner =
            brush.Owner();
        return changed && owner != nullptr
            ? owner->InvalidateRender()
            : Base::Result<void>();
    }
};

} // namespace Aero::Detail
