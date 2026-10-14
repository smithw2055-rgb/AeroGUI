#pragma once

// EasingFunctionBase plus the easing cartesian family.
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
    EasingMode GetEasingMode() const noexcept { return GetValue(EasingModeProperty); }
    void SetEasingMode(EasingMode value) noexcept { SetValue(EasingModeProperty, value); }
    AERO_DEPENDENCY_PROPERTY(EasingMode, EasingMode);

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

    Kind GetKind() const noexcept { return kind_; }

    // XAML names (SineEase, CubicEase, ...) are metadata aliases. Each factory
    // constructs this class with that name's TypeId and the matching Kind.
    EasingFunctionBase(Meta::TypeId runtimeType, Kind kind) noexcept : Freezable(runtimeType), kind_(kind) {}

    double GetExponent() const noexcept { return GetValue(ExponentProperty); }
    double GetPower() const noexcept { return GetValue(PowerProperty); }
    double GetAmplitude() const noexcept { return GetValue(AmplitudeProperty); }
    double GetBounces() const noexcept { return GetValue(BouncesProperty); }
    double GetBounciness() const noexcept { return GetValue(BouncinessProperty); }
    double GetOscillations() const noexcept { return GetValue(OscillationsProperty); }
    double GetSpringiness() const noexcept { return GetValue(SpringinessProperty); }
    void SetExponent(double value) noexcept;
    void SetPower(double value) noexcept;
    void SetAmplitude(double value) noexcept;
    void SetBounces(double value) noexcept;
    void SetBounciness(double value) noexcept;
    void SetOscillations(double value) noexcept;
    void SetSpringiness(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, Exponent);
    AERO_DEPENDENCY_PROPERTY(double, Power);
    AERO_DEPENDENCY_PROPERTY(double, Amplitude);
    AERO_DEPENDENCY_PROPERTY(double, Bounces);
    AERO_DEPENDENCY_PROPERTY(double, Bounciness);
    AERO_DEPENDENCY_PROPERTY(double, Oscillations);
    AERO_DEPENDENCY_PROPERTY(double, Springiness);

private:
    Kind kind_ = Kind::Linear;
};

} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_ENUM(Aero::Media::Animation::EasingMode)
