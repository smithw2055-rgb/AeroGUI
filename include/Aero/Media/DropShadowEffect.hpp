#pragma once

#include <Aero/Media/Effect.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Media {

class AERO_GUI_API DropShadowEffect : public Effect {
    AERO_DECLARE_TYPE(DropShadowEffect, Effect)
public:
    DropShadowEffect() noexcept : Effect(StaticTypeId()) {}

    double GetBlurRadius() const noexcept;
    double GetDirection() const noexcept;
    double GetShadowDepth() const noexcept;
    double GetOpacity() const noexcept;
    Base::Color GetColor() const noexcept;

    void SetBlurRadius(double value) noexcept;
    void SetDirection(double value) noexcept;
    void SetShadowDepth(double value) noexcept;
    void SetOpacity(double value) noexcept;
    void SetColor(Base::Color value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, BlurRadius);
    AERO_DEPENDENCY_PROPERTY(double, Direction);
    AERO_DEPENDENCY_PROPERTY(double, ShadowDepth);
    AERO_DEPENDENCY_PROPERTY(double, Opacity);
    AERO_DEPENDENCY_PROPERTY(Base::Color, Color);
};
} // namespace Aero::Media
