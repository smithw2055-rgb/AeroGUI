#pragma once

#include <Aero/Media/BrushShader.hpp>

namespace Aero::Media {

class AERO_GUI_API MonochromeShader : public BrushShader {
    AERO_DECLARE_TYPE(MonochromeShader, BrushShader)
public:
    MonochromeShader() noexcept : BrushShader(StaticTypeId()) {}
    Color GetColor() const noexcept {
        return GetValue(ColorProperty);
    }
    void SetColor(Color value) noexcept {
        SetValue(ColorProperty, value);
    }
    inline static constexpr DependencyProperty<Color> ColorProperty{"Color"};
};
} // namespace Aero::Media
