#include "gui/media/AnimationModel.hpp"
#include <Aero/Media/Brushes.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Transforms.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Value.hpp>
#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <new>
#include <utility>
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"

namespace Aero::Media::Animation::Model {
namespace {

constexpr double Pi = 3.1415926535897932384626433832795;

Base::Status InvalidAnimation(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

double Clamp01(double value) noexcept {
    return std::max(0.0, std::min(1.0, value));
}

double ApplyAccelerationDeceleration(
    double progress, double acceleration, double deceleration) noexcept {
    const double value = Clamp01(progress);
    if (acceleration <= 0.0 && deceleration <= 0.0) {
        return value;
    }
    const double maximumVelocity =
        1.0 / (1.0 - (acceleration + deceleration) * 0.5);
    if (acceleration > 0.0 && value < acceleration) {
        return 0.5 * maximumVelocity * value * value / acceleration;
    }
    const double beforeDeceleration =
        1.0 - deceleration;
    const double accumulatedBeforeDeceleration =
        maximumVelocity * (beforeDeceleration - acceleration * 0.5);
    if (deceleration <= 0.0 || value <= beforeDeceleration) {
        return accumulatedBeforeDeceleration +
            maximumVelocity * (value - beforeDeceleration);
    }
    const double elapsed = value - beforeDeceleration;
    return accumulatedBeforeDeceleration + maximumVelocity * elapsed -
        0.5 * maximumVelocity * elapsed * elapsed / deceleration;
}

double EaseOutBounce(double value) noexcept {
    constexpr double n1 = 7.5625;
    constexpr double d1 = 2.75;
    if (value < 1.0 / d1) {
        return n1 * value * value;
    }
    if (value < 2.0 / d1) {
        value -= 1.5 / d1;
        return n1 * value * value + 0.75;
    }
    if (value < 2.5 / d1) {
        value -= 2.25 / d1;
        return n1 * value * value + 0.9375;
    }
    value -= 2.625 / d1;
    return n1 * value * value + 0.984375;
}

double CubicBezierCoordinate(
    double t, double first, double second) noexcept {
    const double inverse = 1.0 - t;
    return 3.0 * inverse * inverse * t * first +
        3.0 * inverse * t * t * second + t * t * t;
}

double CubicBezierDerivative(
    double t, double first, double second) noexcept {
    const double inverse = 1.0 - t;
    return 3.0 * inverse * inverse * first +
        6.0 * inverse * t * (second - first) +
        3.0 * t * t * (1.0 - second);
}

template<class TKeyFrame>
double EvaluateSpline(
    double progress,
    const TKeyFrame& frame) noexcept {
    const double target = Clamp01(progress);
    double parameter = target;
    for (std::uint32_t iteration = 0U; iteration < 8U; ++iteration) {
        const double x = CubicBezierCoordinate(
            parameter, frame.controlPoint1X, frame.controlPoint2X);
        const double derivative = CubicBezierDerivative(
            parameter, frame.controlPoint1X, frame.controlPoint2X);
        if (std::abs(derivative) < 1.0e-7) break;
        parameter = Clamp01(parameter - (x - target) / derivative);
    }
    double low = 0.0;
    double high = 1.0;
    for (std::uint32_t iteration = 0U; iteration < 12U; ++iteration) {
        const double x = CubicBezierCoordinate(
            parameter, frame.controlPoint1X, frame.controlPoint2X);
        if (x < target) low = parameter;
        else high = parameter;
        parameter = (low + high) * 0.5;
    }
    return Clamp01(CubicBezierCoordinate(
        parameter, frame.controlPoint1Y, frame.controlPoint2Y));
}

bool IsTimingValid(const TimelineTiming& timing) noexcept {
    return std::isfinite(timing.speedRatio) &&
        timing.speedRatio > 0.0 &&
        (timing.repeat.forever ||
            (std::isfinite(timing.repeat.count) &&
             timing.repeat.count > 0.0));
}

} // namespace

} // namespace Aero::Media::Animation::Model

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Media::Animation::Model;
using namespace Aero::Media;

struct AnimationEngine::Track {
    enum class Kind : std::uint8_t {
        Double,
        CustomDouble,
        Color,
        Point,
        Rect,
        Thickness,
        DoubleKeyFrames,
        ColorKeyFrames,
        Discrete
    };

    explicit Track(Base::IAllocator* allocator) noexcept
        : doubleFrames(allocator),
          colorFrames(allocator),
          discreteFrames(allocator) {}

    Track(Track&&) noexcept = default;
    Track& operator=(Track&&) noexcept = default;
    Track(const Track&) = delete;
    Track& operator=(const Track&) = delete;

