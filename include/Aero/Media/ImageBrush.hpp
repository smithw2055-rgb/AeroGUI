#pragma once

#include <Aero/Media/Brush.hpp>
#include <Aero/Media/Images.hpp>

namespace Aero::Media {

class AERO_GUI_API ImageBrush : public Brush {
    AERO_DECLARE_TYPE(ImageBrush, Brush)
public:
    ImageBrush() noexcept
        : Brush(StaticTypeId()) {}
    ~ImageBrush() override = default;

    Ref<ImageSource> GetSource() const noexcept;
    Stretch GetStretch() const noexcept;
    Rect GetViewbox() const noexcept;
    Rect GetViewport() const noexcept;
    BrushMappingMode GetViewboxUnits() const noexcept;
    BrushMappingMode GetViewportUnits() const noexcept;
    TileMode GetTileMode() const noexcept;
    HorizontalAlignment GetAlignmentX() const noexcept;
    VerticalAlignment GetAlignmentY() const noexcept;
    void SetSource(
        Ref<ImageSource> value) noexcept;
    void SetStretch(
        Stretch value) noexcept;
    void SetViewbox(
        Rect value) noexcept;
    void SetViewport(
        Rect value) noexcept;
    void SetViewboxUnits(
        BrushMappingMode value) noexcept;
    void SetViewportUnits(
        BrushMappingMode value) noexcept;
    void SetTileMode(
        TileMode value) noexcept;
    void SetAlignmentX(
        HorizontalAlignment value) noexcept;
    void SetAlignmentY(
        VerticalAlignment value) noexcept;

    inline static constexpr DependencyProperty<Ref<ImageSource>> ImageSourceProperty{"ImageSource"};
    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};
    inline static constexpr DependencyProperty<Rect> ViewboxProperty{"Viewbox"};
    inline static constexpr DependencyProperty<Rect> ViewportProperty{"Viewport"};
    inline static constexpr DependencyProperty<BrushMappingMode> ViewboxUnitsProperty{"ViewboxUnits"};
    inline static constexpr DependencyProperty<BrushMappingMode> ViewportUnitsProperty{"ViewportUnits"};
    inline static constexpr DependencyProperty<TileMode> TileModeProperty{"TileMode"};
    inline static constexpr DependencyProperty<HorizontalAlignment> AlignmentXProperty{"AlignmentX"};
    inline static constexpr DependencyProperty<VerticalAlignment> AlignmentYProperty{"AlignmentY"};

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
