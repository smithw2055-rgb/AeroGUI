#pragma once

#include <Aero/Media/Effect.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Media {

class AERO_GUI_API TintEffect : public Effect {
    AERO_DECLARE_TYPE(TintEffect, Effect)
public:
    TintEffect() noexcept : Effect(StaticTypeId()) {}

    Base::Color GetColor() const noexcept {
        return GetValueOr(ColorProperty, Base::Color{0.0F, 0.0F, 1.0F, 1.0F});
    }
    void SetColor(Base::Color value) noexcept {
        SetValue(ColorProperty, value);
    }

    inline static constexpr DependencyProperty<Base::Color> ColorProperty{"Color"};
};
} // namespace Aero::Media