    AnimationHandle handle;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;
    TimelineTiming timing;
    EasingFunction easing;
    double accelerationRatio = 0.0;
    double decelerationRatio = 0.0;
    Kind kind = Kind::Double;
    AnimationState state = AnimationState::Active;
    AnimationTime startTimeMicroseconds = 0U;
    AnimationTime pauseTimeMicroseconds = 0U;
    AnimationTime seekOffsetMicroseconds = 0U;
    AnimationTime accumulatedPauseMicroseconds = 0U;
    double from = 0.0;
    double to = 0.0;
    double baseValue = 0.0;
    double defaultDestinationValue = 0.0;
    Base::Ref<
        ::Aero::Media::Animation::DoubleAnimationBase>
        customDouble;
    Base::Color fromColor;
    Base::Color toColor;
    Base::Point fromPoint;
    Base::Point toPoint;
    Base::Rect fromRect;
    Base::Rect toRect;
    Base::Thickness fromThickness;
    Base::Thickness toThickness;
    Base::Vector<DoubleKeyFrame> doubleFrames;
    Base::Vector<ColorKeyFrame> colorFrames;
    Base::Vector<DiscreteAnimationKeyFrame> discreteFrames;
    Meta::PropertyValue discreteBaseValue;
    bool valueApplied = false;
    bool completedCounted = false;
    bool pendingInitialSample = false;
};

AnimationEngine::AnimationEngine(
    ::Aero::Threading::Dispatcher& dispatcher,
    Meta::EffectiveValueEngine& values,
    Base::IAllocator* allocator) noexcept
    : dispatcher_(&dispatcher),
      values_(&values),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

AnimationEngine::~AnimationEngine() noexcept {
    Shutdown();
}

Base::Result<void> AnimationEngine::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (frameHook_.IsValid()) return {};
    Base::Result<::Aero::Threading::DispatcherFrameHookHandle> hook =
        dispatcher_->RegisterFrameHook(
            ::Aero::Threading::DispatcherFramePhase::Animation,
            &AnimationEngine::AnimationFrameHook,
            this);
    if (!hook) return hook.GetStatus();
    frameHook_ = hook.Value();
    currentTimeMicroseconds_ = dispatcher_->NowMicroseconds();
    lastTickStatus_ = Base::Status::Ok();
    return {};
}

void AnimationEngine::Shutdown() noexcept {
    if (dispatcher_ != nullptr && dispatcher_->CheckAccess()) {
        static_cast<void>(RemoveAll());
        if (frameHook_.IsValid()) {
            static_cast<void>(
                dispatcher_->RemoveFrameHook(frameHook_));
        }
    }
    frameHook_ = {};
    ReleaseTracks();
}

