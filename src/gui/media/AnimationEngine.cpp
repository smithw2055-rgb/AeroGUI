#include "gui/media/AnimationEngineInternal.hpp"

#include <Aero/Media/Brushes.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Transforms.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Value.hpp>
#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <utility>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Media::Animation::Model;
using namespace Aero::Media;
using namespace Aero::Media::Animation::EngineDetail;

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
    if (initialized_) return {};
    // P3.2: ViewFrame drives AnimationFrameHook() directly; no hook.
    initialized_ = true;
    currentTimeMicroseconds_ = dispatcher_->NowMicroseconds();
    lastTickStatus_ = Base::Status::Ok();
    return {};
}

void AnimationEngine::Shutdown() noexcept {
    if (dispatcher_ != nullptr && dispatcher_->CheckAccess()) {
        static_cast<void>(RemoveAll());
    }
    initialized_ = false;
    ReleaseTracks();
}

Base::Result<AnimationEngine::Track*>
AnimationEngine::AddTrack(
    ::Aero::DependencyObject* target,
    Meta::DependencyPropertyHandle property) noexcept {
    // SnapshotAndReplace: a new clock on the same property must retire the
    // previous HoldEnd fill (QuestLog Close → Intro). Leaving the filling
    // Close clock active keeps Opacity at 0 and the window rendering.
    if (target != nullptr && property.IsValid()) {
        for (std::uint32_t index = 0U; index < trackCount_; ++index) {
            Track& existing = tracks_[index];
            if (existing.target != target ||
                existing.property != property ||
                existing.state == AnimationState::Stopped) {
                continue;
            }
            existing.state = AnimationState::Stopped;
            static_cast<void>(ClearTrackValue(existing));
        }
    }
    if (!initialized_) {
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
    Base::Result<Track*> added = AddTrack(&target, property);
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
    Base::Result<Track*> added = AddTrack(&target, property);
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
    Base::Result<Track*> added = AddTrack(&target, property);
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
    Base::Result<Track*> added = AddTrack(&target, property);
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
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const IntegerAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(animation.timing)) {
        return InvalidAnimation(
            "Integer animation has invalid target or timing");
    }
    Base::Result<Track*> added = AddTrack(&target, property);
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.easing = animation.easing;
    track.kind = Track::Kind::Integer;
    track.fromInteger = animation.from;
    track.toInteger = animation.to;
    track.integerWidth = animation.width;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const SizeAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    const auto finite = [](Base::Size value) noexcept {
        return std::isfinite(value.width) && std::isfinite(value.height);
    };
    if (!property.IsValid() ||
        !IsTimingValid(animation.timing) ||
        !finite(animation.from) ||
        !finite(animation.to)) {
        return InvalidAnimation(
            "SizeAnimation has invalid target, timing, or values");
    }
    Base::Result<Track*> added = AddTrack(&target, property);
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.easing = animation.easing;
    track.kind = Track::Kind::Size;
    track.fromSize = animation.from;
    track.toSize = animation.to;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const MatrixAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() ||
        !IsTimingValid(animation.timing) ||
        !Base::IsFiniteTransform(animation.from) ||
        !Base::IsFiniteTransform(animation.to)) {
        return InvalidAnimation(
            "MatrixAnimation has invalid target, timing, or values");
    }
    Base::Result<Track*> added = AddTrack(&target, property);
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.easing = animation.easing;
    track.kind = Track::Kind::Matrix;
    track.fromMatrix = animation.from;
    track.toMatrix = animation.to;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
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
    Base::Result<Track*> added = AddTrack(&target, property);
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
    Base::Result<Track*> added = AddTrack(&target, property);
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
    Base::Result<Track*> added = AddTrack(&target, property);
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
    Base::Result<Track*> added = AddTrack(&target, property);
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
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const PointKeyFrameAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(animation.timing) ||
        !std::isfinite(animation.baseValue.x) ||
        !std::isfinite(animation.baseValue.y) ||
        animation.keyFrames.Empty()) {
        return InvalidAnimation(
            "Point key-frame animation is incomplete");
    }
    AnimationTime lastKeyTime = 0U;
    for (std::uint32_t index = 0U;
         index < animation.keyFrames.Size(); ++index) {
        const PointKeyFrame& frame = animation.keyFrames[index];
        if (!std::isfinite(frame.value.x) ||
            !std::isfinite(frame.value.y) ||
            (index != 0U && frame.keyTimeMicroseconds < lastKeyTime)) {
            return InvalidAnimation(
                "Point key frames must be finite and ordered");
        }
        lastKeyTime = frame.keyTimeMicroseconds;
    }
    Base::Result<Track*> added = AddTrack(&target, property);
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    Base::Result<void> copied =
        track.pointFrames.Append(animation.keyFrames);
    if (!copied) {
        track.state = AnimationState::Stopped;
        CompactStopped();
        return copied.GetStatus();
    }
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.kind = Track::Kind::PointKeyFrames;
    track.fromPoint = animation.baseValue;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const ThicknessKeyFrameAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(animation.timing) ||
        !std::isfinite(animation.baseValue.left) ||
        !std::isfinite(animation.baseValue.top) ||
        !std::isfinite(animation.baseValue.right) ||
        !std::isfinite(animation.baseValue.bottom) ||
        animation.keyFrames.Empty()) {
        return InvalidAnimation(
            "Thickness key-frame animation is incomplete");
    }
    AnimationTime lastKeyTime = 0U;
    for (std::uint32_t index = 0U;
         index < animation.keyFrames.Size(); ++index) {
        const ThicknessKeyFrame& frame = animation.keyFrames[index];
        if (!std::isfinite(frame.value.left) ||
            !std::isfinite(frame.value.top) ||
            !std::isfinite(frame.value.right) ||
            !std::isfinite(frame.value.bottom) ||
            (index != 0U && frame.keyTimeMicroseconds < lastKeyTime)) {
            return InvalidAnimation(
                "Thickness key frames must be finite and ordered");
        }
        lastKeyTime = frame.keyTimeMicroseconds;
    }
    Base::Result<Track*> added = AddTrack(&target, property);
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    Base::Result<void> copied =
        track.thicknessFrames.Append(animation.keyFrames);
    if (!copied) {
        track.state = AnimationState::Stopped;
        CompactStopped();
        return copied.GetStatus();
    }
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.kind = Track::Kind::ThicknessKeyFrames;
    track.fromThickness = animation.baseValue;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const IntegerKeyFrameAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() ||
        !IsTimingValid(animation.timing) ||
        animation.keyFrames.Empty()) {
        return InvalidAnimation(
            "Integer key-frame animation is incomplete");
    }
    AnimationTime lastKeyTime = 0U;
    for (std::uint32_t index = 0U;
         index < animation.keyFrames.Size(); ++index) {
        const IntegerKeyFrame& frame = animation.keyFrames[index];
        if (index != 0U && frame.keyTimeMicroseconds < lastKeyTime) {
            return InvalidAnimation(
                "Integer key frames must be ordered");
        }
        lastKeyTime = frame.keyTimeMicroseconds;
    }
    Base::Result<Track*> added = AddTrack(&target, property);
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    Base::Result<void> copied =
        track.integerFrames.Append(animation.keyFrames);
    if (!copied) {
        track.state = AnimationState::Stopped;
        CompactStopped();
        return copied.GetStatus();
    }
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.kind = Track::Kind::IntegerKeyFrames;
    track.fromInteger = animation.baseValue;
    track.integerWidth = animation.width;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const SizeKeyFrameAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(animation.timing) ||
        !std::isfinite(animation.baseValue.width) ||
        !std::isfinite(animation.baseValue.height) ||
        animation.keyFrames.Empty()) {
        return InvalidAnimation(
            "Size key-frame animation is incomplete");
    }
    AnimationTime lastKeyTime = 0U;
    for (std::uint32_t index = 0U;
         index < animation.keyFrames.Size(); ++index) {
        const SizeKeyFrame& frame = animation.keyFrames[index];
        if (!std::isfinite(frame.value.width) ||
            !std::isfinite(frame.value.height) ||
            (index != 0U && frame.keyTimeMicroseconds < lastKeyTime)) {
            return InvalidAnimation(
                "Size key frames must be finite and ordered");
        }
        lastKeyTime = frame.keyTimeMicroseconds;
    }
    Base::Result<Track*> added = AddTrack(&target, property);
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    Base::Result<void> copied =
        track.sizeFrames.Append(animation.keyFrames);
    if (!copied) {
        track.state = AnimationState::Stopped;
        CompactStopped();
        return copied.GetStatus();
    }
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.kind = Track::Kind::SizeKeyFrames;
    track.fromSize = animation.baseValue;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    return track.handle;
}

