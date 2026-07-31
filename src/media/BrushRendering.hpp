#pragma once

#include "../render/DisplayList.hpp"

#include <Aero/Media/Brushes.hpp>
#include <Aero/FrameworkElement.hpp>

namespace Aero::Media {

Base::Result<void> PaintBrushRect(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Rect bounds,
    double cornerRadius = 0.0) noexcept;

} // namespace Aero::Media