Base::Result<AnimationEngine::Track*>
AnimationEngine::AddTrack() noexcept {
    if (!frameHook_.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "AnimationEngine is not initialized");
    }
    if (trackCount_ == trackCapacity_) {
        const std::uint32_t replacementCapacity =
            trackCapacity_ == 0U ? 8U : trackCapacity_ * 2U;
        if (replacementCapacity < trackCapacity_) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Animation track capacity overflow");
        }
        const std::size_t bytes =
            sizeof(Track) * replacementCapacity;
        void* allocation = allocator_->Allocate({
            bytes, alignof(Track), Base::MemoryTag::Ui});
        if (allocation == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Animation track allocation failed");
        }
        Track* replacement = static_cast<Track*>(allocation);
        for (std::uint32_t index = 0U; index < trackCount_; ++index) {
            new (replacement + index)
                Track(std::move(tracks_[index]));
            tracks_[index].~Track();
        }
        if (tracks_ != nullptr) {
            allocator_->Deallocate(
                tracks_,
                sizeof(Track) * trackCapacity_,
                alignof(Track),
                Base::MemoryTag::Ui);
        }
        tracks_ = replacement;
        trackCapacity_ = replacementCapacity;
    }
    Track* track = new (tracks_ + trackCount_) Track(allocator_);
    ++trackCount_;
    return track;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const DoubleAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(animation.timing) ||
        !std::isfinite(animation.from) ||
        !std::isfinite(animation.to) ||
        !std::isfinite(animation.accelerationRatio) ||
        !std::isfinite(animation.decelerationRatio) ||
        animation.accelerationRatio < 0.0 ||
        animation.decelerationRatio < 0.0 ||
        animation.accelerationRatio + animation.decelerationRatio > 1.0) {
        return InvalidAnimation(
            "DoubleAnimation has invalid target, timing, or values");
    }
    Base::Result<Track*> added = AddTrack();
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.easing = animation.easing;
    track.accelerationRatio = animation.accelerationRatio;
    track.decelerationRatio = animation.decelerationRatio;
    track.kind = Track::Kind::Double;
    track.from = animation.from;
    track.to = animation.to;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = automaticTickingEnabled_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const CustomDoubleAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() ||
        !animation.animation ||
        !IsTimingValid(animation.timing) ||
        !std::isfinite(animation.defaultOriginValue) ||
        !std::isfinite(animation.defaultDestinationValue)) {
        return InvalidAnimation(
            "Custom DoubleAnimation has invalid target, timing, or values");
    }
    Base::Result<Track*> added = AddTrack();
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.kind = Track::Kind::CustomDouble;
    track.baseValue = animation.defaultOriginValue;
    track.defaultDestinationValue =
        animation.defaultDestinationValue;
    track.customDouble = animation.animation;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = automaticTickingEnabled_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const RectAnimation& animation) noexcept {
    Base::Result<void> access =
        dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() ||
        !IsTimingValid(animation.timing) ||
        !Base::IsFiniteRect(animation.from) ||
        !Base::IsFiniteRect(animation.to)) {
        return InvalidAnimation(
            "RectAnimation has invalid target, timing, or values");
    }
    Base::Result<Track*> added = AddTrack();
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.easing = animation.easing;
    track.kind = Track::Kind::Rect;
    track.fromRect = animation.from;
    track.toRect = animation.to;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = automaticTickingEnabled_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const ThicknessAnimation& animation) noexcept {
    Base::Result<void> access =
        dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    const auto finite = [](Base::Thickness value) noexcept {
        return std::isfinite(value.left) &&
            std::isfinite(value.top) &&
            std::isfinite(value.right) &&
            std::isfinite(value.bottom);
    };
    if (!property.IsValid() ||
        !IsTimingValid(animation.timing) ||
        !finite(animation.from) ||
        !finite(animation.to)) {
        return InvalidAnimation(
            "ThicknessAnimation has invalid target, timing, or values");
    }
    Base::Result<Track*> added = AddTrack();
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.easing = animation.easing;
    track.kind = Track::Kind::Thickness;
    track.fromThickness = animation.from;
    track.toThickness = animation.to;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = automaticTickingEnabled_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const PointAnimation& animation) noexcept {
    Base::Result<void> access =
        dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    const auto finite = [](Base::Point value) noexcept {
        return std::isfinite(value.x) &&
            std::isfinite(value.y);
    };
    if (!property.IsValid() ||
        !IsTimingValid(animation.timing) ||
        !finite(animation.from) ||
        !finite(animation.to)) {
        return InvalidAnimation(
            "PointAnimation has invalid target, timing, or values");
    }
    Base::Result<Track*> added = AddTrack();
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.easing = animation.easing;
    track.kind = Track::Kind::Point;
    track.fromPoint = animation.from;
    track.toPoint = animation.to;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = automaticTickingEnabled_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const ColorAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(animation.timing) ||
        !Base::IsFiniteColor(animation.from) ||
        !Base::IsFiniteColor(animation.to)) {
        return InvalidAnimation(
            "ColorAnimation has invalid target, timing, or values");
    }
    Base::Result<Track*> added = AddTrack();
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.easing = animation.easing;
    track.kind = Track::Kind::Color;
    track.fromColor = animation.from;
    track.toColor = animation.to;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = automaticTickingEnabled_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const DoubleKeyFrameAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(animation.timing) ||
        !std::isfinite(animation.baseValue) ||
        animation.keyFrames.Empty()) {
        return InvalidAnimation(
            "Double key-frame animation is incomplete");
    }
    AnimationTime lastKeyTime = 0U;
    for (std::uint32_t index = 0U;
         index < animation.keyFrames.Size(); ++index) {
        const DoubleKeyFrame& frame = animation.keyFrames[index];
        if (!std::isfinite(frame.value) ||
            (index != 0U &&
             frame.keyTimeMicroseconds < lastKeyTime)) {
            return InvalidAnimation(
                "Double key frames must be finite and ordered");
        }
        lastKeyTime = frame.keyTimeMicroseconds;
    }
    Base::Result<Track*> added = AddTrack();
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    Base::Result<void> copied =
        track.doubleFrames.Append(animation.keyFrames);
    if (!copied) {
        track.state = AnimationState::Stopped;
        CompactStopped();
        return copied.GetStatus();
    }
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.kind = Track::Kind::DoubleKeyFrames;
    track.baseValue = animation.baseValue;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = automaticTickingEnabled_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const ColorKeyFrameAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() ||
        !IsTimingValid(animation.timing) ||
        !Base::IsFiniteColor(animation.baseValue) ||
        animation.keyFrames.Empty()) {
        return InvalidAnimation(
            "Color key-frame animation is incomplete");
    }
    AnimationTime lastKeyTime = 0U;
    for (std::uint32_t index = 0U;
         index < animation.keyFrames.Size(); ++index) {
        const ColorKeyFrame& frame =
            animation.keyFrames[index];
        if (!Base::IsFiniteColor(frame.value) ||
            (index != 0U &&
             frame.keyTimeMicroseconds < lastKeyTime)) {
            return InvalidAnimation(
                "Color key frames must be finite and ordered");
        }
        lastKeyTime = frame.keyTimeMicroseconds;
    }
    Base::Result<Track*> added = AddTrack();
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    Base::Result<void> copied =
        track.colorFrames.Append(animation.keyFrames);
    if (!copied) {
        track.state = AnimationState::Stopped;
        CompactStopped();
        return copied.GetStatus();
    }
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.kind = Track::Kind::ColorKeyFrames;
    track.fromColor = animation.baseValue;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = automaticTickingEnabled_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const DiscreteAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(animation.timing) ||
        animation.baseValue.IsUnset() ||
        animation.keyFrames.Empty()) {
        return InvalidAnimation(
            "Discrete animation is incomplete");
    }
    AnimationTime lastKeyTime = 0U;
    for (std::uint32_t index = 0U;
         index < animation.keyFrames.Size(); ++index) {
        const DiscreteAnimationKeyFrame& frame =
            animation.keyFrames[index];
        if (frame.value.IsUnset() ||
            (index != 0U &&
             frame.keyTimeMicroseconds < lastKeyTime)) {
            return InvalidAnimation(
                "Discrete key frames must be set and ordered");
        }
        lastKeyTime = frame.keyTimeMicroseconds;
    }
    Base::Result<Track*> added = AddTrack();
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    Base::Result<void> copied =
        track.discreteFrames.Append(animation.keyFrames);
    if (!copied) {
        track.state = AnimationState::Stopped;
        CompactStopped();
        return copied.GetStatus();
    }
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.kind = Track::Kind::Discrete;
    track.discreteBaseValue = animation.baseValue;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = automaticTickingEnabled_;
    return track.handle;
}

