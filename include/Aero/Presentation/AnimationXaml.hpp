#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Presentation/Animation.hpp>
#include <Aero/Presentation/Binding.hpp>

namespace Aero::Animation {

class AERO_API Timeline : public Core::DependencyObject {
    AERO_DECLARE_TYPE(Timeline, Core::DependencyObject)
public:
    Base::StringView BeginTime() const noexcept {
        return beginTimeText_.View();
    }
    Base::StringView Duration() const noexcept {
        return durationText_.View();
    }
    Base::StringView RepeatBehavior() const noexcept {
        return repeatBehaviorText_.View();
    }
    double SpeedRatio() const noexcept { return timing_.speedRatio; }
    bool AutoReverse() const noexcept { return timing_.autoReverse; }
    Presentation::FillBehavior GetFillBehavior() const noexcept {
        return timing_.fillBehavior;
    }
    const Presentation::TimelineTiming& Timing() const noexcept {
        return timing_;
    }

    Base::Result<void> SetBeginTime(Base::StringView value) noexcept;
    Base::Result<void> SetDuration(Base::StringView value) noexcept;
    Base::Result<void> SetRepeatBehavior(
        Base::StringView value) noexcept;
    Base::Result<void> SetSpeedRatio(double value) noexcept;
    Base::Result<void> SetAutoReverse(bool value) noexcept;
    Base::Result<void> SetFillBehavior(
        Presentation::FillBehavior value) noexcept;

protected:
    explicit Timeline(Core::TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}

private:
    Base::String beginTimeText_;
    Base::String durationText_;
    Base::String repeatBehaviorText_;
    Presentation::TimelineTiming timing_;
};

class AERO_API EasingFunctionBase : public Base::Object {
    AERO_DECLARE_TYPE(EasingFunctionBase, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Presentation::EasingMode EasingMode() const noexcept {
        return easing_.mode;
    }
    Base::Result<void> SetEasingMode(
        Presentation::EasingMode value) noexcept {
        easing_.mode = value;
        return {};
    }
    const Presentation::EasingFunction& RuntimeEasing() const noexcept {
        return easing_;
    }

protected:
    EasingFunctionBase(
        Core::TypeId runtimeType,
        Presentation::EasingFunctionKind kind) noexcept
        : runtimeType_(runtimeType) {
        easing_.kind = kind;
    }
    Presentation::EasingFunction& MutableEasing() noexcept {
        return easing_;
    }

private:
    Core::TypeId runtimeType_ = StaticTypeId();
    Presentation::EasingFunction easing_;
};

#define AERO_DECLARE_SIMPLE_EASING(typeName, kindValue)                    \
class AERO_API typeName final : public EasingFunctionBase {                \
    AERO_DECLARE_TYPE(typeName, EasingFunctionBase)                        \
public:                                                                    \
    typeName() noexcept                                                    \
        : EasingFunctionBase(StaticTypeId(), kindValue) {}                 \
};

AERO_DECLARE_SIMPLE_EASING(
    SineEase, Presentation::EasingFunctionKind::Sine)
AERO_DECLARE_SIMPLE_EASING(
    QuadraticEase, Presentation::EasingFunctionKind::Quadratic)
AERO_DECLARE_SIMPLE_EASING(
    CubicEase, Presentation::EasingFunctionKind::Cubic)
AERO_DECLARE_SIMPLE_EASING(
    QuarticEase, Presentation::EasingFunctionKind::Quartic)
AERO_DECLARE_SIMPLE_EASING(
    QuinticEase, Presentation::EasingFunctionKind::Quintic)
AERO_DECLARE_SIMPLE_EASING(
    CircleEase, Presentation::EasingFunctionKind::Circle)

#undef AERO_DECLARE_SIMPLE_EASING

class AERO_API ExponentialEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(ExponentialEase, EasingFunctionBase)
public:
    ExponentialEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              Presentation::EasingFunctionKind::Exponential) {}
    double Exponent() const noexcept {
        return RuntimeEasing().power;
    }
    Base::Result<void> SetExponent(double value) noexcept;
};

