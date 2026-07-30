#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>

#include <cstdint>

namespace Aero::Presentation {

using AnimationTime = std::uint64_t;

enum class FillBehavior : std::uint8_t {
    HoldEnd = 0U,
    Stop
};

enum class AnimationState : std::uint8_t {
    Active = 0U,
    Paused,
    Filling,
    Stopped
};

enum class EasingMode : std::uint8_t {
    EaseOut = 0U,
    EaseIn,
    EaseInOut
};

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

class AERO_API AnimationManager final {
public:
    AnimationManager(
        Core::Dispatcher& dispatcher,
        Core::EffectiveValueEngine& values,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~AnimationManager() noexcept;

    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const DoubleAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const ColorAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const PointAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const RectAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const ThicknessAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const DoubleKeyFrameAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const ColorKeyFrameAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const DiscreteAnimation& animation) noexcept;

    Base::Result<void> Pause(AnimationHandle handle) noexcept;
    Base::Result<void> Resume(AnimationHandle handle) noexcept;
    Base::Result<void> Seek(
        AnimationHandle handle,
        AnimationTime offsetMicroseconds) noexcept;
    Base::Result<void> Stop(AnimationHandle handle) noexcept;
    Base::Result<void> Remove(AnimationHandle handle) noexcept;
    Base::Result<std::uint32_t> RemoveTarget(
        Core::DependencyObject& target) noexcept;
    Base::Result<void> RemoveAll() noexcept;

    Base::Result<std::uint32_t> Tick(
        AnimationTime nowMicroseconds) noexcept;
    // Samples newly-created automatic timelines at t=0 so the first submitted
    // frame has the authored initial key frame.
    Base::Result<std::uint32_t> ApplyPendingInitialValues() noexcept;
    // Called after a frame containing those initial values is submitted. The
    // automatic clock starts here rather than at storyboard construction.
    void CommitPendingInitialValues() noexcept;
    Base::Result<std::uint32_t> AdvanceBy(
        AnimationTime elapsedMicroseconds) noexcept;

    AnimationState State(AnimationHandle handle) const noexcept;
    AnimationDiagnostics Diagnostics() const noexcept;
    Base::Status LastTickStatus() const noexcept {
        return lastTickStatus_;
    }
    bool IsInitialized() const noexcept {
        return frameHook_.IsValid();
    }
    void SetAutomaticTickingEnabled(bool enabled) noexcept {
        automaticTickingEnabled_ = enabled;
    }
    bool AutomaticTickingEnabled() const noexcept {
        return automaticTickingEnabled_;
    }

    static double Ease(
        double progress,
        const EasingFunction& easing) noexcept;

private:
    struct Track;

    Core::Dispatcher* dispatcher_ = nullptr;
    Core::EffectiveValueEngine* values_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Track* tracks_ = nullptr;
    std::uint32_t trackCount_ = 0U;
    std::uint32_t trackCapacity_ = 0U;
    Core::DispatcherFrameHookHandle frameHook_;
    AnimationTime currentTimeMicroseconds_ = 0U;
    std::uint64_t nextHandle_ = 1U;
    AnimationDiagnostics diagnostics_;
    Base::Status lastTickStatus_;
    bool ticking_ = false;
    bool automaticTickingEnabled_ = true;

    Base::Result<Track*> AddTrack() noexcept;
    Track* FindTrack(AnimationHandle handle) noexcept;
    const Track* FindTrack(AnimationHandle handle) const noexcept;
    Base::Result<void> ClearTrackValue(Track& track) noexcept;
    Base::Result<bool> ApplyTrack(
        Track& track,
        AnimationTime nowMicroseconds) noexcept;
    void CompactStopped() noexcept;
    void ReleaseTracks() noexcept;

    static void AnimationFrameHook(void* context) noexcept;
};

} // namespace Aero::Presentation

namespace Aero::Core {

template<>
struct MetaTypeTraits<Presentation::FillBehavior> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("FillBehavior");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "FillBehavior";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Presentation::EasingMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("EasingMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "EasingMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