Base::Result<AnimationHandle> AnimationEngine::Begin(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const MatrixKeyFrameAnimation& animation) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(animation.timing) ||
        !Base::IsFiniteTransform(animation.baseValue) ||
        animation.keyFrames.Empty()) {
        return InvalidAnimation(
            "Matrix key-frame animation is incomplete");
    }
    AnimationTime lastKeyTime = 0U;
    for (std::uint32_t index = 0U;
         index < animation.keyFrames.Size(); ++index) {
        const MatrixKeyFrame& frame = animation.keyFrames[index];
        if (!Base::IsFiniteTransform(frame.value) ||
            (index != 0U && frame.keyTimeMicroseconds < lastKeyTime)) {
            return InvalidAnimation(
                "Matrix key frames must be finite and ordered");
        }
        lastKeyTime = frame.keyTimeMicroseconds;
    }
    Base::Result<Track*> added = AddTrack(&target, property);
    if (!added) return added.GetStatus();
    Track& track = *added.Value();
    Base::Result<void> copied =
        track.matrixFrames.Append(animation.keyFrames);
    if (!copied) {
        track.state = AnimationState::Stopped;
        CompactStopped();
        return copied.GetStatus();
    }
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = animation.timing;
    track.kind = Track::Kind::MatrixKeyFrames;
    track.fromMatrix = animation.baseValue;
    track.startTimeMicroseconds = currentTimeMicroseconds_;
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
    Base::Result<Track*> added = AddTrack(&target, property);
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
    if (track.target == nullptr) return {};
    if (track.valueApplied) {
        Base::Result<void> cleared = values_->ClearAnimationValue(
            *track.target, track.property);
        if (!cleared) return cleared.GetStatus();
        track.valueApplied = false;
    }
    // HoldEnd clocks leave the last keyframe on the animation layer. Removing
    // the clock must restore the pre-storyboard value (Menu3D CircledArrow
    // Fill.Opacity 0) even when that rest value is not a Local DP.
    if (track.kind == Track::Kind::DoubleKeyFrames) {
        Base::Result<PropertyValue> rest =
            Meta::ValueCodec<double>::Encode(track.baseValue);
        if (!rest) return rest.GetStatus();
        Base::Result<void> restored = values_->SetAnimationValue(
            *track.target, track.property, rest.Value());
        if (!restored) return restored.GetStatus();
        track.valueApplied = true;
    } else if (track.kind == Track::Kind::ColorKeyFrames) {
        Base::Result<PropertyValue> rest =
            Meta::ValueCodec<Base::Color>::Encode(track.fromColor);
        if (!rest) return rest.GetStatus();
        Base::Result<void> restored = values_->SetAnimationValue(
            *track.target, track.property, rest.Value());
        if (!restored) return restored.GetStatus();
        track.valueApplied = true;
    }
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
    if (values_->IsFlushing()) {
        return {};
    }
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