class AERO_API PowerEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(PowerEase, EasingFunctionBase)
public:
    PowerEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              Presentation::EasingFunctionKind::Power) {}
    double Power() const noexcept { return RuntimeEasing().power; }
    Base::Result<void> SetPower(double value) noexcept;
};

class AERO_API BackEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(BackEase, EasingFunctionBase)
public:
    BackEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              Presentation::EasingFunctionKind::Back) {}
    double Amplitude() const noexcept {
        return RuntimeEasing().amplitude;
    }
    Base::Result<void> SetAmplitude(double value) noexcept;
};

class AERO_API BounceEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(BounceEase, EasingFunctionBase)
public:
    BounceEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              Presentation::EasingFunctionKind::Bounce) {}
    double Bounces() const noexcept {
        return RuntimeEasing().oscillations;
    }
    double Bounciness() const noexcept {
        return RuntimeEasing().springiness;
    }
    Base::Result<void> SetBounces(double value) noexcept;
    Base::Result<void> SetBounciness(double value) noexcept;
};

class AERO_API ElasticEase final : public EasingFunctionBase {
    AERO_DECLARE_TYPE(ElasticEase, EasingFunctionBase)
public:
    ElasticEase() noexcept
        : EasingFunctionBase(
              StaticTypeId(),
              Presentation::EasingFunctionKind::Elastic) {}
    double Oscillations() const noexcept {
        return RuntimeEasing().oscillations;
    }
    double Springiness() const noexcept {
        return RuntimeEasing().springiness;
    }
    Base::Result<void> SetOscillations(double value) noexcept;
    Base::Result<void> SetSpringiness(double value) noexcept;
};

