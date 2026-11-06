#include "gui/media/AnimationEngineCommon.hpp"

#include <Aero/Media/Brushes.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Value.hpp>
#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <utility>
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Media::Animation::Model;
using namespace Aero::Media;
using namespace Aero::Media::Animation::EngineSupport;

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
        bool clearedAny = false;
        for (std::uint32_t index = 0U; index < trackCount_; ++index) {
            Track& existing = tracks_[index];
            if (existing.target != target ||
                existing.property != property ||
                existing.state == AnimationState::Stopped) {
                continue;
            }
            existing.state = AnimationState::Stopped;
            clearedAny = clearedAny || existing.valueApplied;
            ClearTrackValue(existing);
        }
        if (clearedAny && !values_->IsFlushing()) {
            static_cast<void>(values_->Flush());
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

bool AnimationEngine::Pause(
    AnimationHandle handle) noexcept {
    if (dispatcher_ != nullptr && !dispatcher_->CheckAccess()) return false;
    Track* track = FindTrack(handle);
    if (track == nullptr) {
        return false;
    }
    if (track->state == AnimationState::Active) {
        track->state = AnimationState::Paused;
        track->pauseTimeMicroseconds = currentTimeMicroseconds_;
    }
    return true;
}

bool AnimationEngine::Resume(
    AnimationHandle handle) noexcept {
    if (dispatcher_ != nullptr && !dispatcher_->CheckAccess()) return false;
    Track* track = FindTrack(handle);
    if (track == nullptr) {
        return false;
    }
    if (track->state == AnimationState::Paused) {
        const AnimationTime now = currentTimeMicroseconds_;
        if (now >= track->pauseTimeMicroseconds) {
            track->accumulatedPauseMicroseconds +=
                now - track->pauseTimeMicroseconds;
        }
        track->state = AnimationState::Active;
    }
    return true;
}

bool AnimationEngine::Seek(
    AnimationHandle handle,
    AnimationTime offsetMicroseconds) noexcept {
    if (dispatcher_ != nullptr && !dispatcher_->CheckAccess()) return false;
    Track* track = FindTrack(handle);
    if (track == nullptr) {
        return false;
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
    return true;
}

void AnimationEngine::ClearTrackValue(
    Track& track) noexcept {
    if (track.target == nullptr) return;
    if (track.valueApplied) {
        static_cast<void>(values_->ClearAnimationValue(
            *track.target, track.property));
        track.valueApplied = false;
    }
}

bool AnimationEngine::Stop(
    AnimationHandle handle) noexcept {
    if (dispatcher_ != nullptr && !dispatcher_->CheckAccess()) return false;
    Track* track = FindTrack(handle);
    if (track == nullptr) {
        return false;
    }
    ClearTrackValue(*track);
    track->state = AnimationState::Stopped;
    track->target = nullptr;
    if (!values_->IsFlushing()) {
        static_cast<void>(values_->Flush());
    }
    return true;
}

bool AnimationEngine::Remove(
    AnimationHandle handle) noexcept {
    const bool stopped = Stop(handle);
    if (!stopped) return false;
    CompactStopped();
    return true;
}

std::uint32_t AnimationEngine::RemoveBatch(
    Base::Span<const AnimationHandle> handles) noexcept {
    if (handles.Empty() || trackCount_ == 0U) return 0U;
    if (dispatcher_ != nullptr && !dispatcher_->CheckAccess()) return 0U;
    std::uint32_t removed = 0U;
    bool clearedAnyValue = false;
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        Track& track = tracks_[index];
        if (track.state == AnimationState::Stopped) continue;
        bool match = false;
        for (AnimationHandle handle : handles) {
            if (track.handle == handle) {
                match = true;
                break;
            }
        }
        if (!match) continue;
        clearedAnyValue = clearedAnyValue || track.valueApplied;
        ClearTrackValue(track);
        track.state = AnimationState::Stopped;
        track.target = nullptr;
        ++removed;
    }
    if (removed != 0U) {
        CompactStopped();
        if (clearedAnyValue && !values_->IsFlushing()) {
            static_cast<void>(values_->Flush());
        }
    }
    return removed;
}

bool AnimationEngine::HasTarget(
    const ::Aero::DependencyObject& target) const noexcept {
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        if (tracks_[index].target == &target &&
            tracks_[index].state != AnimationState::Stopped) {
            return true;
        }
    }
    return false;
}

std::uint32_t AnimationEngine::RemoveTarget(
    ::Aero::DependencyObject& target) noexcept {
    if (trackCount_ == 0U) return 0U;
    if (dispatcher_ != nullptr && !dispatcher_->CheckAccess()) return 0U;
    std::uint32_t removed = 0U;
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        Track& track = tracks_[index];
        if (track.target != &target) continue;
        ClearTrackValue(track);
        track.state = AnimationState::Stopped;
        track.target = nullptr;
        ++removed;
    }
    if (removed != 0U) {
        CompactStopped();
        static_cast<void>(values_->Flush());
    }
    return removed;
}

void AnimationEngine::RemoveAll() noexcept {
    if (dispatcher_ == nullptr || !dispatcher_->CheckAccess()) {
        return;
    }
    bool changed = false;
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        const bool hadValue = tracks_[index].valueApplied;
        ClearTrackValue(tracks_[index]);
        changed = changed || hadValue;
        tracks_[index].state = AnimationState::Stopped;
    }
    CompactStopped();
    if (changed) {
        static_cast<void>(values_->Flush());
    }
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
    for (std::uint32_t index = 0U; index < trackCount_; ++index) {
        if (ApplyTrack(tracks_[index], currentTimeMicroseconds_)) {
            ++appliedCount;
        }
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
