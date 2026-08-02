#pragma once

#include "gui/ElementInternal.hpp"
#include "media/AnimationModel.hpp"

namespace Aero::Internal {

using namespace Aero::Internal::Animation;

class AERO_API AnimationEngine final {
public:
    AnimationEngine(
        ::Aero::Threading::Dispatcher& dispatcher,
        Meta::EffectiveValueEngine& values,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~AnimationEngine() noexcept;

    AnimationEngine(const AnimationEngine&) = delete;
    AnimationEngine& operator=(const AnimationEngine&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    Base::Result<AnimationHandle> Begin(
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        const DoubleAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        const ColorAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        const PointAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        const RectAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        const ThicknessAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        const DoubleKeyFrameAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        const ColorKeyFrameAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        const DiscreteAnimation& animation) noexcept;

    Base::Result<void> Pause(AnimationHandle handle) noexcept;
    Base::Result<void> Resume(AnimationHandle handle) noexcept;
    Base::Result<void> Seek(
        AnimationHandle handle,
        AnimationTime offsetMicroseconds) noexcept;
    Base::Result<void> Stop(AnimationHandle handle) noexcept;
    Base::Result<void> Remove(AnimationHandle handle) noexcept;
    Base::Result<std::uint32_t> RemoveTarget(
        ::Aero::DependencyObject& target) noexcept;
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

    ::Aero::Threading::Dispatcher* dispatcher_ = nullptr;
    Meta::EffectiveValueEngine* values_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Track* tracks_ = nullptr;
    std::uint32_t trackCount_ = 0U;
    std::uint32_t trackCapacity_ = 0U;
    ::Aero::Threading::DispatcherFrameHookHandle frameHook_;
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

} // namespace Aero::Internal
