#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Media/BrushShader.hpp>
#include <Aero/Media/GradientStop.hpp>

namespace Aero::Media {

class AERO_GUI_API ConicGradientShader : public BrushShader {
    AERO_DECLARE_TYPE(ConicGradientShader, BrushShader)
public:
    ConicGradientShader() noexcept
        : BrushShader(StaticTypeId()),
          stops_(&Base::GetDefaultAllocator()) {}
    void AddGradientStop(Ref<GradientStop> value) noexcept {
        if (!value) { AERO_ASSERT(false); return; }
        Base::Result<void> pushed = stops_.PushBack(std::move(value));
        if (!pushed) { AERO_ASSERT(false); return; }
    }
    void ClearGradientStops() noexcept { stops_.Clear(); }
    Span<const Ref<GradientStop>> GetGradientStops() const noexcept {
        return stops_.AsSpan();
    }
private:
    Base::Vector<Ref<GradientStop>> stops_;
};
} // namespace Aero::Media
