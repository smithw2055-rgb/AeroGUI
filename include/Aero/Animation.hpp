#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Data.hpp>
#include <cstdint>

namespace Aero::Internal { class AnimationPrivate; }

namespace Aero::Media::Animation {

using AnimationTime = std::uint64_t;

enum class FillBehavior : std::uint8_t {
    HoldEnd = 0U,
    Stop
};

enum class EasingMode : std::uint8_t {
    EaseOut = 0U,
    EaseIn,
    EaseInOut
};

class AERO_API Timeline : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(Timeline, ::Aero::DependencyObject)
public:
    Base::StringView GetBeginTime() const noexcept {
        return beginTimeText_.View();
    }
    Base::StringView GetDuration() const noexcept {
        return durationText_.View();
    }
    Base::StringView GetRepeatBehavior() const noexcept {
        return repeatBehaviorText_.View();
    }
    double GetSpeedRatio() const noexcept { return speedRatio_; }
    bool GetAutoReverse() const noexcept { return autoReverse_; }
    FillBehavior GetFillBehavior() const noexcept {
        return fillBehavior_;
    }

    void SetBeginTime(Base::StringView value) noexcept;
    void SetDuration(Base::StringView value) noexcept;
    void SetRepeatBehavior(
        Base::StringView value) noexcept;
    void SetSpeedRatio(double value) noexcept;
    void SetAutoReverse(bool value) noexcept;
    void SetFillBehavior(FillBehavior value) noexcept;

protected:
    explicit Timeline(Meta::TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}

private:
    friend class Aero::Internal::AnimationPrivate;

    Base::String beginTimeText_;
    Base::String durationText_;
    Base::String repeatBehaviorText_;
    AnimationTime beginTimeMicroseconds_ = 0U;
    AnimationTime durationMicroseconds_ = 0U;
    double repeatCount_ = 1.0;
    double speedRatio_ = 1.0;
    bool repeatForever_ = false;
    bool autoReverse_ = false;
    FillBehavior fillBehavior_ = FillBehavior::HoldEnd;
};

class AERO_API EasingFunctionBase : public Base::Object {
    AERO_DECLARE_TYPE(EasingFunctionBase, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    EasingMode GetEasingMode() const noexcept {
        return easingMode_;
    }
    void SetEasingMode(EasingMode value) noexcept {
        easingMode_ = value;
        return;
    }

protected:
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

    EasingFunctionBase(
        Meta::TypeId runtimeType,
        Kind kind) noexcept
        : runtimeType_(runtimeType), kind_(kind) {}

    double PowerValue() const noexcept { return power_; }
    double AmplitudeValue() const noexcept { return amplitude_; }
    double OscillationsValue() const noexcept { return oscillations_; }
    double SpringinessValue() const noexcept { return springiness_; }
    void SetPowerValue(double value) noexcept { power_ = value; }
    void SetAmplitudeValue(double value) noexcept { amplitude_ = value; }
    void SetOscillationsValue(double value) noexcept { oscillations_ = value; }
    void SetSpringinessValue(double value) noexcept { springiness_ = value; }

private:
    friend class Aero::Internal::AnimationPrivate;

    Meta::TypeId runtimeType_ = StaticTypeId();
    Kind kind_ = Kind::Linear;
    EasingMode easingMode_ = EasingMode::EaseOut;
    double power_ = 2.0;
    double amplitude_ = 1.0;
    double oscillations_ = 3.0;
    double springiness_ = 3.0;
};

#define AERO_DECLARE_SIMPLE_EASING(typeName, kindValue)                    \
class AERO_API typeName final : public EasingFunctionBase {                \
    AERO_DECLARE_TYPE(typeName, EasingFunctionBase)                        \
public:                                                                    \
    typeName() noexcept                                                    \
        : EasingFunctionBase(StaticTypeId(), kindValue) {}                 \
};

AERO_DECLARE_SIMPLE_EASING(
    SineEase, EasingFunctionBase::Kind::Sine)
AERO_DECLARE_SIMPLE_EASING(
    QuadraticEase, EasingFunctionBase::Kind::Quadratic)
AERO_DECLARE_SIMPLE_EASING(
    CubicEase, EasingFunctionBase::Kind::Cubic)
AERO_DECLARE_SIMPLE_EASING(
    QuarticEase, EasingFunctionBase::Kind::Quartic)
AERO_DECLARE_SIMPLE_EASING(
    QuinticEase, EasingFunctionBase::Kind::Quintic)
AERO_DECLARE_SIMPLE_EASING(
    CircleEase, EasingFunctionBase::Kind::Circle)

#undef AERO_DECLARE_SIMPLE_EASING

class AERO_API ExponentialEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(ExponentialEase, EasingFunctionBase)
public:
    ExponentialEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              EasingFunctionBase::Kind::Exponential) {}
    double GetExponent() const noexcept {
        return PowerValue();
    }
    void SetExponent(double value) noexcept;
};

