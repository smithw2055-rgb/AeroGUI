#include "gui/media/AnimationEngineCommon.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include <Aero/Layout.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Value.hpp>

namespace Aero {

using namespace Aero::Media::Animation::EngineSupport;

Base::Result<AnimationHandle> AnimationEngine::BeginTimeline(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const Media::Animation::Timeline& timeline,
    const TimelineTiming& timing,
    Meta::Registry* metadata) noexcept {
    static_cast<void>(metadata);
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!property.IsValid() || !IsTimingValid(timing)) {
        return InvalidAnimation("Timeline has invalid target, property, or timing");
    }

    const Meta::TypeId type = timeline.RuntimeType();
    const bool isAnimation = (type == Media::Animation::AnimationTimeline::StaticTypeId()) ||
        IsRuntimeTypeDerivedFrom(type, Media::Animation::AnimationTimeline::StaticTypeId()) ||
        (metadata != nullptr && metadata->Types().IsDerivedFrom(type, Media::Animation::AnimationTimeline::StaticTypeId()));
    if (!isAnimation) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Storyboard contains an unsupported Timeline type");
    }

    const auto* anim = static_cast<const Media::Animation::AnimationTimeline*>(&timeline);

    Base::Result<Track*> added = AddTrack(&target, property);
    if (!added) return added.GetStatus();

    Track& track = *added.Value();
    track.handle = {nextHandle_++};
    track.target = &target;
    track.property = property;
    track.timing = timing;
    if (track.timing.durationMicroseconds == 0U) {
        track.timing.durationMicroseconds = anim->GetEffectiveDurationMicroseconds();
    }
    track.startTimeMicroseconds = currentTimeMicroseconds_;
    track.pendingInitialSample = true;
    track.state = AnimationState::Active;

    // Resolve base / origin value
    Base::Result<Meta::PropertyValue> base = target.GetAnimationBaseValueInternal(property);
    if (!base || (*base).IsUnset()) {
        base = target.GetValue(property);
    }
    if (base && !(*base).IsUnset()) {
        if ((*base).Type() == Meta::TypeOf<Length>()) {
            if (auto len = Meta::ValueCodec<Length>::Decode(*base)) {
                track.originValue = Meta::ValueCodec<double>::Encode((*len).isAuto ? 0.0 : (*len).value);
            } else {
                track.originValue = *base;
            }
        } else {
            track.originValue = *base;
        }
    }

    // Retain timeline (if borrowed, TryFromBorrowed or Clone)
    auto* mutableAnim = const_cast<Media::Animation::AnimationTimeline*>(anim);
    auto ref = Base::Ref<Media::Animation::AnimationTimeline>::TryFromBorrowed(*mutableAnim);
    if (!ref) {
        auto cloned = anim->Clone();
        if (cloned) {
            ref = Base::Ref<Media::Animation::AnimationTimeline>::TryFromBorrowed(
                *static_cast<Media::Animation::AnimationTimeline*>(cloned.Get()));
        }
    }
    track.timeline = std::move(ref);

    return track.handle;
}

} // namespace Aero
