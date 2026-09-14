#pragma once

#include <Aero/Media/Effect.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Media {

class AERO_GUI_API TintEffect : public Effect {
    AERO_DECLARE_TYPE(TintEffect, Effect)
public:
    TintEffect() noexcept : Effect(StaticTypeId()) {}

    Base::Color GetColor() const noexcept;
    void SetColor(Base::Color value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Base::Color, Color);
};
} // namespace Aero::Media
