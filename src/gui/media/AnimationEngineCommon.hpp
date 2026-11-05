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

inline double EvaluateEasing(
    Model::EasingFunctionKind kind,
    Media::Animation::EasingMode mode,
    double progress,
    double power = 2.0,
    double exponent = 2.0,
    double amplitude = 1.0,
    double bounces = 3.0,
    double bounciness = 3.0,
    double oscillations = 3.0,
    double springiness = 3.0) noexcept {
    const double value = progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
    const auto easeIn = [kind, power, exponent, amplitude, bounces, bounciness, oscillations, springiness](double input) noexcept {
        switch (kind) {
        case Model::EasingFunctionKind::Linear:
            return input;
        case Model::EasingFunctionKind::Sine:
            return 1.0 - std::cos(input * Pi * 0.5);
        case Model::EasingFunctionKind::Quadratic:
            return input * input;
        case Model::EasingFunctionKind::Cubic:
            return input * input * input;
        case Model::EasingFunctionKind::Quartic:
            return input * input * input * input;
        case Model::EasingFunctionKind::Quintic:
            return input * input * input * input * input;
        case Model::EasingFunctionKind::Circle:
            return 1.0 - std::sqrt(std::max(0.0, 1.0 - input * input));
        case Model::EasingFunctionKind::Power:
            return std::pow(input, std::max(0.0, power));
        case Model::EasingFunctionKind::Exponential: {
            const double expVal = std::max(0.0, exponent);
            return input <= 0.0
                ? 0.0
                : (std::exp(expVal * input) - 1.0) / std::max(1.0e-9, std::exp(expVal) - 1.0);
        }
        case Model::EasingFunctionKind::Back: {
            const double amp = std::max(0.0, amplitude);
            return input * input * ((amp + 1.0) * input - amp);
        }
        case Model::EasingFunctionKind::Bounce: {
            if (input <= 0.0) return 0.0;
            if (input >= 1.0) return 1.0;
            const double bCount = std::max(1.0, std::floor(bounces));
            double bElasticity = bounciness;
            if (bElasticity <= 1.0 || !std::isfinite(bElasticity)) {
                bElasticity = 1.001;
            }
            const double powB = std::pow(bElasticity, bCount);
            const double oneMinusB = 1.0 - bElasticity;
            const double sum = (1.0 - powB) / oneMinusB + powB * 0.5;
            const double t = 1.0 - input;
            const double unitAtT = std::floor(
                std::log(-t * (1.0 - bElasticity) * sum + 1.0) / std::log(bElasticity));
            const double start = (1.0 - std::pow(bElasticity, unitAtT)) / (oneMinusB * sum);
            const double end = (1.0 - std::pow(bElasticity, unitAtT + 1.0)) / (oneMinusB * sum);
            const double mid = (start + end) * 0.5;
            const double radius = end - mid;
            const double dt = t - mid;
            const double amp = std::pow(1.0 / bElasticity, bCount - unitAtT);
            const double easeOut = 1.0 - ((-amp / (radius * radius)) * (radius - dt) * (radius + dt));
            return 1.0 - easeOut;
        }
        case Model::EasingFunctionKind::Elastic: {
            if (input <= 0.0 || input >= 1.0) return input;
            const double osc = std::max(1.0, oscillations);
            const double spr = std::max(0.0, springiness);
            const double envelope = spr == 0.0
                ? input
                : (std::exp(spr * input) - 1.0) / (std::exp(spr) - 1.0);
            return envelope * std::sin((input * osc - 0.25) * 2.0 * Pi);
        }
        }
        return input;
    };
    switch (mode) {
    case Media::Animation::EasingMode::EaseIn:
        return easeIn(value);
    case Media::Animation::EasingMode::EaseOut:
        return 1.0 - easeIn(1.0 - value);
    case Media::Animation::EasingMode::EaseInOut:
        return value < 0.5
            ? easeIn(value * 2.0) * 0.5
            : 1.0 - easeIn((1.0 - value) * 2.0) * 0.5;
    }
    return value;
}

inline double EvaluateEasing(
    double progress,
    const Model::EasingFunction& easing) noexcept {
    return EvaluateEasing(
        easing.kind,
        easing.mode,
        progress,
        easing.power,
        easing.exponent,
        easing.amplitude,
        3.0,
        3.0,
        easing.oscillations,
        easing.springiness);
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