class AERO_API PowerEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(PowerEase, EasingFunctionBase)
public:
    PowerEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              EasingFunctionBase::Kind::Power) {}
    double GetPower() const noexcept { return PowerValue(); }
    void SetPower(double value) noexcept;
};

class AERO_API BackEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(BackEase, EasingFunctionBase)
public:
    BackEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              EasingFunctionBase::Kind::Back) {}
    double GetAmplitude() const noexcept {
        return AmplitudeValue();
    }
    void SetAmplitude(double value) noexcept;
};

class AERO_API BounceEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(BounceEase, EasingFunctionBase)
public:
    BounceEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              EasingFunctionBase::Kind::Bounce) {}
    double GetBounces() const noexcept {
        return OscillationsValue();
    }
    double GetBounciness() const noexcept {
        return SpringinessValue();
    }
    void SetBounces(double value) noexcept;
    void SetBounciness(double value) noexcept;
};

class AERO_API ElasticEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(ElasticEase, EasingFunctionBase)
public:
    ElasticEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              EasingFunctionBase::Kind::Elastic) {}
    double GetOscillations() const noexcept {
        return OscillationsValue();
    }
    double GetSpringiness() const noexcept {
        return SpringinessValue();
    }
    void SetOscillations(double value) noexcept;
    void SetSpringiness(double value) noexcept;
};

