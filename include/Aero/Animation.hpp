#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Data.hpp>
#include <cstdint>

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

class AERO_API Timeline : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Timeline, ::Aero::Freezable)
public:
    struct Impl;

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
        : Freezable(runtimeType) {}

private:
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
    friend struct ::Aero::Media::Animation::Timeline::Impl;

    Meta::TypeId runtimeType_ = StaticTypeId();
    Kind kind_ = Kind::Linear;
    EasingMode easingMode_ = EasingMode::EaseOut;
    double power_ = 2.0;
    double amplitude_ = 1.0;
    double oscillations_ = 3.0;
    double springiness_ = 3.0;
};

#define AERO_DECLARE_SIMPLE_EASING(typeName, kindValue)                    \
class AERO_API typeName : public EasingFunctionBase {                \
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

class AERO_API ExponentialEase : public EasingFunctionBase {
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

class AERO_API PowerEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(PowerEase, EasingFunctionBase)
public:
    PowerEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              EasingFunctionBase::Kind::Power) {}
    double GetPower() const noexcept { return PowerValue(); }
    void SetPower(double value) noexcept;
};

class AERO_API BackEase : public EasingFunctionBase {
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

class AERO_API BounceEase : public EasingFunctionBase {
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

class AERO_API ElasticEase : public EasingFunctionBase {
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

class AERO_API DoubleAnimation : public Timeline {
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

class AERO_API ColorAnimation : public Timeline {
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

class AERO_API PointAnimation : public Timeline {
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

class AERO_API RectAnimation : public Timeline {
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

class AERO_API ThicknessAnimation : public Timeline {
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
    friend struct ::Aero::Media::Animation::Timeline::Impl;

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

class AERO_API LinearDoubleKeyFrame : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(LinearDoubleKeyFrame, DoubleKeyFrame)
public:
    LinearDoubleKeyFrame() noexcept
        : DoubleKeyFrame(
              StaticTypeId(),
              DoubleKeyFrame::Interpolation::Linear) {}
};

class AERO_API DiscreteDoubleKeyFrame : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(DiscreteDoubleKeyFrame, DoubleKeyFrame)
public:
    DiscreteDoubleKeyFrame() noexcept
        : DoubleKeyFrame(
              StaticTypeId(),
              DoubleKeyFrame::Interpolation::Discrete) {}
};

class AERO_API EasingDoubleKeyFrame : public DoubleKeyFrame {
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

class AERO_API SplineDoubleKeyFrame : public DoubleKeyFrame {
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

class AERO_API DoubleAnimationUsingKeyFrames : public Timeline {
    AERO_DECLARE_TYPE(DoubleAnimationUsingKeyFrames, Timeline)
public:
    DoubleAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> AddKeyFrame(
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

class AERO_API LinearThicknessKeyFrame
    : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(
        LinearThicknessKeyFrame,
        ThicknessKeyFrame)
public:
    LinearThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId()) {}
};

class AERO_API DiscreteThicknessKeyFrame
    : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(
        DiscreteThicknessKeyFrame,
        ThicknessKeyFrame)
public:
    DiscreteThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId()) {}
};

class AERO_API EasingThicknessKeyFrame
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

class AERO_API SplineThicknessKeyFrame
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
        (void)keySpline_.Assign(value);
    }

private:
    Base::String keySpline_;
};

class AERO_API ThicknessAnimationUsingKeyFrames
    : public Timeline {
    AERO_DECLARE_TYPE(
        ThicknessAnimationUsingKeyFrames,
        Timeline)
public:
    ThicknessAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> AddKeyFrame(
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
    friend struct ::Aero::Media::Animation::Timeline::Impl;

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

class AERO_API LinearColorKeyFrame : public ColorKeyFrame {
    AERO_DECLARE_TYPE(LinearColorKeyFrame, ColorKeyFrame)
public:
    LinearColorKeyFrame() noexcept
        : ColorKeyFrame(
              StaticTypeId(),
              ColorKeyFrame::Interpolation::Linear) {}
};

class AERO_API DiscreteColorKeyFrame : public ColorKeyFrame {
    AERO_DECLARE_TYPE(DiscreteColorKeyFrame, ColorKeyFrame)
public:
    DiscreteColorKeyFrame() noexcept
        : ColorKeyFrame(
              StaticTypeId(),
              ColorKeyFrame::Interpolation::Discrete) {}
};

class AERO_API EasingColorKeyFrame : public ColorKeyFrame {
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

class AERO_API SplineColorKeyFrame : public ColorKeyFrame {
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

class AERO_API ColorAnimationUsingKeyFrames : public Timeline {
    AERO_DECLARE_TYPE(ColorAnimationUsingKeyFrames, Timeline)
public:
    ColorAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> AddKeyFrame(
        Base::Ref<ColorKeyFrame> value) noexcept;
    void ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<ColorKeyFrame>>
    GetKeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<ColorKeyFrame>> keyFrames_;
};

class AERO_API DiscreteObjectKeyFrame : public Base::Object {
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

class AERO_API ObjectAnimationUsingKeyFrames : public Timeline {
    AERO_DECLARE_TYPE(ObjectAnimationUsingKeyFrames, Timeline)
public:
    ObjectAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> AddKeyFrame(
        Base::Ref<DiscreteObjectKeyFrame> value) noexcept;
    void ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<DiscreteObjectKeyFrame>>
    GetKeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<DiscreteObjectKeyFrame>> keyFrames_;
};

class AERO_API DiscreteBooleanKeyFrame : public Base::Object {
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

class AERO_API BooleanAnimationUsingKeyFrames : public Timeline {
    AERO_DECLARE_TYPE(BooleanAnimationUsingKeyFrames, Timeline)
public:
    BooleanAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> AddKeyFrame(
        Base::Ref<DiscreteBooleanKeyFrame> value) noexcept;
    void ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<DiscreteBooleanKeyFrame>>
    GetKeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<DiscreteBooleanKeyFrame>> keyFrames_;
};

class AERO_API Storyboard : public Timeline {
    AERO_DECLARE_TYPE(Storyboard, Timeline)
public:
    Storyboard() noexcept : Timeline(StaticTypeId()) {}
    ~Storyboard() override;

    inline static constexpr Members::AttachedProperty<Base::String> TargetNameProperty{"TargetName"};
    inline static constexpr Members::AttachedProperty<Base::String> TargetPropertyProperty{"TargetProperty"};

    Base::Result<void> AddTimeline(
        Base::Ref<Timeline> value) noexcept;
    void ClearTimelines() noexcept;
    Base::Span<const Base::Ref<Timeline>>
    GetTimelines() const noexcept {
        return {timelines_.Data(), timelines_.Size()};
    }

protected:
    bool FreezeCore(bool isChecking) noexcept override;

private:
    void OnTimelineChanged(Freezable&) noexcept;
    Base::Vector<Base::Ref<Timeline>> timelines_;
    FreezableChangedHandler timelineChangedHandler_;
};



} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_ENUM(Aero::Media::Animation::FillBehavior)

AERO_DECLARE_TYPE_ENUM(Aero::Media::Animation::EasingMode)

// Trigger actions are exposed through concept-specific headers. They include
// this animation core and therefore do not introduce a second declaration
// owner or a dependency cycle.
#include <Aero/Triggers/TriggerAction.hpp>
