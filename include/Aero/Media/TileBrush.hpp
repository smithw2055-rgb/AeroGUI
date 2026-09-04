#pragma once

#include <Aero/Media/Brush.hpp>
#include <Aero/Media/Images.hpp>

namespace Aero::Media {

class AERO_GUI_API TileBrush : public Brush {
    AERO_DECLARE_TYPE(TileBrush, Brush)
public:
    Stretch GetStretch() const noexcept;
    Rect GetViewbox() const noexcept;
    Rect GetViewport() const noexcept;
    BrushMappingMode GetViewboxUnits() const noexcept;
    BrushMappingMode GetViewportUnits() const noexcept;
    TileMode GetTileMode() const noexcept;
    HorizontalAlignment GetAlignmentX() const noexcept;
    VerticalAlignment GetAlignmentY() const noexcept;

    void SetStretch(Stretch value) noexcept;
    void SetViewbox(Rect value) noexcept;
    void SetViewport(Rect value) noexcept;
    void SetViewboxUnits(BrushMappingMode value) noexcept;
    void SetViewportUnits(BrushMappingMode value) noexcept;
    void SetTileMode(TileMode value) noexcept;
    void SetAlignmentX(HorizontalAlignment value) noexcept;
    void SetAlignmentY(VerticalAlignment value) noexcept;

    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};
    inline static constexpr DependencyProperty<Rect> ViewboxProperty{"Viewbox"};
    inline static constexpr DependencyProperty<Rect> ViewportProperty{"Viewport"};
    inline static constexpr DependencyProperty<BrushMappingMode> ViewboxUnitsProperty{"ViewboxUnits"};
    inline static constexpr DependencyProperty<BrushMappingMode> ViewportUnitsProperty{"ViewportUnits"};
    inline static constexpr DependencyProperty<TileMode> TileModeProperty{"TileMode"};
    inline static constexpr DependencyProperty<HorizontalAlignment> AlignmentXProperty{"AlignmentX"};
    inline static constexpr DependencyProperty<VerticalAlignment> AlignmentYProperty{"AlignmentY"};

protected:
    explicit TileBrush(TypeId runtimeType) noexcept
        : Brush(runtimeType) {}
};

} // namespace Aero::Media
