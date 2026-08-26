#pragma once

#include <Aero/Media/Effect.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Media {

class AERO_GUI_API DirectionalBlurEffect : public Effect {
    AERO_DECLARE_TYPE(DirectionalBlurEffect, Effect)
public:
    DirectionalBlurEffect() noexcept : Effect(StaticTypeId()) {}

    double GetRadius() const noexcept {
        return GetValueOr(RadiusProperty, 0.0);
    }
    void SetRadius(double value) noexcept {
        SetValue(RadiusProperty, value);
    }
    double GetAngle() const noexcept {
        return GetValueOr(AngleProperty, 0.0);
    }
    void SetAngle(double value) noexcept {
        SetValue(AngleProperty, value);
    }

    inline static constexpr DependencyProperty<double> RadiusProperty{"Radius"};
    inline static constexpr DependencyProperty<double> AngleProperty{"Angle"};
};
} // namespace Aero::Media
