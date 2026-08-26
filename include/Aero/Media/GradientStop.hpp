#pragma once

#include <Aero/Media/Brush.hpp>

namespace Aero::Media {

class AERO_GUI_API GradientStop : public Freezable {
    AERO_DECLARE_TYPE(GradientStop, Freezable)
public:
    GradientStop() noexcept
        : Freezable(StaticTypeId()) {}
    ~GradientStop() override = default;

    double GetOffset() const noexcept;
    Color GetColor() const noexcept;
    void SetOffset(double value) noexcept;
    void SetColor(Color value) noexcept;

    inline static constexpr DependencyProperty<double> OffsetProperty{"Offset"};
    inline static constexpr DependencyProperty<Color> ColorProperty{"Color"};

};
} // namespace Aero::Media
