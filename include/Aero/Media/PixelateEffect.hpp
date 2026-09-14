#pragma once

#include <Aero/Media/Effect.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Media {

class AERO_GUI_API PixelateEffect : public Effect {
    AERO_DECLARE_TYPE(PixelateEffect, Effect)
public:
    PixelateEffect() noexcept : Effect(StaticTypeId()) {}

    double GetSize() const noexcept;
    void SetSize(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, Size);
};
} // namespace Aero::Media
