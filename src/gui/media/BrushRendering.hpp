#pragma once

#include "render/DisplayList.hpp"

#include <Aero/Media/Brushes.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/TryCast.hpp>

namespace Aero::Media {

inline bool IsSpatialGradientBrush(const Brush* brush) noexcept {
    return TryCast<LinearGradientBrush>(brush) != nullptr ||
        TryCast<RadialGradientBrush>(brush) != nullptr;
}

Base::Result<void> PaintBrushRect(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Rect bounds,
    double cornerRadius = 0.0,
    bool isRtl = false) noexcept;

// Ellipse/rounded-rect pen as an annulus (same construction as StrokeRect).
// LinearGradientBrush maps through `bounds`; do not Flatten+tessellate.
Base::Result<void> PaintBrushRoundedStroke(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Rect bounds,
    double thickness,
    double cornerRadius,
    bool isRtl = false) noexcept;

Base::Result<void> PaintBrushGeometry(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Base::Span<const Point> vertices,
    Base::Span<const std::uint32_t> indices,
    Rect bounds,
    bool isRtl = false,
    Render::RenderMeshId mesh = Render::InvalidRenderMeshId) noexcept;

} // namespace Aero::Media
