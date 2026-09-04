#pragma once

#include <Aero/Media/TileBrush.hpp>
#include <Aero/Media/Images.hpp>

namespace Aero::Media {

class AERO_GUI_API ImageBrush : public TileBrush {
    AERO_DECLARE_TYPE(ImageBrush, TileBrush)
public:
    ImageBrush() noexcept
        : TileBrush(StaticTypeId()) {}
    ~ImageBrush() override = default;

    Ref<ImageSource> GetSource() const noexcept;
    void SetSource(Ref<ImageSource> value) noexcept;

    inline static constexpr DependencyProperty<Ref<ImageSource>> ImageSourceProperty{"ImageSource"};

    std::uint64_t GetRenderImageId() const noexcept { return renderImage_; }
    std::uint32_t GetPixelWidth() const noexcept { return pixelWidth_; }
    std::uint32_t GetPixelHeight() const noexcept { return pixelHeight_; }
    Result<void> SetRuntimeImage(
        std::uint64_t image, std::uint32_t width, std::uint32_t height) noexcept;

private:
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};
} // namespace Aero::Media