Base::Result<std::uint32_t> AnimationEngine::Tick(
    AnimationTime nowMicroseconds) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!initialized_) {
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
    Base::Status firstTrackError{};
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        Base::Result<bool> applied =
            ApplyTrack(tracks_[index], currentTimeMicroseconds_);
        if (!applied) {
            if (firstTrackError.IsOk()) {
                firstTrackError = applied.GetStatus();
            }
            continue;
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
    lastTickStatus_ = firstTrackError;
    return appliedCount;
}

Base::Result<std::uint32_t>
AnimationEngine::ApplyPendingInitialValues() noexcept {
    bool hasPending = false;
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        if (tracks_[index].pendingInitialSample) {
            hasPending = true;
            break;
        }
    }
    if (!hasPending) {
        return 0U;
    }
    Base::Result<std::uint32_t> sampled = Tick(currentTimeMicroseconds_);
    if (!sampled) return sampled.GetStatus();
    // Manual clocks (DesktopHost) own subsequent AdvanceBy steps. Clear the
    // t=0 latch here so later ticks do not stay frozen at the first keyframe.
    // Automatic clocks keep the latch until CommitPendingInitialValues so the
    // wall clock starts at the first presented frame rather than construction.
    if (!automaticTickingEnabled_) {
        for (std::uint32_t index = 0U; index < trackCount_; ++index) {
            tracks_[index].pendingInitialSample = false;
        }
    }
    return sampled;
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

} // namespace Aero
