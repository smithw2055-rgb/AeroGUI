#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>
#include <Aero/Media/Animation/KeyTime.hpp>
#include <Aero/Media/Animation/Timeline.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

class AERO_GUI_API KeyFrameBase : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(KeyFrameBase, ::Aero::Freezable)
public:
    enum class Interpolation : std::uint8_t {
        Linear = 0U,
        Discrete,
        Easing,
        Spline
    };

    KeyTime GetKeyTime() const noexcept {
        return GetValueOr(KeyTimeProperty, KeyTime{});
    }
    void SetKeyTime(KeyTime value) noexcept;
    void SetKeyTime(StringView value) noexcept;
    AnimationTime GetKeyTimeMicroseconds() const noexcept {
        return GetKeyTime().GetTimeSpan().Microseconds();
    }

    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return GetValueOr(EasingFunctionProperty, Ref<EasingFunctionBase>{});
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

    StringView GetKeySpline() const noexcept {
        return GetValueOr(KeySplineProperty, StringView{});
    }
    void SetKeySpline(StringView value) noexcept;

    Interpolation GetInterpolation() const noexcept { return interpolation_; }
    double GetSplineControlPoint1X() const noexcept { return controlPoint1X_; }
    double GetSplineControlPoint1Y() const noexcept { return controlPoint1Y_; }
    double GetSplineControlPoint2X() const noexcept { return controlPoint2X_; }
    double GetSplineControlPoint2Y() const noexcept { return controlPoint2Y_; }

    inline static constexpr DependencyProperty<KeyTime> KeyTimeProperty{"KeyTime"};
    inline static constexpr DependencyProperty<Ref<EasingFunctionBase>>
        EasingFunctionProperty{"EasingFunction"};
    inline static constexpr DependencyProperty<String> KeySplineProperty{"KeySpline"};

    static void OnKeySplineChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;

protected:
    KeyFrameBase(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : Freezable(runtimeType), interpolation_(interpolation) {}

private:
    Interpolation interpolation_ = Interpolation::Linear;
    double controlPoint1X_ = 0.0;
    double controlPoint1Y_ = 0.0;
    double controlPoint2X_ = 1.0;
    double controlPoint2Y_ = 1.0;
};

} // namespace Aero::Media::Animation
