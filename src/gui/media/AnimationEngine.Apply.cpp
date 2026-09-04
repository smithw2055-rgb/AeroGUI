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
    AnimationTime sampledNow = nowMicroseconds;
    if (track.pendingInitialSample) {
        // Automatic clocks keep t=0 until the first presented frame.
        // Manual clocks (View::Update / AdvanceBy) must sample elapsed time
        // on the same tick, otherwise a GeneratedDuration fade stays frozen
        // at `from` (Menu3D CircledArrow Opacity 1 after Unchecked).
        if (automaticTickingEnabled_ ||
            nowMicroseconds <= track.startTimeMicroseconds) {
            sampledNow = track.startTimeMicroseconds;
        } else {
            track.pendingInitialSample = false;
        }
    }
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

    // Key-frame animations without a zero-time key frame interpolate from
    // the property's current base value. That base can change while the
    // animation is active (for example an ElementName binding to ActualWidth
    // after an image finishes layout), even though the animation contribution
    // still wins the effective-value precedence.
    if (track.kind == Track::Kind::DoubleKeyFrames &&
        !track.doubleFrames.Empty() &&
        track.doubleFrames.Front().keyTimeMicroseconds != 0U) {
        Base::Result<Meta::PropertyValue> base =
            AeroGuiInternal::GetAnimationBaseValue(
                *track.target, track.property);
        if (!base) return base.GetStatus();
        Base::Result<double> decoded =
            Meta::ValueCodec<double>::Decode(base.Value());
        if (!decoded) return decoded.GetStatus();
        track.baseValue = decoded.Value();
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
    } else if (track.kind == Track::Kind::Integer) {
        const double eased = Ease(progress, track.easing);
        Base::Result<Meta::PropertyValue> encoded = EncodeIntegerWidth(
            LerpInteger(track.fromInteger, track.toInteger, eased),
            track.integerWidth);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::Size) {
        const double eased = Ease(progress, track.easing);
        const Base::Size size{
            track.fromSize.width +
                (track.toSize.width - track.fromSize.width) * eased,
            track.fromSize.height +
                (track.toSize.height - track.fromSize.height) * eased};
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Size>::Encode(size);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::Matrix) {
        const double eased = Ease(progress, track.easing);
        const Base::Transform2D matrix = LerpMatrix(
            track.fromMatrix, track.toMatrix, eased);
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Transform2D>::Encode(matrix);
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
    } else if (track.kind == Track::Kind::PointKeyFrames) {
        Base::Point previousValue = track.fromPoint;
        AnimationTime previousTime = 0U;
        Base::Point sampledValue = previousValue;
        bool found = false;
        for (std::uint32_t index = 0U;
             index < track.pointFrames.Size(); ++index) {
            const PointKeyFrame& frame = track.pointFrames[index];
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
                segmentProgress = EvaluateSpline(segmentProgress, frame);
                break;
            case DoubleKeyFrameInterpolation::Linear:
                segmentProgress = Clamp01(segmentProgress);
                break;
            }
            sampledValue = {
                previousValue.x +
                    (frame.value.x - previousValue.x) * segmentProgress,
                previousValue.y +
                    (frame.value.y - previousValue.y) * segmentProgress};
            found = true;
            break;
        }
        if (!found && !track.pointFrames.Empty()) {
            sampledValue = track.pointFrames.Back().value;
        }
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Point>::Encode(sampledValue);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::ThicknessKeyFrames) {
        Base::Thickness previousValue = track.fromThickness;
        AnimationTime previousTime = 0U;
        Base::Thickness sampledValue = previousValue;
        bool found = false;
        for (std::uint32_t index = 0U;
             index < track.thicknessFrames.Size(); ++index) {
            const ThicknessKeyFrame& frame = track.thicknessFrames[index];
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
                segmentProgress = EvaluateSpline(segmentProgress, frame);
                break;
            case DoubleKeyFrameInterpolation::Linear:
                segmentProgress = Clamp01(segmentProgress);
                break;
            }
            sampledValue = {
                previousValue.left +
                    (frame.value.left - previousValue.left) * segmentProgress,
                previousValue.top +
                    (frame.value.top - previousValue.top) * segmentProgress,
                previousValue.right +
                    (frame.value.right - previousValue.right) * segmentProgress,
                previousValue.bottom +
                    (frame.value.bottom - previousValue.bottom) *
                        segmentProgress};
            found = true;
            break;
        }
        if (!found && !track.thicknessFrames.Empty()) {
            sampledValue = track.thicknessFrames.Back().value;
        }
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Thickness>::Encode(sampledValue);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::IntegerKeyFrames) {
        std::int64_t previousValue = track.fromInteger;
        AnimationTime previousTime = 0U;
        std::int64_t sampledValue = previousValue;
        bool found = false;
        for (std::uint32_t index = 0U;
             index < track.integerFrames.Size(); ++index) {
            const IntegerKeyFrame& frame = track.integerFrames[index];
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
                segmentProgress = EvaluateSpline(segmentProgress, frame);
                break;
            case DoubleKeyFrameInterpolation::Linear:
                segmentProgress = Clamp01(segmentProgress);
                break;
            }
            sampledValue = LerpInteger(
                previousValue, frame.value, segmentProgress);
            found = true;
            break;
        }
        if (!found && !track.integerFrames.Empty()) {
            sampledValue = track.integerFrames.Back().value;
        }
        Base::Result<Meta::PropertyValue> encoded =
            EncodeIntegerWidth(sampledValue, track.integerWidth);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::SizeKeyFrames) {
        Base::Size previousValue = track.fromSize;
        AnimationTime previousTime = 0U;
        Base::Size sampledValue = previousValue;
        bool found = false;
        for (std::uint32_t index = 0U;
             index < track.sizeFrames.Size(); ++index) {
            const SizeKeyFrame& frame = track.sizeFrames[index];
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
                segmentProgress = EvaluateSpline(segmentProgress, frame);
                break;
            case DoubleKeyFrameInterpolation::Linear:
                segmentProgress = Clamp01(segmentProgress);
                break;
            }
            sampledValue = {
                previousValue.width +
                    (frame.value.width - previousValue.width) * segmentProgress,
                previousValue.height +
                    (frame.value.height - previousValue.height) *
                        segmentProgress};
            found = true;
            break;
        }
        if (!found && !track.sizeFrames.Empty()) {
            sampledValue = track.sizeFrames.Back().value;
        }
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Size>::Encode(sampledValue);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::MatrixKeyFrames) {
        Base::Transform2D previousValue = track.fromMatrix;
        AnimationTime previousTime = 0U;
        Base::Transform2D sampledValue = previousValue;
        bool found = false;
        for (std::uint32_t index = 0U;
             index < track.matrixFrames.Size(); ++index) {
            const MatrixKeyFrame& frame = track.matrixFrames[index];
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
                segmentProgress = EvaluateSpline(segmentProgress, frame);
                break;
            case DoubleKeyFrameInterpolation::Linear:
                segmentProgress = Clamp01(segmentProgress);
                break;
            }
            sampledValue = LerpMatrix(
                previousValue, frame.value, segmentProgress);
            found = true;
            break;
        }
        if (!found && !track.matrixFrames.Empty()) {
            sampledValue = track.matrixFrames.Back().value;
        }
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Transform2D>::Encode(sampledValue);
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

} // namespace Aero
