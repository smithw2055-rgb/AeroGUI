#pragma once

#include <Aero/Media/Effect.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Media {

class AERO_GUI_API BlurEffect : public Effect {
    AERO_DECLARE_TYPE(BlurEffect, Effect)
public:
    BlurEffect() noexcept : Effect(StaticTypeId()) {}

    double GetRadius() const noexcept;
    void SetRadius(double value) noexcept;

    inline static constexpr DependencyProperty<double> RadiusProperty{"Radius"};
};
} // namespace Aero::Media