class AERO_API DoubleAnimation final : public Timeline {
    AERO_DECLARE_TYPE(DoubleAnimation, Timeline)
public:
    DoubleAnimation() noexcept : Timeline(StaticTypeId()) {}
    double From() const noexcept { return from_; }
    double To() const noexcept { return to_; }
    double AccelerationRatio() const noexcept {
        return accelerationRatio_;
    }
    double DecelerationRatio() const noexcept {
        return decelerationRatio_;
    }
    Base::Ref<EasingFunctionBase> EasingFunction() const noexcept {
        return easing_;
    }
    Base::Result<void> SetFrom(double value) noexcept;
    Base::Result<void> SetTo(double value) noexcept;
    Base::Result<void> SetAccelerationRatio(double value) noexcept;
    Base::Result<void> SetDecelerationRatio(double value) noexcept;
    Base::Result<void> SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;
    Presentation::DoubleAnimation RuntimeAnimation() const noexcept;

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
    Base::Color From() const noexcept { return from_; }
    Base::Color To() const noexcept { return to_; }
    Base::Ref<EasingFunctionBase> EasingFunction() const noexcept {
        return easing_;
    }
    Base::Result<void> SetFrom(Base::Color value) noexcept;
    Base::Result<void> SetTo(Base::Color value) noexcept;
    Base::Result<void> SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;
    Presentation::ColorAnimation RuntimeAnimation() const noexcept;

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
    Base::Point From() const noexcept {
        return from_;
    }
    Base::Point To() const noexcept {
        return to_;
    }
    Base::Ref<EasingFunctionBase>
    EasingFunction() const noexcept {
        return easing_;
    }
    Base::Result<void> SetFrom(
        Base::Point value) noexcept;
    Base::Result<void> SetTo(
        Base::Point value) noexcept;
    Base::Result<void> SetEasingFunction(
        Base::Ref<EasingFunctionBase>
            value) noexcept;
    Presentation::PointAnimation
        RuntimeAnimation() const noexcept;

private:
    Base::Point from_;
    Base::Point to_;
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API RectAnimation final : public Timeline {
    AERO_DECLARE_TYPE(RectAnimation, Timeline)
public:
    RectAnimation() noexcept : Timeline(StaticTypeId()) {}
    Base::Rect From() const noexcept { return from_; }
    Base::Rect To() const noexcept { return to_; }
    Base::Ref<EasingFunctionBase> EasingFunction() const noexcept {
        return easing_;
    }
    Base::Result<void> SetFrom(Base::Rect value) noexcept;
    Base::Result<void> SetTo(Base::Rect value) noexcept;
    Base::Result<void> SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;
    Presentation::RectAnimation RuntimeAnimation() const noexcept;

private:
    Base::Rect from_;
    Base::Rect to_;
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API ThicknessAnimation final : public Timeline {
    AERO_DECLARE_TYPE(ThicknessAnimation, Timeline)
public:
    ThicknessAnimation() noexcept : Timeline(StaticTypeId()) {}
    Base::Thickness From() const noexcept { return from_; }
    Base::Thickness To() const noexcept { return to_; }
    Base::Ref<EasingFunctionBase> EasingFunction() const noexcept {
        return easing_;
    }
    Base::Result<void> SetFrom(Base::Thickness value) noexcept;
    Base::Result<void> SetTo(Base::Thickness value) noexcept;
    Base::Result<void> SetEasingFunction(
        Base::Ref<EasingFunctionBase> value) noexcept;
    Presentation::ThicknessAnimation RuntimeAnimation() const noexcept;

private:
    Base::Thickness from_;
    Base::Thickness to_;
    Base::Ref<EasingFunctionBase> easing_;
};

class AERO_API DoubleKeyFrame : public Base::Object {
    AERO_DECLARE_TYPE(DoubleKeyFrame, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    double Value() const noexcept { return value_; }
    Base::StringView KeyTime() const noexcept {
        return keyTimeText_.View();
    }
    Base::Result<void> SetValue(double value) noexcept;
    Base::Result<void> SetKeyTime(Base::StringView value) noexcept;
    const Presentation::DoubleKeyFrame& RuntimeFrame() const noexcept {
        return frame_;
    }

protected:
    DoubleKeyFrame(
        Core::TypeId runtimeType,
        Presentation::DoubleKeyFrameInterpolation interpolation) noexcept
        : runtimeType_(runtimeType) {
        frame_.interpolation = interpolation;
    }
    Presentation::DoubleKeyFrame& MutableFrame() noexcept {
        return frame_;
    }

private:
    Core::TypeId runtimeType_ = StaticTypeId();
    double value_ = 0.0;
    Base::String keyTimeText_;
    Presentation::DoubleKeyFrame frame_;
};

class AERO_API LinearDoubleKeyFrame final : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(LinearDoubleKeyFrame, DoubleKeyFrame)
public:
    LinearDoubleKeyFrame() noexcept
        : DoubleKeyFrame(
              StaticTypeId(),
              Presentation::DoubleKeyFrameInterpolation::Linear) {}
};

class AERO_API DiscreteDoubleKeyFrame final : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(DiscreteDoubleKeyFrame, DoubleKeyFrame)
public:
    DiscreteDoubleKeyFrame() noexcept
        : DoubleKeyFrame(
              StaticTypeId(),
              Presentation::DoubleKeyFrameInterpolation::Discrete) {}
};

class AERO_API EasingDoubleKeyFrame final : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(EasingDoubleKeyFrame, DoubleKeyFrame)
public:
    EasingDoubleKeyFrame() noexcept
        : DoubleKeyFrame(
              StaticTypeId(),
              Presentation::DoubleKeyFrameInterpolation::Easing) {}
    Base::Ref<EasingFunctionBase> EasingFunction() const noexcept {
        return easing_;
    }
    Base::Result<void> SetEasingFunction(
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
              Presentation::DoubleKeyFrameInterpolation::Spline) {}
    Base::StringView KeySpline() const noexcept {
        return keySpline_.View();
    }
    Base::Result<void> SetKeySpline(Base::StringView value) noexcept;

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
    Base::Result<void> ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<DoubleKeyFrame>>
    KeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<DoubleKeyFrame>> keyFrames_;
};