AnimationEngine::Track* AnimationEngine::FindTrack(
    AnimationHandle handle) noexcept {
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        if (tracks_[index].handle == handle) return tracks_ + index;
    }
    return nullptr;
}

const AnimationEngine::Track* AnimationEngine::FindTrack(
    AnimationHandle handle) const noexcept {
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        if (tracks_[index].handle == handle) return tracks_ + index;
    }
    return nullptr;
}

Base::Result<void> AnimationEngine::Pause(
    AnimationHandle handle) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    Track* track = FindTrack(handle);
    if (track == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound, "Animation handle was not found");
    }
    if (track->state == AnimationState::Active) {
        track->state = AnimationState::Paused;
        track->pauseTimeMicroseconds = currentTimeMicroseconds_;
    }
    return {};
}

Base::Result<void> AnimationEngine::Resume(
    AnimationHandle handle) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    Track* track = FindTrack(handle);
    if (track == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound, "Animation handle was not found");
    }
    if (track->state == AnimationState::Paused) {
        const AnimationTime now = currentTimeMicroseconds_;
        if (now >= track->pauseTimeMicroseconds) {
            track->accumulatedPauseMicroseconds +=
                now - track->pauseTimeMicroseconds;
        }
        track->state = AnimationState::Active;
    }
    return {};
}

Base::Result<void> AnimationEngine::Seek(
    AnimationHandle handle,
    AnimationTime offsetMicroseconds) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    Track* track = FindTrack(handle);
    if (track == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound, "Animation handle was not found");
    }
    track->seekOffsetMicroseconds = offsetMicroseconds;
    track->startTimeMicroseconds = currentTimeMicroseconds_;
    track->pendingInitialSample = false;
    track->accumulatedPauseMicroseconds = 0U;
    if (track->state == AnimationState::Paused) {
        track->pauseTimeMicroseconds = track->startTimeMicroseconds;
    } else {
        track->state = AnimationState::Active;
    }
    track->completedCounted = false;
    return {};
}

Base::Result<void> AnimationEngine::ClearTrackValue(
    Track& track) noexcept {
    if (!track.valueApplied || track.target == nullptr) return {};
    Base::Result<void> cleared = values_->ClearAnimationValue(
        *track.target, track.property);
    if (!cleared) return cleared.GetStatus();
    track.valueApplied = false;
    return {};
}

Base::Result<void> AnimationEngine::Stop(
    AnimationHandle handle) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    Track* track = FindTrack(handle);
    if (track == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound, "Animation handle was not found");
    }
    Base::Result<void> cleared = ClearTrackValue(*track);
    if (!cleared) return cleared.GetStatus();
    track->state = AnimationState::Stopped;
    Base::Result<std::uint32_t> flushed = values_->Flush();
    if (!flushed) return flushed.GetStatus();
    return {};
}

Base::Result<void> AnimationEngine::Remove(
    AnimationHandle handle) noexcept {
    Base::Result<void> stopped = Stop(handle);
    if (!stopped) return stopped.GetStatus();
    CompactStopped();
    return {};
}

Base::Result<std::uint32_t> AnimationEngine::RemoveTarget(
    ::Aero::DependencyObject& target) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    std::uint32_t removed = 0U;
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        Track& track = tracks_[index];
        if (track.target != &target) continue;
        Base::Result<void> cleared = ClearTrackValue(track);
        if (!cleared) return cleared.GetStatus();
        track.state = AnimationState::Stopped;
        ++removed;
    }
    CompactStopped();
    if (removed != 0U) {
        Base::Result<std::uint32_t> flushed = values_->Flush();
        if (!flushed) return flushed.GetStatus();
    }
    return removed;
}

