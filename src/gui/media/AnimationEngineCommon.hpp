#pragma once

// Shared internals for AnimationEngine translation units (Track + helpers).

#include "gui/media/AnimationEngine.hpp"
#include "gui/media/AnimationModel.hpp"

#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Value.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <cstdint>
#include <utility>

namespace Aero::Media::Animation::EngineSupport {

inline constexpr double Pi = 3.1415926535897932384626433832795;

inline Base::Status InvalidAnimation(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

inline double Clamp01(double value) noexcept {
    return std::max(0.0, std::min(1.0, value));
}

inline Base::Ref<Media::Animation::EasingFunctionBase> MakeEasingFunction(
    const ::Aero::Media::Animation::Model::EasingFunction& easing) noexcept {
    if (easing.kind == ::Aero::Media::Animation::Model::EasingFunctionKind::Linear) return {};
    auto kind = static_cast<Media::Animation::EasingFunctionBase::Kind>(static_cast<std::uint8_t>(easing.kind));
    auto ease = Base::MakeRef<Media::Animation::EasingFunctionBase>(
        Media::Animation::EasingFunctionBase::StaticTypeId(), kind);
    ease->SetEasingMode(easing.mode);
    ease->SetPower(easing.power);
    ease->SetExponent(easing.exponent);
    ease->SetAmplitude(easing.amplitude);
    ease->SetOscillations(easing.oscillations);
    ease->SetSpringiness(easing.springiness);
    return ease;
}

inline bool IsTimingValid(
    const ::Aero::Media::Animation::Model::TimelineTiming& timing) noexcept {
    return std::isfinite(timing.speedRatio) &&
        timing.speedRatio > 0.0 &&
        (timing.repeat.forever ||
            (std::isfinite(timing.repeat.count) &&
             timing.repeat.count > 0.0));
}

} // namespace Aero::Media::Animation::EngineSupport

namespace Aero {

struct AnimationEngine::Track {
    explicit Track(Base::IAllocator* = nullptr) noexcept {}

    Track(Track&&) noexcept = default;
    Track& operator=(Track&&) noexcept = default;
    Track(const Track&) = delete;
    Track& operator=(const Track&) = delete;

    AnimationHandle handle;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;
    TimelineTiming timing;
    AnimationState state = AnimationState::Active;
    AnimationTime startTimeMicroseconds = 0U;
    AnimationTime pauseTimeMicroseconds = 0U;
    AnimationTime seekOffsetMicroseconds = 0U;
    AnimationTime accumulatedPauseMicroseconds = 0U;
    Base::Ref<::Aero::Media::Animation::AnimationTimeline> timeline;
    Meta::PropertyValue originValue;
    Meta::PropertyValue destinationValue;
    bool valueApplied = false;
    bool completedCounted = false;
    bool pendingInitialSample = true;
};

} // namespace Aero