class AERO_API ThicknessKeyFrame : public Base::Object {
    AERO_DECLARE_TYPE(ThicknessKeyFrame, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Base::Thickness Value() const noexcept {
        return value_;
    }
    Base::StringView KeyTime() const noexcept {
        return keyTimeText_.View();
    }
    Presentation::AnimationTime
    KeyTimeMicroseconds() const noexcept {
        return keyTimeMicroseconds_;
    }
    Base::Result<void> SetValue(
        Base::Thickness value) noexcept;
    Base::Result<void> SetKeyTime(
        Base::StringView value) noexcept;

protected:
    explicit ThicknessKeyFrame(
        Core::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

private:
    Core::TypeId runtimeType_ = StaticTypeId();
    Base::Thickness value_;
    Base::String keyTimeText_;
    Presentation::AnimationTime
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
    EasingFunction() const noexcept {
        return easing_;
    }
    Base::Result<void> SetEasingFunction(
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
    Base::StringView KeySpline() const noexcept {
        return keySpline_.View();
    }
    Base::Result<void> SetKeySpline(
        Base::StringView value) noexcept {
        return keySpline_.TryAssign(value);
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
    Base::Result<void> ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<ThicknessKeyFrame>>
    KeyFrames() const noexcept {
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
    Core::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Base::Color Value() const noexcept { return value_; }
    Base::StringView KeyTime() const noexcept {
        return keyTimeText_.View();
    }
    Base::Result<void> SetValue(Base::Color value) noexcept;
    Base::Result<void> SetKeyTime(Base::StringView value) noexcept;
    const Presentation::ColorKeyFrame& RuntimeFrame() const noexcept {
        return frame_;
    }

protected:
    ColorKeyFrame(
        Core::TypeId runtimeType,
        Presentation::DoubleKeyFrameInterpolation interpolation) noexcept
        : runtimeType_(runtimeType) {
        frame_.interpolation = interpolation;
    }
    Presentation::ColorKeyFrame& MutableFrame() noexcept {
        return frame_;
    }

private:
    Core::TypeId runtimeType_ = StaticTypeId();
    Base::Color value_;
    Base::String keyTimeText_;
    Presentation::ColorKeyFrame frame_;
};

class AERO_API LinearColorKeyFrame final : public ColorKeyFrame {
    AERO_DECLARE_TYPE(LinearColorKeyFrame, ColorKeyFrame)
public:
    LinearColorKeyFrame() noexcept
        : ColorKeyFrame(
              StaticTypeId(),
              Presentation::DoubleKeyFrameInterpolation::Linear) {}
};

class AERO_API DiscreteColorKeyFrame final : public ColorKeyFrame {
    AERO_DECLARE_TYPE(DiscreteColorKeyFrame, ColorKeyFrame)
public:
    DiscreteColorKeyFrame() noexcept
        : ColorKeyFrame(
              StaticTypeId(),
              Presentation::DoubleKeyFrameInterpolation::Discrete) {}
};

class AERO_API EasingColorKeyFrame final : public ColorKeyFrame {
    AERO_DECLARE_TYPE(EasingColorKeyFrame, ColorKeyFrame)
public:
    EasingColorKeyFrame() noexcept
        : ColorKeyFrame(
              StaticTypeId(),
              Presentation::DoubleKeyFrameInterpolation::Easing) {}
    Base::Ref<EasingFunctionBase> EasingFunction() const noexcept {
        return easing_;
    }
    Base::Result<void> SetEasingFunction(
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
              Presentation::DoubleKeyFrameInterpolation::Spline) {}
    Base::StringView KeySpline() const noexcept {
        return keySpline_.View();
    }
    Base::Result<void> SetKeySpline(Base::StringView value) noexcept;

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
    Base::Result<void> ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<ColorKeyFrame>>
    KeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<ColorKeyFrame>> keyFrames_;
};

class AERO_API DiscreteObjectKeyFrame final : public Base::Object {
    AERO_DECLARE_TYPE(DiscreteObjectKeyFrame, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    const Core::PropertyValue& Value() const noexcept { return value_; }
    Base::StringView KeyTime() const noexcept {
        return keyTimeText_.View();
    }
    Presentation::AnimationTime KeyTimeMicroseconds() const noexcept {
        return keyTimeMicroseconds_;
    }
    Base::Result<void> SetValue(
        const Core::PropertyValue& value) noexcept;
    Base::Result<void> SetKeyTime(Base::StringView value) noexcept;

private:
    Core::PropertyValue value_;
    Base::String keyTimeText_;
    Presentation::AnimationTime keyTimeMicroseconds_ = 0U;
};

class AERO_API ObjectAnimationUsingKeyFrames final : public Timeline {
    AERO_DECLARE_TYPE(ObjectAnimationUsingKeyFrames, Timeline)
public:
    ObjectAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> TryAddKeyFrame(
        Base::Ref<DiscreteObjectKeyFrame> value) noexcept;
    Base::Result<void> ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<DiscreteObjectKeyFrame>>
    KeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<DiscreteObjectKeyFrame>> keyFrames_;
};

class AERO_API DiscreteBooleanKeyFrame final : public Base::Object {
    AERO_DECLARE_TYPE(DiscreteBooleanKeyFrame, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    bool Value() const noexcept { return value_; }
    Base::StringView KeyTime() const noexcept {
        return keyTimeText_.View();
    }
    Presentation::AnimationTime KeyTimeMicroseconds() const noexcept {
        return keyTimeMicroseconds_;
    }
    Base::Result<void> SetValue(bool value) noexcept {
        value_ = value;
        return {};
    }
    Base::Result<void> SetKeyTime(Base::StringView value) noexcept;

private:
    bool value_ = false;
    Base::String keyTimeText_;
    Presentation::AnimationTime keyTimeMicroseconds_ = 0U;
};

class AERO_API BooleanAnimationUsingKeyFrames final : public Timeline {
    AERO_DECLARE_TYPE(BooleanAnimationUsingKeyFrames, Timeline)
public:
    BooleanAnimationUsingKeyFrames() noexcept
        : Timeline(StaticTypeId()) {}
    Base::Result<void> TryAddKeyFrame(
        Base::Ref<DiscreteBooleanKeyFrame> value) noexcept;
    Base::Result<void> ClearKeyFrames() noexcept;
    Base::Span<const Base::Ref<DiscreteBooleanKeyFrame>>
    KeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

private:
    Base::Vector<Base::Ref<DiscreteBooleanKeyFrame>> keyFrames_;
};

class AERO_API Storyboard final : public Timeline {
    AERO_DECLARE_TYPE(Storyboard, Timeline)
public:
    Storyboard() noexcept : Timeline(StaticTypeId()) {}

    inline static constexpr Members::AttachedProperty<Base::String>
        TargetNameProperty{"TargetName"};
    inline static constexpr Members::AttachedProperty<Base::String>
        TargetPropertyProperty{"TargetProperty"};

    Base::Result<void> TryAddTimeline(
        Base::Ref<Timeline> value) noexcept;
    Base::Result<void> ClearTimelines() noexcept;
    Base::Span<const Base::Ref<Timeline>>
    Timelines() const noexcept {
        return {timelines_.Data(), timelines_.Size()};
    }

private:
    Base::Vector<Base::Ref<Timeline>> timelines_;
};

class AERO_API TriggerAction : public Base::Object {
    AERO_DECLARE_TYPE(TriggerAction, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }

protected:
    explicit TriggerAction(
        Core::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

private:
    Core::TypeId runtimeType_ = StaticTypeId();
};

class AERO_API ChangePropertyAction final : public TriggerAction {
    AERO_DECLARE_TYPE(ChangePropertyAction, TriggerAction)
public:
    ChangePropertyAction() noexcept
        : TriggerAction(StaticTypeId()) {}

    Base::StringView TargetName() const noexcept {
        return targetName_.View();
    }
    Base::StringView PropertyName() const noexcept {
        return propertyName_.View();
    }
    const Core::PropertyValue& Value() const noexcept {
        return value_;
    }
    Base::Result<void> SetTargetName(
        Base::StringView value) noexcept;
    Base::Result<void> SetPropertyName(
        Base::StringView value) noexcept;
    Base::Result<void> SetValue(
        const Core::PropertyValue& value) noexcept;

private:
    Base::String targetName_;
    Base::String propertyName_;
    Core::PropertyValue value_;
};

// Gallery interactivity action. It deliberately uses the trigger owner when
// no external target is supplied, matching the WPF attached-action form.
class AERO_API SetFocusAction final : public TriggerAction {
    AERO_DECLARE_TYPE(SetFocusAction, TriggerAction)
public:
    SetFocusAction() noexcept
        : TriggerAction(StaticTypeId()) {}

    bool Engage() const noexcept { return engage_; }
    Base::Result<void> SetEngage(bool value) noexcept {
        engage_ = value;
        return {};
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
    Base::StringView Path() const noexcept { return path_.View(); }
    Base::Result<void> SetPath(Base::StringView value) noexcept;
    Base::Ref<Presentation::BindingSpec> PathBinding() const noexcept {
        return pathBinding_;
    }
    Base::Result<void> SetPathBinding(
        Base::Ref<Presentation::BindingSpec> value) noexcept {
        pathBinding_ = std::move(value);
        return {};
    }
private:
    Base::String path_;
    Base::Ref<Presentation::BindingSpec> pathBinding_;
};

class AERO_API RemoveElementAction final : public TriggerAction {
    AERO_DECLARE_TYPE(RemoveElementAction, TriggerAction)
public:
    RemoveElementAction() noexcept
        : TriggerAction(StaticTypeId()) {}

    Base::Ref<Presentation::BindingSpec> TargetObject() const noexcept {
        return targetObject_;
    }
    Base::Result<void> SetTargetObject(
        Base::Ref<Presentation::BindingSpec> value) noexcept {
        targetObject_ = std::move(value);
        return {};
    }

private:
    Base::Ref<Presentation::BindingSpec> targetObject_;
};

class AERO_API BeginStoryboard final : public TriggerAction {
    AERO_DECLARE_TYPE(BeginStoryboard, TriggerAction)
public:
    BeginStoryboard() noexcept
        : TriggerAction(StaticTypeId()) {}
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::Ref<Storyboard> StoryboardValue() const noexcept {
        return storyboard_;
    }
    Base::Result<void> SetName(
        Base::StringView value) noexcept;
    Base::Result<void> SetStoryboard(
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
    Base::Ref<Storyboard> StoryboardValue() const noexcept { return storyboard_; }
    Base::Result<void> SetStoryboard(Base::Ref<Storyboard> value) noexcept { storyboard_ = std::move(value); return {}; }
    Option ControlOption() const noexcept { return option_; }
    Base::Result<void> SetControlOption(Option value) noexcept { option_ = value; return {}; }
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
    Base::StringView BeginStoryboardName() const noexcept {
        return beginStoryboardName_.View();
    }
    Base::Result<void> SetBeginStoryboardName(
        Base::StringView value) noexcept;

protected:
    explicit ControllableStoryboardAction(
        Core::TypeId runtimeType) noexcept
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
    Base::StringView Offset() const noexcept {
        return offsetText_.View();
    }
    Presentation::AnimationTime
    OffsetMicroseconds() const noexcept {
        return offsetMicroseconds_;
    }
    Base::Result<void> SetOffset(
        Base::StringView value) noexcept;

private:
    Base::String offsetText_;
    Presentation::AnimationTime
        offsetMicroseconds_ = 0U;
};

class AERO_API EventTrigger : public Base::Object {
    AERO_DECLARE_TYPE(EventTrigger, Base::Object)
public:
    EventTrigger() noexcept : EventTrigger(StaticTypeId()) {}
    Core::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Base::StringView RoutedEvent() const noexcept {
        return routedEvent_.View();
    }
    Base::StringView EventName() const noexcept {
        return routedEvent_.View();
    }
    Base::StringView SourceName() const noexcept {
        return sourceName_.View();
    }
    Base::Result<void> SetRoutedEvent(
        Base::StringView value) noexcept;
    Base::Result<void> SetEventName(
        Base::StringView value) noexcept {
        return SetRoutedEvent(value);
    }
    Base::Result<void> SetSourceName(
        Base::StringView value) noexcept;
    Base::Result<void> TryAddAction(
        Base::Ref<TriggerAction> value) noexcept;
    Base::Result<void> ClearActions() noexcept;
    Base::Span<const Base::Ref<TriggerAction>>
    Actions() const noexcept {
        return {actions_.Data(), actions_.Size()};
    }
    Base::Result<void> TryAddConditionBehavior(
        Base::Ref<Base::Object> value) noexcept {
        return behaviors_.TryPushBack(std::move(value));
    }
    void ClearConditionBehaviors() noexcept { behaviors_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> Behaviors() const noexcept {
        return {behaviors_.Data(), behaviors_.Size()};
    }

protected:
    explicit EventTrigger(Core::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

private:
    Core::TypeId runtimeType_ = StaticTypeId();
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

    std::uint32_t TotalTicks() const noexcept { return totalTicks_; }
    const Core::PropertyValue& MillisecondsPerTick() const noexcept {
        return millisecondsPerTick_;
    }
    Base::Result<void> SetTotalTicks(std::uint32_t value) noexcept {
        totalTicks_ = value;
        return {};
    }
    Base::Result<void> SetMillisecondsPerTick(
        const Core::PropertyValue& value) noexcept {
        millisecondsPerTick_ = value;
        return {};
    }

private:
    std::uint32_t totalTicks_ = 1U;
    Core::PropertyValue millisecondsPerTick_;
};

class AERO_API ComparisonCondition final : public Base::Object {
    AERO_DECLARE_TYPE(ComparisonCondition, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Ref<Presentation::BindingSpec> LeftOperand() const noexcept { return left_; }
    Base::Result<void> SetLeftOperand(Base::Ref<Presentation::BindingSpec> value) noexcept {
        left_ = std::move(value); return {};
    }
    const Core::PropertyValue& RightOperand() const noexcept { return right_; }
    Base::Result<void> SetRightOperand(const Core::PropertyValue& value) noexcept {
        right_ = value; return {};
    }
    enum class Operator : std::uint8_t {
        Equal = 0U,
        NotEqual,
        LessThan,
        LessThanOrEqual,
        GreaterThan,
        GreaterThanOrEqual,
    };
    Operator ComparisonOperator() const noexcept { return operator_; }
    Base::Result<void> SetComparisonOperator(Operator value) noexcept {
        operator_ = value; return {};
    }
private:
    Base::Ref<Presentation::BindingSpec> left_;
    Core::PropertyValue right_;
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
    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Result<void> AddCondition(Base::Ref<ComparisonCondition> value) noexcept {
        return value ? conditions_.TryPushBack(std::move(value))
            : Base::Result<void>(Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Condition is null"));
    }
    void ClearConditions() noexcept { conditions_.Clear(); }
    Base::Span<const Base::Ref<ComparisonCondition>> Conditions() const noexcept {
        return {conditions_.Data(), conditions_.Size()};
    }
    ForwardChaining Chaining() const noexcept { return chaining_; }
    Base::Result<void> SetChaining(ForwardChaining value) noexcept {
        chaining_ = value; return {};
    }
private:
    Base::Vector<Base::Ref<ComparisonCondition>> conditions_;
    ForwardChaining chaining_ = ForwardChaining::And;
};

class AERO_API ConditionBehavior final : public Base::Object {
    AERO_DECLARE_TYPE(ConditionBehavior, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Ref<ConditionalExpression> Expression() const noexcept { return expression_; }
    Base::Result<void> SetExpression(Base::Ref<ConditionalExpression> value) noexcept {
        expression_ = std::move(value); return {};
    }
private:
    Base::Ref<ConditionalExpression> expression_;
};

class AERO_API StoryboardCompletedTrigger final :
    public Base::Object {
    AERO_DECLARE_TYPE(StoryboardCompletedTrigger, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Ref<Storyboard> StoryboardValue() const noexcept {
        return storyboard_;
    }
    Base::Result<void> SetStoryboard(
        Base::Ref<Storyboard> value) noexcept;
    Base::Result<void> TryAddAction(
        Base::Ref<TriggerAction> value) noexcept;
    Base::Result<void> ClearActions() noexcept;
    Base::Span<const Base::Ref<TriggerAction>>
    Actions() const noexcept {
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

} // namespace Aero::Animation

namespace Aero::Core {

template<>
struct MetaTypeTraits<Animation::ComparisonCondition::Operator> {
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
struct MetaTypeTraits<Animation::ConditionalExpression::ForwardChaining> {
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
struct MetaTypeTraits<Animation::ControlStoryboardAction::Option> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("ControlStoryboardOption"); }
    static constexpr Base::StringView Namespace() noexcept { return AeroNamespaceUri(); }
    static constexpr Base::StringView Name() noexcept { return "ControlStoryboardOption"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Core