Base::Result<void> AnimationEngine::RemoveAll() noexcept {
    if (dispatcher_ == nullptr || !dispatcher_->CheckAccess()) {
        return Base::Status::Failure(
            Base::ErrorCode::WrongThread,
            "AnimationEngine removal requires dispatcher access");
    }
    bool changed = false;
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        const bool hadValue = tracks_[index].valueApplied;
        Base::Result<void> cleared = ClearTrackValue(tracks_[index]);
        if (!cleared) return cleared.GetStatus();
        changed = changed || hadValue;
        tracks_[index].state = AnimationState::Stopped;
    }
    CompactStopped();
    if (changed) {
        Base::Result<std::uint32_t> flushed = values_->Flush();
        if (!flushed) return flushed.GetStatus();
    }
    return {};
}

double AnimationEngine::Ease(
    double progress,
    const EasingFunction& easing) noexcept {
    const double value = Clamp01(progress);
    const auto easeIn = [&](double input) noexcept {
        switch (easing.kind) {
        case EasingFunctionKind::Linear:
            return input;
        case EasingFunctionKind::Sine:
            return 1.0 - std::cos(input * Pi * 0.5);
        case EasingFunctionKind::Quadratic:
            return input * input;
        case EasingFunctionKind::Cubic:
            return input * input * input;
        case EasingFunctionKind::Quartic:
            return input * input * input * input;
        case EasingFunctionKind::Quintic:
            return input * input * input * input * input;
        case EasingFunctionKind::Circle:
            return 1.0 - std::sqrt(
                std::max(0.0, 1.0 - input * input));
        case EasingFunctionKind::Power:
            return std::pow(input, std::max(0.0, easing.power));
        case EasingFunctionKind::Exponential: {
            const double exponent =
                std::max(0.0, easing.power);
            return input <= 0.0
                ? 0.0
                : (std::exp(exponent * input) - 1.0) /
                    std::max(
                        1.0e-9,
                        std::exp(exponent) - 1.0);
        }
        case EasingFunctionKind::Back: {
            const double amplitude =
                std::max(0.0, easing.amplitude);
            return input * input *
                ((amplitude + 1.0) * input - amplitude);
        }
        case EasingFunctionKind::Bounce:
            return 1.0 - EaseOutBounce(1.0 - input);
        case EasingFunctionKind::Elastic: {
            if (input <= 0.0 || input >= 1.0) return input;
            const double oscillations =
                std::max(1.0, easing.oscillations);
            const double springiness =
                std::max(0.0, easing.springiness);
            const double envelope = springiness == 0.0
                ? input
                : (std::exp(springiness * input) - 1.0) /
                    (std::exp(springiness) - 1.0);
            return envelope *
                std::sin((input * oscillations - 0.25) * 2.0 * Pi);
        }
        }
        return input;
    };
    switch (easing.mode) {
    case EasingMode::EaseIn:
        return easeIn(value);
    case EasingMode::EaseOut:
        return 1.0 - easeIn(1.0 - value);
    case EasingMode::EaseInOut:
        return value < 0.5
            ? easeIn(value * 2.0) * 0.5
            : 1.0 - easeIn((1.0 - value) * 2.0) * 0.5;
    }
    return value;
}

