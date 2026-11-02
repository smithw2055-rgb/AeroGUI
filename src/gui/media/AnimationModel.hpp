#pragma once

#include <Aero/Media/Animation.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
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
    double exponent = 2.0;
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

struct MatrixAnimation {
    Base::Transform2D from;
    Base::Transform2D to;
    TimelineTiming timing;
    EasingFunction easing;
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

inline Model::TimelineTiming ComposeTiming(
        const Timeline& timeline,
        const Model::TimelineTiming& parent) noexcept {
    Model::TimelineTiming timing = Timing(timeline);
    if (UINT64_MAX - timing.beginTimeMicroseconds <
        parent.beginTimeMicroseconds) {
        timing.beginTimeMicroseconds = UINT64_MAX;
    } else {
        timing.beginTimeMicroseconds +=
            parent.beginTimeMicroseconds;
    }
    if (timing.durationMicroseconds == 0U) {
        timing.durationMicroseconds =
            parent.durationMicroseconds;
    }
    if (parent.repeat.forever) {
        timing.repeat = parent.repeat;
    }
    timing.speedRatio *= parent.speedRatio;
    timing.autoReverse =
        timing.autoReverse || parent.autoReverse;
    return timing;
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

template <typename TKeyFrame>
inline const TKeyFrame* ResolveFinalKeyFrame(
    Base::Span<const Base::Ref<TKeyFrame>> frames,
    AnimationTime authoredDuration) noexcept {
    if (frames.Empty()) return nullptr;
    const auto schedule = MakeSchedule(frames, authoredDuration);
    const TKeyFrame* bestFrame = nullptr;
    AnimationTime bestTime = 0;
    std::uint32_t index = 0;
    for (const auto& frame : frames) {
        if (!frame) continue;
        const AnimationTime t = ResolveKeyTime(
            frame->GetKeyTime(), schedule.duration, index++, schedule.count);
        if (!bestFrame || t >= bestTime) {
            bestFrame = frame.Get();
            bestTime = t;
        }
    }
    return bestFrame;
}

inline Model::EasingFunction Easing(
        const EasingFunctionBase& easing) noexcept {
        Model::EasingFunction result;
        result.kind = static_cast<Model::EasingFunctionKind>(
            static_cast<std::uint8_t>(easing.GetKind()));
        result.mode = easing.GetEasingMode();
        result.power = easing.GetPower();
        result.exponent = easing.GetExponent();
        result.amplitude = easing.GetAmplitude();
        result.oscillations = easing.GetOscillations();
        result.springiness = easing.GetSpringiness();
        return result;
    }

} // namespace Aero::Media::Animation
