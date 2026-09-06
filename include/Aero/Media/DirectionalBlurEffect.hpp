#pragma once

#include <Aero/Media/Effect.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Media {

class AERO_GUI_API DirectionalBlurEffect : public Effect {
    AERO_DECLARE_TYPE(DirectionalBlurEffect, Effect)
public:
    DirectionalBlurEffect() noexcept : Effect(StaticTypeId()) {}

    double GetRadius() const noexcept;
    void SetRadius(double value) noexcept;
    double GetAngle() const noexcept;
    void SetAngle(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, Radius);
    AERO_DEPENDENCY_PROPERTY(double, Angle);
};
} // namespace Aero::Media