class AERO_API DoubleAnimation final : public Timeline {
    AERO_DECLARE_TYPE(DoubleAnimation, Timeline)
public:
    DoubleAnimation() noexcept : Timeline(StaticTypeId()) {}
    double GetFrom() const noexcept { return from_; }
    double GetTo() const noexcept { return to_; }
    double GetAccelerationRatio() const noexcept {
        return accelerationRatio_;
    }
    double GetDecelerationRatio() const noexcept {
        return decelerationRatio_;
    }
    Base::Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetFrom(double value) noexcept;
    void SetTo(double value) noexcept;
    void SetAccelerationRatio(double value) noexcept;
    void SetDecelerationRatio(double value) noexcept;
    void SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;

private:
    double from_ = 0.0;
    double to_ = 0.0;
    double accelerationRatio_ = 0.0;
    double decelerationRatio_ = 0.0;
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API ColorAnimation final : public Timeline {
    AERO_DECLARE_TYPE(ColorAnimation, Timeline)
public:
    ColorAnimation() noexcept : Timeline(StaticTypeId()) {}
    Base::Color GetFrom() const noexcept { return from_; }
    Base::Color GetTo() const noexcept { return to_; }
    Base::Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetFrom(Base::Color value) noexcept;
    void SetTo(Base::Color value) noexcept;
    void SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;

private:
    Base::Color from_;
    Base::Color to_;
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API PointAnimation final : public Timeline {
    AERO_DECLARE_TYPE(PointAnimation, Timeline)
public:
    PointAnimation() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Point GetFrom() const noexcept {
        return from_;
    }
    Base::Point GetTo() const noexcept {
        return to_;
    }
    Base::Ref<EasingFunctionBase>
    GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetFrom(
        Base::Point value) noexcept;
    void SetTo(
        Base::Point value) noexcept;
    void SetEasingFunction(
        Base::Ref<EasingFunctionBase>
            value) noexcept;

private:
    Base::Point from_;
    Base::Point to_;
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API RectAnimation final : public Timeline {
    AERO_DECLARE_TYPE(RectAnimation, Timeline)
public:
    RectAnimation() noexcept : Timeline(StaticTypeId()) {}
    Base::Rect GetFrom() const noexcept { return from_; }
    Base::Rect GetTo() const noexcept { return to_; }
    Base::Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetFrom(Base::Rect value) noexcept;
    void SetTo(Base::Rect value) noexcept;
    void SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;

private:
    Base::Rect from_;
    Base::Rect to_;
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API ThicknessAnimation final : public Timeline {
    AERO_DECLARE_TYPE(ThicknessAnimation, Timeline)
public:
    ThicknessAnimation() noexcept : Timeline(StaticTypeId()) {}
    Base::Thickness GetFrom() const noexcept { return from_; }
    Base::Thickness GetTo() const noexcept { return to_; }
    Base::Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetFrom(Base::Thickness value) noexcept;
    void SetTo(Base::Thickness value) noexcept;
    void SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;

private:
    Base::Thickness from_;
    Base::Thickness to_;
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API DoubleKeyFrame : public Base::Object {
    AERO_DECLARE_TYPE(DoubleKeyFrame, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    double GetValue() const noexcept { return value_; }
    Base::StringView GetKeyTime() const noexcept {
        return keyTimeText_.View();
    }
    void SetValue(double value) noexcept;
    void SetKeyTime(Base::StringView value) noexcept;

protected:
    enum class Interpolation : std::uint8_t {
        Linear = 0U,
        Discrete,
        Easing,
        Spline
    };

    DoubleKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : runtimeType_(runtimeType), interpolation_(interpolation) {}

    void SetSplineControlPoints(
        double x1,
        double y1,
        double x2,
        double y2) noexcept {
        controlPoint1X_ = x1;
        controlPoint1Y_ = y1;
        controlPoint2X_ = x2;
        controlPoint2Y_ = y2;
    }

private:
    friend class Aero::Internal::AnimationPrivate;

    Meta::TypeId runtimeType_ = StaticTypeId();
    double value_ = 0.0;
    Base::String keyTimeText_;
    AnimationTime keyTimeMicroseconds_ = 0U;
    Interpolation interpolation_ = Interpolation::Linear;
    double controlPoint1X_ = 0.0;
    double controlPoint1Y_ = 0.0;
    double controlPoint2X_ = 1.0;
    double controlPoint2Y_ = 1.0;
};

class AERO_API LinearDoubleKeyFrame final : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(LinearDoubleKeyFrame, DoubleKeyFrame)
public:
    LinearDoubleKeyFrame() noexcept
        : DoubleKeyFrame(
              StaticTypeId(),
              DoubleKeyFrame::Interpolation::Linear) {}
};

class AERO_API DiscreteDoubleKeyFrame final : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(DiscreteDoubleKeyFrame, DoubleKeyFrame)
public:
    DiscreteDoubleKeyFrame() noexcept
        : DoubleKeyFrame(
              StaticTypeId(),
              DoubleKeyFrame::Interpolation::Discrete) {}
};

class AERO_API EasingDoubleKeyFrame final : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(EasingDoubleKeyFrame, DoubleKeyFrame)
public:
    EasingDoubleKeyFrame() noexcept
        : DoubleKeyFrame(
              StaticTypeId(),
              DoubleKeyFrame::Interpolation::Easing) {}
    Base::Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;

private:
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API SplineDoubleKeyFrame final : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(SplineDoubleKeyFrame, DoubleKeyFrame)
public:
    SplineDoubleKeyFrame() noexcept
        : DoubleKeyFrame(
              StaticTypeId(),
              DoubleKeyFrame::Interpolation::Spline) {}
    Base::StringView GetKeySpline() const noexcept {
        return keySpline_.View();
    }
    void SetKeySpline(Base::StringView value) noexcept;

private:
    Base::String keySpline_;
};

class AERO_API DoubleAnimationUsingKeyFrames final : public Timeline {
    AERO_DECLARE_TYPE(DoubleAnimationUsingKeyFrames, Timeline)
public:
    DoubleAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> TryAddKeyFrame(
        Base::Ref<DoubleKeyFrame> value) noexcept;
    void ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<DoubleKeyFrame>>
    GetKeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<DoubleKeyFrame>> keyFrames_;
};

class AERO_API ThicknessKeyFrame : public Base::Object {
    AERO_DECLARE_TYPE(ThicknessKeyFrame, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Base::Thickness GetValue() const noexcept {
        return value_;
    }
    Base::StringView GetKeyTime() const noexcept {
        return keyTimeText_.View();
    }
    AnimationTime
    GetKeyTimeMicroseconds() const noexcept {
        return keyTimeMicroseconds_;
    }
    void SetValue(
        Base::Thickness value) noexcept;
    void SetKeyTime(
        Base::StringView value) noexcept;

protected:
    explicit ThicknessKeyFrame(
        Meta::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
    Base::Thickness value_;
    Base::String keyTimeText_;
    AnimationTime
        keyTimeMicroseconds_ = 0U;
};

class AERO_API LinearThicknessKeyFrame final
    : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(
        LinearThicknessKeyFrame,
        ThicknessKeyFrame)
public:
    LinearThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId()) {}
};

class AERO_API DiscreteThicknessKeyFrame final
    : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(
        DiscreteThicknessKeyFrame,
        ThicknessKeyFrame)
public:
    DiscreteThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId()) {}
};

class AERO_API EasingThicknessKeyFrame final
    : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(
        EasingThicknessKeyFrame,
        ThicknessKeyFrame)
public:
    EasingThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId()) {}
    Base::Ref<EasingFunctionBase>
    GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;

