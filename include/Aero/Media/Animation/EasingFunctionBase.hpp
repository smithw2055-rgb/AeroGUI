#pragma once

#include <Aero/DependencyProperty.hpp>
#include <Aero/Freezable.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

enum class EasingMode : std::uint8_t {
    EaseOut = 0U,
    EaseIn,
    EaseInOut
};

class AERO_GUI_API EasingFunctionBase : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(EasingFunctionBase, ::Aero::Freezable)
public:
    EasingMode GetEasingMode() const noexcept {
        return GetValueOr(EasingModeProperty, EasingMode::EaseOut);
    }
    void SetEasingMode(EasingMode value) noexcept {
        SetValue(EasingModeProperty, value);
    }
    inline static constexpr DependencyProperty<EasingMode> EasingModeProperty{"EasingMode"};

    enum class Kind : std::uint8_t {
        Linear = 0U,
        Sine,
        Quadratic,
        Cubic,
        Quartic,
        Quintic,
        Circle,
        Power,
        Exponential,
        Back,
        Bounce,
        Elastic
    };

protected:
    EasingFunctionBase(Meta::TypeId runtimeType, Kind kind) noexcept
        : Freezable(runtimeType), kind_(kind) {}

private:
    friend struct TimelineRuntime;
    Kind kind_ = Kind::Linear;
};

} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_ENUM(Aero::Media::Animation::EasingMode)
