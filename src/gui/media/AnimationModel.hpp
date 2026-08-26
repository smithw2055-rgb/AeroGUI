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
