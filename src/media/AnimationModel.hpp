#pragma once

#include <Aero/Animation.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Threading.hpp>
#include <Aero/DependencyProperty.hpp>
#include "gui/PropertyInternal.hpp"

#include <cstdint>

namespace Aero::Detail::Animation {

using AnimationTime = Media::Animation::AnimationTime;
using FillBehavior = Media::Animation::FillBehavior;

enum class AnimationState : std::uint8_t {
    Active = 0U,
    Paused,
    Filling,
    Stopped
};

using EasingMode = Media::Animation::EasingMode;

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

struct EasingFunction final {
    EasingFunctionKind kind = EasingFunctionKind::Linear;
    EasingMode mode = EasingMode::EaseOut;
    double power = 2.0;
    double amplitude = 1.0;
    double oscillations = 3.0;
    double springiness = 3.0;
};

struct RepeatBehavior final {
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

struct TimelineTiming final {
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

struct DoubleKeyFrame final {
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

struct ColorKeyFrame final {
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

struct DoubleAnimation final {
    double from = 0.0;
    double to = 0.0;
    double accelerationRatio = 0.0;
    double decelerationRatio = 0.0;
    TimelineTiming timing;
    EasingFunction easing;
};

struct ColorAnimation final {
    Base::Color from;
    Base::Color to;
    TimelineTiming timing;
    EasingFunction easing;
};

struct PointAnimation final {
    Base::Point from;
    Base::Point to;
    TimelineTiming timing;
    EasingFunction easing;
};

struct RectAnimation final {
    Base::Rect from;
    Base::Rect to;
    TimelineTiming timing;
    EasingFunction easing;
};

struct ThicknessAnimation final {
    Base::Thickness from;
    Base::Thickness to;
    TimelineTiming timing;
    EasingFunction easing;
};

struct DoubleKeyFrameAnimation final {
    double baseValue = 0.0;
    TimelineTiming timing;
    Base::Span<const DoubleKeyFrame> keyFrames;
};

struct ColorKeyFrameAnimation final {
    Base::Color baseValue;
    TimelineTiming timing;
    Base::Span<const ColorKeyFrame> keyFrames;
};

struct DiscreteAnimationKeyFrame final {
    AnimationTime keyTimeMicroseconds = 0U;
    Core::PropertyValue value;
};

struct DiscreteAnimation final {
    Core::PropertyValue baseValue;
    TimelineTiming timing;
    Base::Span<const DiscreteAnimationKeyFrame> keyFrames;
};

struct AnimationHandle final {
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

struct AnimationDiagnostics final {
    std::uint32_t activeCount = 0U;
    std::uint32_t pausedCount = 0U;
    std::uint32_t fillingCount = 0U;
    std::uint32_t appliedValueCount = 0U;
    std::uint32_t completedCount = 0U;
    std::uint64_t tickSequence = 0U;
};


} // namespace Aero::Detail::Animation