private:
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API SplineThicknessKeyFrame final
    : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(
        SplineThicknessKeyFrame,
        ThicknessKeyFrame)
public:
    SplineThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId()) {}
    Base::StringView GetKeySpline() const noexcept {
        return keySpline_.View();
    }
    void SetKeySpline(
        Base::StringView value) noexcept {
        (void)keySpline_.TryAssign(value);
    }

private:
    Base::String keySpline_;
};

class AERO_API ThicknessAnimationUsingKeyFrames final
    : public Timeline {
    AERO_DECLARE_TYPE(
        ThicknessAnimationUsingKeyFrames,
        Timeline)
public:
    ThicknessAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> TryAddKeyFrame(
        Base::Ref<ThicknessKeyFrame> value) noexcept;
    void ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<ThicknessKeyFrame>>
    GetKeyFrames() const noexcept {
        return {
            keyFrames_.Data(),
            keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<ThicknessKeyFrame>>
        keyFrames_;
};

class AERO_API ColorKeyFrame : public Base::Object {
    AERO_DECLARE_TYPE(ColorKeyFrame, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Base::Color GetValue() const noexcept { return value_; }
    Base::StringView GetKeyTime() const noexcept {
        return keyTimeText_.View();
    }
    void SetValue(Base::Color value) noexcept;
    void SetKeyTime(Base::StringView value) noexcept;

protected:
    enum class Interpolation : std::uint8_t {
        Linear = 0U,
        Discrete,
        Easing,
        Spline
    };

    ColorKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : runtimeType_(runtimeType), interpolation_(interpolation) {}

    void SetSplineControlPoints(
        double x1,
        double y1,
        double x2,
        double y2) noexcept {
        controlPoint1X_ = x1;
        controlPoint1Y_ = y1;
        controlPoint2X_ = x2;
        controlPoint2Y_ = y2;
    }

private:
    friend class Aero::Internal::AnimationPrivate;

    Meta::TypeId runtimeType_ = StaticTypeId();
    Base::Color value_;
    Base::String keyTimeText_;
    AnimationTime keyTimeMicroseconds_ = 0U;
    Interpolation interpolation_ = Interpolation::Linear;
    double controlPoint1X_ = 0.0;
    double controlPoint1Y_ = 0.0;
    double controlPoint2X_ = 1.0;
    double controlPoint2Y_ = 1.0;
};

class AERO_API LinearColorKeyFrame final : public ColorKeyFrame {
    AERO_DECLARE_TYPE(LinearColorKeyFrame, ColorKeyFrame)
public:
    LinearColorKeyFrame() noexcept
        : ColorKeyFrame(
              StaticTypeId(),
              ColorKeyFrame::Interpolation::Linear) {}
};

class AERO_API DiscreteColorKeyFrame final : public ColorKeyFrame {
    AERO_DECLARE_TYPE(DiscreteColorKeyFrame, ColorKeyFrame)
public:
    DiscreteColorKeyFrame() noexcept
        : ColorKeyFrame(
              StaticTypeId(),
              ColorKeyFrame::Interpolation::Discrete) {}
};

class AERO_API EasingColorKeyFrame final : public ColorKeyFrame {
    AERO_DECLARE_TYPE(EasingColorKeyFrame, ColorKeyFrame)
public:
    EasingColorKeyFrame() noexcept
        : ColorKeyFrame(
              StaticTypeId(),
              ColorKeyFrame::Interpolation::Easing) {}
    Base::Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;

private:
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API SplineColorKeyFrame final : public ColorKeyFrame {
    AERO_DECLARE_TYPE(SplineColorKeyFrame, ColorKeyFrame)
public:
    SplineColorKeyFrame() noexcept
        : ColorKeyFrame(
              StaticTypeId(),
              ColorKeyFrame::Interpolation::Spline) {}
    Base::StringView GetKeySpline() const noexcept {
        return keySpline_.View();
    }
    void SetKeySpline(Base::StringView value) noexcept;

private:
    Base::String keySpline_;
};

class AERO_API ColorAnimationUsingKeyFrames final : public Timeline {
    AERO_DECLARE_TYPE(ColorAnimationUsingKeyFrames, Timeline)
public:
    ColorAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> TryAddKeyFrame(
        Base::Ref<ColorKeyFrame> value) noexcept;
    void ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<ColorKeyFrame>>
    GetKeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<ColorKeyFrame>> keyFrames_;
};

class AERO_API DiscreteObjectKeyFrame final : public Base::Object {
    AERO_DECLARE_TYPE(DiscreteObjectKeyFrame, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    const Meta::PropertyValue& GetValue() const noexcept { return value_; }
    Base::StringView GetKeyTime() const noexcept {
        return keyTimeText_.View();
    }
    AnimationTime GetKeyTimeMicroseconds() const noexcept {
        return keyTimeMicroseconds_;
    }
    void SetValue(
        const Meta::PropertyValue& value) noexcept;
    void SetKeyTime(Base::StringView value) noexcept;

private:
    Meta::PropertyValue value_;
    Base::String keyTimeText_;
    AnimationTime keyTimeMicroseconds_ = 0U;
};

class AERO_API ObjectAnimationUsingKeyFrames final : public Timeline {
    AERO_DECLARE_TYPE(ObjectAnimationUsingKeyFrames, Timeline)
public:
    ObjectAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> TryAddKeyFrame(
        Base::Ref<DiscreteObjectKeyFrame> value) noexcept;
    void ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<DiscreteObjectKeyFrame>>
    GetKeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<DiscreteObjectKeyFrame>> keyFrames_;
};

class AERO_API DiscreteBooleanKeyFrame final : public Base::Object {
    AERO_DECLARE_TYPE(DiscreteBooleanKeyFrame, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    bool GetValue() const noexcept { return value_; }
    Base::StringView GetKeyTime() const noexcept {
        return keyTimeText_.View();
    }
    AnimationTime GetKeyTimeMicroseconds() const noexcept {
        return keyTimeMicroseconds_;
    }
    void SetValue(bool value) noexcept {
        value_ = value;
        return;
    }
    void SetKeyTime(Base::StringView value) noexcept;

private:
    bool value_ = false;
    Base::String keyTimeText_;
    AnimationTime keyTimeMicroseconds_ = 0U;
};

class AERO_API BooleanAnimationUsingKeyFrames final : public Timeline {
    AERO_DECLARE_TYPE(BooleanAnimationUsingKeyFrames, Timeline)
public:
    BooleanAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> TryAddKeyFrame(
        Base::Ref<DiscreteBooleanKeyFrame> value) noexcept;
    void ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<DiscreteBooleanKeyFrame>>
    GetKeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<DiscreteBooleanKeyFrame>> keyFrames_;
};

class AERO_API Storyboard final : public Timeline {
    AERO_DECLARE_TYPE(Storyboard, Timeline)
public:
    Storyboard() noexcept : Timeline(StaticTypeId()) {}

    inline static constexpr Members::AttachedProperty<Base::String> TargetNameProperty{"TargetName"};
    inline static constexpr Members::AttachedProperty<Base::String> TargetPropertyProperty{"TargetProperty"};

    Base::Result<void> TryAddTimeline(
        Base::Ref<Timeline> value) noexcept;
    void ClearTimelines() noexcept;
    Base::Span<const Base::Ref<Timeline>>
    GetTimelines() const noexcept {
        return {timelines_.Data(), timelines_.Size()};
    }

private:
    Base::Vector<Base::Ref<Timeline>> timelines_;
};

class AERO_API TriggerAction : public Base::Object {
    AERO_DECLARE_TYPE(TriggerAction, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }

protected:
    explicit TriggerAction(
        Meta::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
};

class AERO_API ChangePropertyAction final : public TriggerAction {
    AERO_DECLARE_TYPE(ChangePropertyAction, TriggerAction)
public:
    ChangePropertyAction() noexcept
        : TriggerAction(StaticTypeId()) {}

    Base::StringView GetTargetName() const noexcept {
        return targetName_.View();
    }
    Base::StringView GetPropertyName() const noexcept {
        return propertyName_.View();
    }
    const Meta::PropertyValue& GetValue() const noexcept {
        return value_;
    }
    void SetTargetName(
        Base::StringView value) noexcept;
    void SetPropertyName(
        Base::StringView value) noexcept;
    void SetValue(
        const Meta::PropertyValue& value) noexcept;

private:
    Base::String targetName_;
    Base::String propertyName_;
    Meta::PropertyValue value_;
};

// Gallery interactivity action. It deliberately uses the trigger owner when
// no external target is supplied, matching the WPF attached-action form.
class AERO_API SetFocusAction final : public TriggerAction {
    AERO_DECLARE_TYPE(SetFocusAction, TriggerAction)
public:
    SetFocusAction() noexcept
        : TriggerAction(StaticTypeId()) {}

    bool GetEngage() const noexcept { return engage_; }
    void SetEngage(bool value) noexcept {
        engage_ = value;
        return;
    }

private:
    bool engage_ = true;
};

// Interactivity's link action is represented in markup as a regular trigger
// action so it participates in the same event pipeline as the other Gallery
// actions. The host decides how to open the resolved URI or file.
class AERO_API LaunchUriOrFileAction final : public TriggerAction {
    AERO_DECLARE_TYPE(LaunchUriOrFileAction, TriggerAction)
public:
    LaunchUriOrFileAction() noexcept : TriggerAction(StaticTypeId()) {}
    Base::StringView GetPath() const noexcept { return path_.View(); }
    void SetPath(Base::StringView value) noexcept;
    Base::Ref<Aero::Data::Binding> GetPathBinding() const noexcept {
        return pathBinding_;
    }
    void SetPathBinding(
        Base::Ref<Aero::Data::Binding> value) noexcept {
        pathBinding_ = std::move(value);
        return;
    }
private:
    Base::String path_;
    Base::Ref<Aero::Data::Binding> pathBinding_;
};

class AERO_API RemoveElementAction final : public TriggerAction {
    AERO_DECLARE_TYPE(RemoveElementAction, TriggerAction)
public:
    RemoveElementAction() noexcept
        : TriggerAction(StaticTypeId()) {}

    Base::Ref<Aero::Data::Binding> GetTargetObject() const noexcept {
        return targetObject_;
    }
    void SetTargetObject(
        Base::Ref<Aero::Data::Binding> value) noexcept {
        targetObject_ = std::move(value);
        return;
    }

private:
    Base::Ref<Aero::Data::Binding> targetObject_;
};

class AERO_API BeginStoryboard final : public TriggerAction {
    AERO_DECLARE_TYPE(BeginStoryboard, TriggerAction)
public:
    BeginStoryboard() noexcept
        : TriggerAction(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView GetName() const noexcept {
        return name_.View();
    }
    Base::Ref<Storyboard> GetStoryboard() const noexcept {
        return storyboard_;
    }
    void SetName(
        Base::StringView value) noexcept;
    void SetStoryboard(
        Base::Ref<Storyboard> value) noexcept;

private:
    Base::String name_;
    Base::Ref<Storyboard> storyboard_;
};

class AERO_API ControlStoryboardAction final : public TriggerAction {
    AERO_DECLARE_TYPE(ControlStoryboardAction, TriggerAction)
public:
    enum class Option : std::uint8_t { Play = 0U, Stop, TogglePlayPause, Pause, Resume, SkipToFill };
    ControlStoryboardAction() noexcept : TriggerAction(StaticTypeId()) {}
    Base::Ref<Storyboard> GetStoryboard() const noexcept { return storyboard_; }
    void SetStoryboard(Base::Ref<Storyboard> value) noexcept { storyboard_ = std::move(value); return; }
    Option GetControlOption() const noexcept { return option_; }
    void SetControlOption(Option value) noexcept { option_ = value; return; }
private:
    Base::Ref<Storyboard> storyboard_;
    Option option_ = Option::Play;
};

class AERO_API ControllableStoryboardAction :
    public TriggerAction {
    AERO_DECLARE_TYPE(
        ControllableStoryboardAction,
        TriggerAction)
public:
    Base::StringView GetBeginStoryboardName() const noexcept {
        return beginStoryboardName_.View();
    }
    void SetBeginStoryboardName(
        Base::StringView value) noexcept;

protected:
    explicit ControllableStoryboardAction(
        Meta::TypeId runtimeType) noexcept
        : TriggerAction(runtimeType) {}

private:
    Base::String beginStoryboardName_;
};

#define AERO_DECLARE_STORYBOARD_ACTION(typeName)                  \
class AERO_API typeName final :                                   \
    public ControllableStoryboardAction {                         \
    AERO_DECLARE_TYPE(typeName, ControllableStoryboardAction)     \
public:                                                           \
    typeName() noexcept                                           \
        : ControllableStoryboardAction(StaticTypeId()) {}         \
};

AERO_DECLARE_STORYBOARD_ACTION(PauseStoryboard)
AERO_DECLARE_STORYBOARD_ACTION(ResumeStoryboard)
AERO_DECLARE_STORYBOARD_ACTION(StopStoryboard)
AERO_DECLARE_STORYBOARD_ACTION(RemoveStoryboard)

#undef AERO_DECLARE_STORYBOARD_ACTION

class AERO_API SeekStoryboard final :
    public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(
        SeekStoryboard,
        ControllableStoryboardAction)
public:
    SeekStoryboard() noexcept
        : ControllableStoryboardAction(
              StaticTypeId()) {}
    Base::StringView GetOffset() const noexcept {
        return offsetText_.View();
    }
    AnimationTime
    GetOffsetMicroseconds() const noexcept {
        return offsetMicroseconds_;
    }
    void SetOffset(
        Base::StringView value) noexcept;

private:
    Base::String offsetText_;
    AnimationTime
        offsetMicroseconds_ = 0U;
};

class AERO_API EventTrigger : public Base::Object {
    AERO_DECLARE_TYPE(EventTrigger, Base::Object)
public:
    EventTrigger() noexcept : EventTrigger(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Base::StringView GetRoutedEvent() const noexcept {
        return routedEvent_.View();
    }
    Base::StringView GetEventName() const noexcept {
        return routedEvent_.View();
    }
    Base::StringView GetSourceName() const noexcept {
        return sourceName_.View();
    }
    void SetRoutedEvent(
        Base::StringView value) noexcept;
    void SetEventName(
        Base::StringView value) noexcept {
        SetRoutedEvent(value);
    }
    void SetSourceName(
        Base::StringView value) noexcept;
    Base::Result<void> TryAddAction(
        Base::Ref<TriggerAction> value) noexcept;
    void ClearActions() noexcept;
    Base::Span<const Base::Ref<TriggerAction>>
    GetActions() const noexcept {
        return {actions_.Data(), actions_.Size()};
    }
    Base::Result<void> TryAddConditionBehavior(
        Base::Ref<Base::Object> value) noexcept {
        return behaviors_.TryPushBack(std::move(value));
    }
    void ClearConditionBehaviors() noexcept { behaviors_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> GetBehaviors() const noexcept {
        return {behaviors_.Data(), behaviors_.Size()};
    }

protected:
    explicit EventTrigger(Meta::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
    Base::String routedEvent_;
    Base::String sourceName_;
    Base::Vector<Base::Ref<TriggerAction>> actions_;
    Base::Vector<Base::Ref<Base::Object>> behaviors_;
};

// Compatibility trigger used by the original Gallery's behavior XAML. The
// runtime treats it as an EventTrigger with a repeatable timer cadence.
class AERO_API TimerTrigger final : public EventTrigger {
    AERO_DECLARE_TYPE(TimerTrigger, EventTrigger)
public:
    TimerTrigger() noexcept : EventTrigger(StaticTypeId()) {}

    std::uint32_t GetTotalTicks() const noexcept { return totalTicks_; }
    const Meta::PropertyValue& GetMillisecondsPerTick() const noexcept {
        return millisecondsPerTick_;
    }
    void SetTotalTicks(std::uint32_t value) noexcept {
        totalTicks_ = value;
        return;
    }
    void SetMillisecondsPerTick(
        const Meta::PropertyValue& value) noexcept {
        millisecondsPerTick_ = value;
        return;
    }

private:
    std::uint32_t totalTicks_ = 1U;
    Meta::PropertyValue millisecondsPerTick_;
};

class AERO_API ComparisonCondition final : public Base::Object {
    AERO_DECLARE_TYPE(ComparisonCondition, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Ref<Aero::Data::Binding> GetLeftOperand() const noexcept { return left_; }
    void SetLeftOperand(Base::Ref<Aero::Data::Binding> value) noexcept {
        left_ = std::move(value); return;
    }
    const Meta::PropertyValue& GetRightOperand() const noexcept { return right_; }
    void SetRightOperand(const Meta::PropertyValue& value) noexcept {
        right_ = value; return;
    }
    enum class Operator : std::uint8_t {
        Equal = 0U,
        NotEqual,
        LessThan,
        LessThanOrEqual,
        GreaterThan,
        GreaterThanOrEqual,
    };
    Operator GetComparisonOperator() const noexcept { return operator_; }
    void SetComparisonOperator(Operator value) noexcept {
        operator_ = value; return;
    }
private:
    Base::Ref<Aero::Data::Binding> left_;
    Meta::PropertyValue right_;
    Operator operator_ = Operator::Equal;
};

class AERO_API ConditionalExpression final : public Base::Object {
    AERO_DECLARE_TYPE(ConditionalExpression, Base::Object)
public:
    enum class ForwardChaining : std::uint8_t {
        And = 0U,
        Or,
    };
    ConditionalExpression() noexcept : conditions_(&Base::GetDefaultAllocator()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Result<void> TryAddCondition(Base::Ref<ComparisonCondition> value) noexcept {
        return value ? conditions_.TryPushBack(std::move(value))
            : Base::Result<void>(Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Condition is null"));
    }
    void ClearConditions() noexcept { conditions_.Clear(); }
    Base::Span<const Base::Ref<ComparisonCondition>> GetConditions() const noexcept {
        return {conditions_.Data(), conditions_.Size()};
    }
    ForwardChaining GetChaining() const noexcept { return chaining_; }
    void SetChaining(ForwardChaining value) noexcept {
        chaining_ = value; return;
    }
private:
    Base::Vector<Base::Ref<ComparisonCondition>> conditions_;
    ForwardChaining chaining_ = ForwardChaining::And;
};

class AERO_API ConditionBehavior final : public Base::Object {
    AERO_DECLARE_TYPE(ConditionBehavior, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Ref<ConditionalExpression> GetExpression() const noexcept { return expression_; }
    void SetExpression(Base::Ref<ConditionalExpression> value) noexcept {
        expression_ = std::move(value); return;
    }
private:
    Base::Ref<ConditionalExpression> expression_;
};

class AERO_API StoryboardCompletedTrigger final :
    public Base::Object {
    AERO_DECLARE_TYPE(StoryboardCompletedTrigger, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Ref<Storyboard> GetStoryboard() const noexcept {
        return storyboard_;
    }
    void SetStoryboard(
        Base::Ref<Storyboard> value) noexcept;
    Base::Result<void> TryAddAction(
        Base::Ref<TriggerAction> value) noexcept;
    void ClearActions() noexcept;
    Base::Span<const Base::Ref<TriggerAction>>
    GetActions() const noexcept {
        return {actions_.Data(), actions_.Size()};
    }

private:
    Base::Ref<Storyboard> storyboard_;
    Base::Vector<Base::Ref<TriggerAction>> actions_;
};

class AERO_API Interaction final : public Base::Object {
    AERO_DECLARE_TYPE(Interaction, Base::Object)
private:
    Interaction() noexcept = default;
};

} // namespace Aero::Media::Animation

namespace Aero::Meta {

template<>
struct TypeTraits<Media::Animation::FillBehavior> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("FillBehavior"); }
    static constexpr Base::StringView Namespace() noexcept { return AeroNamespaceUri(); }
    static constexpr Base::StringView Name() noexcept { return "FillBehavior"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct TypeTraits<Media::Animation::EasingMode> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("EasingMode"); }
    static constexpr Base::StringView Namespace() noexcept { return AeroNamespaceUri(); }
    static constexpr Base::StringView Name() noexcept { return "EasingMode"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct TypeTraits<Media::Animation::ComparisonCondition::Operator> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ComparisonConditionOperator");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ComparisonConditionOperator";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct TypeTraits<Media::Animation::ConditionalExpression::ForwardChaining> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ForwardChaining");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ForwardChaining";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct TypeTraits<Media::Animation::ControlStoryboardAction::Option> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("ControlStoryboardOption"); }
    static constexpr Base::StringView Namespace() noexcept { return AeroNamespaceUri(); }
    static constexpr Base::StringView Name() noexcept { return "ControlStoryboardOption"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Meta
