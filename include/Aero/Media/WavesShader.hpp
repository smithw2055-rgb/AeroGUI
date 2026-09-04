#pragma once

#include <Aero/Media/BrushShader.hpp>

namespace Aero::Media {

class AERO_GUI_API WavesShader : public BrushShader {
    AERO_DECLARE_TYPE(WavesShader, BrushShader)
public:
    WavesShader() noexcept : BrushShader(StaticTypeId()) {}
    double GetTime() const noexcept {
        return GetValue(TimeProperty);
    }
    void SetTime(double value) noexcept {
        SetValue(TimeProperty, value);
    }
    inline static constexpr DependencyProperty<double> TimeProperty{"Time"};
};
} // namespace Aero::Media