Base::Result<bool> AnimationEngine::ApplyTrack(
    Track& track,
    AnimationTime nowMicroseconds) noexcept {
    if (track.state == AnimationState::Stopped ||
        track.state == AnimationState::Filling) {
        return false;
    }
    AnimationTime sampledNow = track.pendingInitialSample
        ? track.startTimeMicroseconds
        : nowMicroseconds;
    if (!track.pendingInitialSample &&
        track.state == AnimationState::Paused) {
        sampledNow = track.pauseTimeMicroseconds;
    }
    const AnimationTime elapsedClock =
        sampledNow >= track.startTimeMicroseconds
        ? sampledNow - track.startTimeMicroseconds
        : 0U;
    const AnimationTime unpausedClock =
        elapsedClock >= track.accumulatedPauseMicroseconds
        ? elapsedClock - track.accumulatedPauseMicroseconds
        : 0U;
    const long double scaled =
        static_cast<long double>(unpausedClock) *
        track.timing.speedRatio +
        static_cast<long double>(track.seekOffsetMicroseconds);
    AnimationTime localTime = scaled >=
            static_cast<long double>(UINT64_MAX)
        ? UINT64_MAX
        : static_cast<AnimationTime>(scaled);
    if (localTime < track.timing.beginTimeMicroseconds) {
        return false;
    }
    localTime -= track.timing.beginTimeMicroseconds;

    const AnimationTime duration =
        track.timing.durationMicroseconds;
    const long double cycleDuration = static_cast<long double>(
        duration) * (track.timing.autoReverse ? 2.0L : 1.0L);
    const long double activeDuration = track.timing.repeat.forever
        ? static_cast<long double>(UINT64_MAX)
        : cycleDuration * track.timing.repeat.count;
    const bool completed = duration == 0U ||
        (!track.timing.repeat.forever &&
         static_cast<long double>(localTime) >= activeDuration);

    double progress = 1.0;
    AnimationTime sampleTime = duration;
    if (!completed && duration != 0U) {
        long double within = std::fmod(
            static_cast<long double>(localTime),
            cycleDuration);
        if (track.timing.autoReverse &&
            within > static_cast<long double>(duration)) {
            within = cycleDuration - within;
        }
        within = std::max(
            0.0L,
            std::min(
                within,
                static_cast<long double>(duration)));
        sampleTime =
            static_cast<AnimationTime>(within);
        progress = static_cast<double>(sampleTime) /
            static_cast<double>(duration);
    } else if (completed && track.timing.autoReverse) {
        progress = 0.0;
        sampleTime = 0U;
    }

    Meta::PropertyValue value;
    if (track.kind == Track::Kind::Double) {
        const double eased = Ease(
            ApplyAccelerationDeceleration(
                progress,
                track.accelerationRatio,
                track.decelerationRatio),
            track.easing);
        value = Meta::ValueCodec<double>::Encode(
            track.from + (track.to - track.from) * eased).Value();
    } else if (track.kind == Track::Kind::CustomDouble) {
        if (!track.customDouble) {
            return InvalidAnimation(
                "Custom DoubleAnimation object is unavailable");
        }
        const double sampled =
            track.customDouble->GetCurrentValue(
                track.baseValue,
                track.defaultDestinationValue,
                progress);
        if (!std::isfinite(sampled)) {
            return InvalidAnimation(
                "Custom DoubleAnimation returned a non-finite value");
        }
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<double>::Encode(sampled);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::Color) {
        const float eased =
            static_cast<float>(Ease(progress, track.easing));
        const Base::Color color{
            track.fromColor.red +
                (track.toColor.red - track.fromColor.red) * eased,
            track.fromColor.green +
                (track.toColor.green - track.fromColor.green) * eased,
            track.fromColor.blue +
                (track.toColor.blue - track.fromColor.blue) * eased,
            track.fromColor.alpha +
                (track.toColor.alpha - track.fromColor.alpha) * eased};
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Color>::Encode(color);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::Point) {
        const double eased =
            Ease(progress, track.easing);
        const Base::Point point{
            track.fromPoint.x +
                (track.toPoint.x -
                 track.fromPoint.x) * eased,
            track.fromPoint.y +
                (track.toPoint.y -
                 track.fromPoint.y) * eased};
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Point>::Encode(
                point);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::Rect) {
        const double eased =
            Ease(progress, track.easing);
        const Base::Rect rect{
            track.fromRect.x +
                (track.toRect.x -
                 track.fromRect.x) * eased,
            track.fromRect.y +
                (track.toRect.y -
                 track.fromRect.y) * eased,
            track.fromRect.width +
                (track.toRect.width -
                 track.fromRect.width) * eased,
            track.fromRect.height +
                (track.toRect.height -
                 track.fromRect.height) * eased};
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Rect>::Encode(
                rect);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind ==
        Track::Kind::Thickness) {
        const double eased =
            Ease(progress, track.easing);
        const Base::Thickness thickness{
            track.fromThickness.left +
                (track.toThickness.left -
                 track.fromThickness.left) * eased,
            track.fromThickness.top +
                (track.toThickness.top -
                 track.fromThickness.top) * eased,
            track.fromThickness.right +
                (track.toThickness.right -
                 track.fromThickness.right) * eased,
            track.fromThickness.bottom +
                (track.toThickness.bottom -
                 track.fromThickness.bottom) * eased};
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Thickness>::
                Encode(thickness);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::DoubleKeyFrames) {
        double previousValue = track.baseValue;
        AnimationTime previousTime = 0U;
        double sampledValue = previousValue;
        bool found = false;
        for (std::uint32_t index = 0U;
             index < track.doubleFrames.Size(); ++index) {
            const DoubleKeyFrame& frame =
                track.doubleFrames[index];
            if (sampleTime > frame.keyTimeMicroseconds) {
                previousValue = frame.value;
                previousTime = frame.keyTimeMicroseconds;
                sampledValue = frame.value;
                continue;
            }
            const AnimationTime segmentDuration =
                frame.keyTimeMicroseconds >= previousTime
                ? frame.keyTimeMicroseconds - previousTime
                : 0U;
            double segmentProgress = segmentDuration == 0U
                ? 1.0
                : static_cast<double>(sampleTime - previousTime) /
                    static_cast<double>(segmentDuration);
            switch (frame.interpolation) {
            case DoubleKeyFrameInterpolation::Discrete:
                segmentProgress = sampleTime >=
                    frame.keyTimeMicroseconds ? 1.0 : 0.0;
                break;
            case DoubleKeyFrameInterpolation::Easing:
                segmentProgress = Ease(segmentProgress, frame.easing);
                break;
            case DoubleKeyFrameInterpolation::Spline:
                segmentProgress =
                    EvaluateSpline(segmentProgress, frame);
                break;
            case DoubleKeyFrameInterpolation::Linear:
                segmentProgress = Clamp01(segmentProgress);
                break;
            }
            sampledValue = previousValue +
                (frame.value - previousValue) * segmentProgress;
            found = true;
            break;
        }
        if (!found && !track.doubleFrames.Empty()) {
            sampledValue = track.doubleFrames.Back().value;
        }
        value = Meta::ValueCodec<double>::Encode(sampledValue).Value();
    } else if (track.kind == Track::Kind::ColorKeyFrames) {
        Base::Color previousValue = track.fromColor;
        AnimationTime previousTime = 0U;
        Base::Color sampledValue = previousValue;
        bool found = false;
        for (std::uint32_t index = 0U;
             index < track.colorFrames.Size(); ++index) {
            const ColorKeyFrame& frame =
                track.colorFrames[index];
            if (sampleTime > frame.keyTimeMicroseconds) {
                previousValue = frame.value;
                previousTime = frame.keyTimeMicroseconds;
                sampledValue = frame.value;
                continue;
            }
            const AnimationTime segmentDuration =
                frame.keyTimeMicroseconds >= previousTime
                ? frame.keyTimeMicroseconds - previousTime
                : 0U;
            double segmentProgress = segmentDuration == 0U
                ? 1.0
                : static_cast<double>(
                      sampleTime - previousTime) /
                    static_cast<double>(segmentDuration);
            switch (frame.interpolation) {
            case DoubleKeyFrameInterpolation::Discrete:
                segmentProgress = sampleTime >=
                    frame.keyTimeMicroseconds ? 1.0 : 0.0;
                break;
            case DoubleKeyFrameInterpolation::Easing:
                segmentProgress =
                    Ease(segmentProgress, frame.easing);
                break;
            case DoubleKeyFrameInterpolation::Spline:
                segmentProgress =
                    EvaluateSpline(segmentProgress, frame);
                break;
            case DoubleKeyFrameInterpolation::Linear:
                segmentProgress =
                    Clamp01(segmentProgress);
                break;
            }
            const float amount =
                static_cast<float>(segmentProgress);
            sampledValue = {
                previousValue.red +
                    (frame.value.red -
                     previousValue.red) * amount,
                previousValue.green +
                    (frame.value.green -
                     previousValue.green) * amount,
                previousValue.blue +
                    (frame.value.blue -
                     previousValue.blue) * amount,
                previousValue.alpha +
                    (frame.value.alpha -
                     previousValue.alpha) * amount};
            found = true;
            break;
        }
        if (!found && !track.colorFrames.Empty()) {
            sampledValue = track.colorFrames.Back().value;
        }
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Color>::Encode(
                sampledValue);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else {
        value = track.discreteBaseValue;
        for (const DiscreteAnimationKeyFrame& frame :
             track.discreteFrames) {
            if (sampleTime < frame.keyTimeMicroseconds) break;
            value = frame.value;
        }
    }

    const Meta::DependencyProperty* targetProperty =
        track.target->PropertyRegistry().Find(
            track.property);
    if (targetProperty != nullptr &&
        targetProperty->ValueType() ==
            Brush::StaticTypeId() &&
        value.Type() == Meta::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            Meta::ValueCodec<Base::Color>::Decode(
                value);
        if (!color) return color.GetStatus();
        Base::Result<Base::Ref<Brush>> brush =
            MakeSolidColorBrush(color.Value());
        if (!brush) return brush.GetStatus();
        value = Meta::PropertyValue::FromObject(
            Brush::StaticTypeId(),
            Base::Ref<Base::Object>(
                std::move(brush).Value()));
    }
    if (targetProperty != nullptr &&
        targetProperty->ValueType() ==
            Meta::TypeOf<Length>() &&
        value.Type() == Meta::TypeOf<double>()) {
        Base::Result<double> numeric =
            Meta::ValueCodec<double>::Decode(value);
        if (!numeric) return numeric.GetStatus();
        Base::Result<Meta::PropertyValue> length =
            Meta::ValueCodec<Length>::Encode(
                Length::Pixels(numeric.Value()));
        if (!length) return length.GetStatus();
        value = std::move(length).Value();
    }
    Base::Result<void> applied = values_->SetAnimationValue(
        *track.target, track.property, value);
    if (!applied) return applied.GetStatus();
    track.valueApplied = true;
    ++diagnostics_.appliedValueCount;

    if (completed) {
        if (!track.completedCounted) {
            ++diagnostics_.completedCount;
            track.completedCounted = true;
        }
        if (track.timing.fillBehavior == FillBehavior::Stop) {
            Base::Result<void> cleared = ClearTrackValue(track);
            if (!cleared) return cleared.GetStatus();
            track.state = AnimationState::Stopped;
        } else {
            track.state = AnimationState::Filling;
        }
    }
    return true;
}

Base::Result<std::uint32_t> AnimationEngine::Tick(
    AnimationTime nowMicroseconds) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!frameHook_.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "AnimationEngine is not initialized");
    }
    if (ticking_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AnimationEngine tick is already active");
    }
    if (nowMicroseconds > currentTimeMicroseconds_) {
        currentTimeMicroseconds_ = nowMicroseconds;
    }
    ticking_ = true;
    ++diagnostics_.tickSequence;
    diagnostics_.appliedValueCount = 0U;
    std::uint32_t appliedCount = 0U;
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        Base::Result<bool> applied =
            ApplyTrack(tracks_[index], currentTimeMicroseconds_);
        if (!applied) {
            ticking_ = false;
            lastTickStatus_ = applied.GetStatus();
            return applied.GetStatus();
        }
        if (applied.Value()) ++appliedCount;
    }
    if (appliedCount != 0U) {
        Base::Result<std::uint32_t> flushed = values_->Flush();
        if (!flushed) {
            ticking_ = false;
            lastTickStatus_ = flushed.GetStatus();
            return flushed.GetStatus();
        }
    }
    CompactStopped();
    ticking_ = false;
    lastTickStatus_ = Base::Status::Ok();
    return appliedCount;
}

