#pragma once

#include "../render/DisplayList.hpp"

#include <Aero/Media/Brushes.hpp>

namespace Aero::Detail {

class BrushPrivate final {
public:
    static Render::RenderImageId RuntimeImage(
        const Media::ImageBrush& brush) noexcept {
        return brush.renderImage_;
    }

    static std::uint32_t PixelWidth(
        const Media::ImageBrush& brush) noexcept {
        return brush.pixelWidth_;
    }

    static std::uint32_t PixelHeight(
        const Media::ImageBrush& brush) noexcept {
        return brush.pixelHeight_;
    }

    static Base::Result<void> SetRuntimeImage(
        Media::ImageBrush& brush,
        Render::RenderImageId image,
        std::uint32_t width,
        std::uint32_t height) noexcept {
        const bool changed =
            brush.renderImage_ != image ||
            brush.pixelWidth_ != width ||
            brush.pixelHeight_ != height;
        brush.renderImage_ = image;
        brush.pixelWidth_ = width;
        brush.pixelHeight_ = height;
        Aero::FrameworkElement* owner =
            brush.Owner();
        return changed && owner != nullptr
            ? owner->InvalidateVisual()
            : Base::Result<void>();
    }
};

} // namespace Aero::Detail
