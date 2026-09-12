#pragma once

#include <Aero/Media/Animation.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Threading.hpp>
#include <Aero/DependencyProperty.hpp>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace Aero::Media::Animation::Model {

using AnimationTime = ::Aero::Media::Animation::AnimationTime;
using FillBehavior = ::Aero::Media::Animation::FillBehavior;

enum class AnimationState : std::uint8_t {
    Active = 0U,
    Paused,
    Filling,
    Stopped
};

using EasingMode = ::Aero::Media::Animation::EasingMode;

enum class EasingFunctionKind : std::uint8_t {
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

struct EasingFunction {
    EasingFunctionKind kind = EasingFunctionKind::Linear;
    EasingMode mode = EasingMode::EaseOut;
    double power = 2.0;
    double amplitude = 1.0;
    double oscillations = 3.0;
    double springiness = 3.0;
};

struct RepeatBehavior {
    double count = 1.0;
    bool forever = false;

    static constexpr RepeatBehavior Once() noexcept {
        return {};
    }
    static constexpr RepeatBehavior Count(double value) noexcept {
        return {value, false};
    }
    static constexpr RepeatBehavior Forever() noexcept {
        return {1.0, true};
    }
};

struct TimelineTiming {
    AnimationTime beginTimeMicroseconds = 0U;
    AnimationTime durationMicroseconds = 0U;
    RepeatBehavior repeat;
    double speedRatio = 1.0;
    bool autoReverse = false;
    FillBehavior fillBehavior = FillBehavior::HoldEnd;
};

enum class DoubleKeyFrameInterpolation : std::uint8_t {
    Linear = 0U,
    Discrete,
    Easing,
    Spline
};

struct DoubleKeyFrame {
    AnimationTime keyTimeMicroseconds = 0U;
    double value = 0.0;
    DoubleKeyFrameInterpolation interpolation =
        DoubleKeyFrameInterpolation::Linear;
    EasingFunction easing;
    // Cubic Bezier control points used by Spline key frames.
    double controlPoint1X = 0.0;
    double controlPoint1Y = 0.0;
    double controlPoint2X = 1.0;
    double controlPoint2Y = 1.0;
};

struct ColorKeyFrame {
    AnimationTime keyTimeMicroseconds = 0U;
    Base::Color value;
    DoubleKeyFrameInterpolation interpolation =
        DoubleKeyFrameInterpolation::Linear;
    EasingFunction easing;
    double controlPoint1X = 0.0;
    double controlPoint1Y = 0.0;
    double controlPoint2X = 1.0;
    double controlPoint2Y = 1.0;
};

struct PointKeyFrame {
    AnimationTime keyTimeMicroseconds = 0U;
    Base::Point value;
    DoubleKeyFrameInterpolation interpolation =
        DoubleKeyFrameInterpolation::Linear;
    EasingFunction easing;
    double controlPoint1X = 0.0;
    double controlPoint1Y = 0.0;
    double controlPoint2X = 1.0;
    double controlPoint2Y = 1.0;
};

struct ThicknessKeyFrame {
    AnimationTime keyTimeMicroseconds = 0U;
    Base::Thickness value;
    DoubleKeyFrameInterpolation interpolation =
        DoubleKeyFrameInterpolation::Linear;
    EasingFunction easing;
    double controlPoint1X = 0.0;
    double controlPoint1Y = 0.0;
    double controlPoint2X = 1.0;
    double controlPoint2Y = 1.0;
};

struct DoubleAnimation {
    double from = 0.0;
    double to = 0.0;
    double accelerationRatio = 0.0;
    double decelerationRatio = 0.0;
    TimelineTiming timing;
    EasingFunction easing;
};

struct CustomDoubleAnimation {
    Base::Ref<
        ::Aero::Media::Animation::DoubleAnimationBase>
        animation;
    double defaultOriginValue = 0.0;
    double defaultDestinationValue = 0.0;
    TimelineTiming timing;
};

struct ColorAnimation {
    Base::Color from;
    Base::Color to;
    TimelineTiming timing;
    EasingFunction easing;
};

struct PointAnimation {
    Base::Point from;
    Base::Point to;
    TimelineTiming timing;
    EasingFunction easing;
};

struct RectAnimation {
    Base::Rect from;
    Base::Rect to;
    TimelineTiming timing;
    EasingFunction easing;
};

struct ThicknessAnimation {
    Base::Thickness from;
    Base::Thickness to;
    TimelineTiming timing;
    EasingFunction easing;
};

enum class IntegerAnimationWidth : std::uint8_t {
    Int16 = 0U,
    Int32,
    Int64
};

struct IntegerAnimation {
    std::int64_t from = 0;
    std::int64_t to = 0;
    IntegerAnimationWidth width = IntegerAnimationWidth::Int32;
    TimelineTiming timing;
    EasingFunction easing;
};

struct SizeAnimation {
    Base::Size from;
    Base::Size to;
    TimelineTiming timing;
    EasingFunction easing;
};

struct IntegerKeyFrame {
    AnimationTime keyTimeMicroseconds = 0U;
    std::int64_t value = 0;
    DoubleKeyFrameInterpolation interpolation =
        DoubleKeyFrameInterpolation::Linear;
    EasingFunction easing;
    double controlPoint1X = 0.0;
    double controlPoint1Y = 0.0;
    double controlPoint2X = 1.0;
    double controlPoint2Y = 1.0;
};

struct IntegerKeyFrameAnimation {
    std::int64_t baseValue = 0;
    IntegerAnimationWidth width = IntegerAnimationWidth::Int32;
    TimelineTiming timing;
    Base::Span<const IntegerKeyFrame> keyFrames;
};

struct SizeKeyFrame {
    AnimationTime keyTimeMicroseconds = 0U;
    Base::Size value;
    DoubleKeyFrameInterpolation interpolation =
        DoubleKeyFrameInterpolation::Linear;
    EasingFunction easing;
    double controlPoint1X = 0.0;
    double controlPoint1Y = 0.0;
    double controlPoint2X = 1.0;
    double controlPoint2Y = 1.0;
};

struct SizeKeyFrameAnimation {
    Base::Size baseValue;
    TimelineTiming timing;
    Base::Span<const SizeKeyFrame> keyFrames;
};

struct MatrixAnimation {
    Base::Transform2D from;
    Base::Transform2D to;
    TimelineTiming timing;
    EasingFunction easing;
};

struct MatrixKeyFrame {
    AnimationTime keyTimeMicroseconds = 0U;
    Base::Transform2D value;
    DoubleKeyFrameInterpolation interpolation =
        DoubleKeyFrameInterpolation::Linear;
    EasingFunction easing;
    double controlPoint1X = 0.0;
    double controlPoint1Y = 0.0;
    double controlPoint2X = 1.0;
    double controlPoint2Y = 1.0;
};

struct MatrixKeyFrameAnimation {
    Base::Transform2D baseValue;
    TimelineTiming timing;
    Base::Span<const MatrixKeyFrame> keyFrames;
};

struct DoubleKeyFrameAnimation {
    double baseValue = 0.0;
    TimelineTiming timing;
    Base::Span<const DoubleKeyFrame> keyFrames;
};

struct ColorKeyFrameAnimation {
    Base::Color baseValue;
    TimelineTiming timing;
    Base::Span<const ColorKeyFrame> keyFrames;
};

struct PointKeyFrameAnimation {
    Base::Point baseValue;
    TimelineTiming timing;
    Base::Span<const PointKeyFrame> keyFrames;
};

struct ThicknessKeyFrameAnimation {
    Base::Thickness baseValue;
    TimelineTiming timing;
    Base::Span<const ThicknessKeyFrame> keyFrames;
};

struct DiscreteAnimationKeyFrame {
    AnimationTime keyTimeMicroseconds = 0U;
    Meta::PropertyValue value;
};

struct DiscreteAnimation {
    Meta::PropertyValue baseValue;
    TimelineTiming timing;
    Base::Span<const DiscreteAnimationKeyFrame> keyFrames;
};

struct AnimationHandle {
    std::uint64_t value = 0U;

    constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

constexpr bool operator==(
    AnimationHandle left, AnimationHandle right) noexcept {
    return left.value == right.value;
}

constexpr bool operator!=(
    AnimationHandle left, AnimationHandle right) noexcept {
    return !(left == right);
}

struct AnimationDiagnostics {
    std::uint32_t activeCount = 0U;
    std::uint32_t pausedCount = 0U;
    std::uint32_t fillingCount = 0U;
    std::uint32_t appliedValueCount = 0U;
    std::uint32_t completedCount = 0U;
    std::uint64_t tickSequence = 0U;
};


} // namespace Aero::Media::Animation::Model

// Public Animation → Model conversion factories (absorbed from BrushRendering/AnimationModel.hpp).
namespace Aero::Media::Animation {

inline Model::TimelineTiming Timing(
        const Timeline& timeline) noexcept {
        Model::TimelineTiming result;
        result.beginTimeMicroseconds =
            timeline.GetBeginTime().Microseconds();
        const Duration duration = timeline.GetDuration();
        if (duration.IsForever()) {
            result.durationMicroseconds = UINT64_MAX;
        } else if (duration.HasTimeSpan()) {
            result.durationMicroseconds =
                duration.GetTimeSpan().Microseconds();
        } else {
            result.durationMicroseconds = 0U;
        }
        const RepeatBehavior repeat = timeline.GetRepeatBehavior();
        if (repeat.IsForever()) {
            result.repeat = Model::RepeatBehavior::Forever();
        } else if (repeat.HasDuration()) {
            const std::uint64_t span = repeat.GetDuration().Microseconds();
            if (result.durationMicroseconds > 0U &&
                result.durationMicroseconds != UINT64_MAX) {
                result.repeat = Model::RepeatBehavior::Count(
                    static_cast<double>(span) /
                    static_cast<double>(result.durationMicroseconds));
            } else {
                result.repeat = Model::RepeatBehavior::Once();
            }
        } else {
            result.repeat = Model::RepeatBehavior::Count(repeat.GetCount());
        }
        result.speedRatio = timeline.GetSpeedRatio();
        result.autoReverse = timeline.GetAutoReverse();
        result.fillBehavior = timeline.GetFillBehavior();
        return result;
    }

struct KeyframeSchedule {
    AnimationTime duration = 0U;
    std::uint32_t count = 0U;
};

template<class TKeyFrame>
inline KeyframeSchedule MakeSchedule(
        Base::Span<const Base::Ref<TKeyFrame>> frames,
        AnimationTime authoredDuration) noexcept {
        KeyframeSchedule schedule;
        AnimationTime maxTimeSpan = 0U;
        for (const Base::Ref<TKeyFrame>& frame : frames) {
            if (!frame) continue;
            ++schedule.count;
            const KeyTime keyTime = frame->GetKeyTime();
            if (keyTime.IsTimeSpan() &&
                keyTime.GetTimeSpan().Microseconds() > maxTimeSpan) {
                maxTimeSpan = keyTime.GetTimeSpan().Microseconds();
            }
        }
        schedule.duration =
            (authoredDuration == 0U || authoredDuration == UINT64_MAX)
            ? maxTimeSpan
            : authoredDuration;
        return schedule;
    }

inline AnimationTime ResolveKeyTime(
        const KeyTime& keyTime,
        AnimationTime duration,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        return keyTime.ResolveMicroseconds(duration, index, count);
    }

inline Model::EasingFunction Easing(
        const EasingFunctionBase& easing) noexcept {
        Model::EasingFunction result;
        result.kind = static_cast<Model::EasingFunctionKind>(
            static_cast<std::uint8_t>(easing.GetKind()));
        result.mode = easing.GetEasingMode();
        if (easing.RuntimeType() == PowerEase::StaticTypeId()) {
            result.power = static_cast<const PowerEase&>(easing).GetPower();
        } else if (easing.RuntimeType() == ExponentialEase::StaticTypeId()) {
            result.power = static_cast<const ExponentialEase&>(easing).GetExponent();
        } else if (easing.RuntimeType() == BackEase::StaticTypeId()) {
            result.amplitude = static_cast<const BackEase&>(easing).GetAmplitude();
        } else if (easing.RuntimeType() == BounceEase::StaticTypeId()) {
            const auto& bounce = static_cast<const BounceEase&>(easing);
            result.oscillations = bounce.GetBounces();
            result.springiness = bounce.GetBounciness();
        } else if (easing.RuntimeType() == ElasticEase::StaticTypeId()) {
            const auto& elastic = static_cast<const ElasticEase&>(easing);
            result.oscillations = elastic.GetOscillations();
            result.springiness = elastic.GetSpringiness();
        }
        return result;
    }

inline Model::DoubleAnimation Double(
        const DoubleAnimation& animation) noexcept {
        Model::DoubleAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.accelerationRatio = animation.GetAccelerationRatio();
        result.decelerationRatio = animation.GetDecelerationRatio();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::ColorAnimation Color(
        const ColorAnimation& animation) noexcept {
        Model::ColorAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::PointAnimation Point(
        const PointAnimation& animation) noexcept {
        Model::PointAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::RectAnimation Rect(
        const RectAnimation& animation) noexcept {
        Model::RectAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::ThicknessAnimation Thickness(
        const ThicknessAnimation& animation) noexcept {
        Model::ThicknessAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::IntegerAnimation Integer16(
        const Int16Animation& animation) noexcept {
        Model::IntegerAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.width = Model::IntegerAnimationWidth::Int16;
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::IntegerAnimation Integer32(
        const Int32Animation& animation) noexcept {
        Model::IntegerAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.width = Model::IntegerAnimationWidth::Int32;
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::IntegerAnimation Integer64(
        const Int64Animation& animation) noexcept {
        Model::IntegerAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.width = Model::IntegerAnimationWidth::Int64;
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::SizeAnimation Size(
        const SizeAnimation& animation) noexcept {
        Model::SizeAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::MatrixAnimation Matrix(
        const MatrixAnimation& animation) noexcept {
        Model::MatrixAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::DoubleKeyFrame DoubleFrame(
        const DoubleKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::DoubleKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::ColorKeyFrame ColorFrame(
        const ColorKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::ColorKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::PointKeyFrame PointFrame(
        const PointKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::PointKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::ThicknessKeyFrame ThicknessFrame(
        const ThicknessKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::ThicknessKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::IntegerKeyFrame IntegerFrame(
        const Int16KeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::IntegerKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::IntegerKeyFrame IntegerFrame(
        const Int32KeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::IntegerKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::IntegerKeyFrame IntegerFrame(
        const Int64KeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::IntegerKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::SizeKeyFrame SizeFrame(
        const SizeKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::SizeKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

inline Model::MatrixKeyFrame MatrixFrame(
        const MatrixKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::MatrixKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
}

} // namespace Aero::Media::Animation

