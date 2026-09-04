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
    Result<void> AddGradientStop(Ref<GradientStop> value) noexcept {
        return value ? stops_.PushBack(std::move(value))
            : Result<void>(Base::Status::Failure(
                Base::ErrorCode::InvalidArgument, "ConicGradientShader stop is null"));
    }
    void ClearGradientStops() noexcept { stops_.Clear(); }
    Span<const Ref<GradientStop>> GetGradientStops() const noexcept {
        return stops_.AsSpan();
    }
private:
    Base::Vector<Ref<GradientStop>> stops_;
};
} // namespace Aero::Media