Base::Result<std::uint32_t>
AnimationEngine::ApplyPendingInitialValues() noexcept {
    if (!automaticTickingEnabled_) {
        return 0U;
    }
    bool hasPending = false;
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        if (tracks_[index].pendingInitialSample) {
            hasPending = true;
            break;
        }
    }
    return hasPending ? Tick(currentTimeMicroseconds_) : 0U;
}

void AnimationEngine::CommitPendingInitialValues() noexcept {
    if (!automaticTickingEnabled_ || dispatcher_ == nullptr) {
        return;
    }
    const AnimationTime presentedAt = dispatcher_->NowMicroseconds();
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        Track& track = tracks_[index];
        if (!track.pendingInitialSample) {
            continue;
        }
        track.startTimeMicroseconds = presentedAt;
        track.pendingInitialSample = false;
    }
}

Base::Result<std::uint32_t> AnimationEngine::AdvanceBy(
    AnimationTime elapsedMicroseconds) noexcept {
    if (UINT64_MAX - currentTimeMicroseconds_ < elapsedMicroseconds) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Animation manual clock overflow");
    }
    return Tick(currentTimeMicroseconds_ + elapsedMicroseconds);
}

AnimationState AnimationEngine::State(
    AnimationHandle handle) const noexcept {
    const Track* track = FindTrack(handle);
    return track != nullptr
        ? track->state
        : AnimationState::Stopped;
}

AnimationDiagnostics AnimationEngine::Diagnostics() const noexcept {
    AnimationDiagnostics result = diagnostics_;
    result.activeCount = 0U;
    result.pausedCount = 0U;
    result.fillingCount = 0U;
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        switch (tracks_[index].state) {
        case AnimationState::Active:
            ++result.activeCount;
            break;
        case AnimationState::Paused:
            ++result.pausedCount;
            break;
        case AnimationState::Filling:
            ++result.fillingCount;
            break;
        case AnimationState::Stopped:
            break;
        }
    }
    return result;
}

void AnimationEngine::CompactStopped() noexcept {
    std::uint32_t destination = 0U;
    for (std::uint32_t source = 0U;
         source < trackCount_; ++source) {
        if (tracks_[source].state == AnimationState::Stopped) {
            tracks_[source].~Track();
            continue;
        }
        if (destination != source) {
            new (tracks_ + destination)
                Track(std::move(tracks_[source]));
            tracks_[source].~Track();
        }
        ++destination;
    }
    trackCount_ = destination;
}

void AnimationEngine::ReleaseTracks() noexcept {
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        tracks_[index].~Track();
    }
    if (tracks_ != nullptr) {
        allocator_->Deallocate(
            tracks_,
            sizeof(Track) * trackCapacity_,
            alignof(Track),
            Base::MemoryTag::Ui);
    }
    tracks_ = nullptr;
    trackCount_ = 0U;
    trackCapacity_ = 0U;
}

void AnimationEngine::AnimationFrameHook(void* context) noexcept {
    auto* manager = static_cast<AnimationEngine*>(context);
    if (manager == nullptr || manager->dispatcher_ == nullptr ||
        !manager->automaticTickingEnabled_) {
        return;
    }
    Base::Result<std::uint32_t> ticked =
        manager->Tick(manager->dispatcher_->NowMicroseconds());
    if (!ticked) manager->lastTickStatus_ = ticked.GetStatus();
}

} // namespace Aero::Media
