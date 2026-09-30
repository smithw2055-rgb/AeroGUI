#include "gui/media/AnimationEngineCommon.hpp"

#include <Aero/Media/Brushes.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Value.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/TryCast.hpp>

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
                std::max(0.0, easing.exponent);
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

template<class TFrame>
double SegmentProgress(
    const TFrame& frame,
    AnimationTime sampleTime,
    AnimationTime previousTime) noexcept {
    const AnimationTime segmentDuration =
        frame.keyTimeMicroseconds >= previousTime
        ? frame.keyTimeMicroseconds - previousTime
        : 0U;
    const double raw = segmentDuration == 0U
        ? 1.0
        : static_cast<double>(sampleTime - previousTime) /
            static_cast<double>(segmentDuration);
    switch (frame.interpolation) {
    case DoubleKeyFrameInterpolation::Discrete:
        return sampleTime >= frame.keyTimeMicroseconds ? 1.0 : 0.0;
    case DoubleKeyFrameInterpolation::Easing:
        return AnimationEngine::Ease(raw, frame.easing);
    case DoubleKeyFrameInterpolation::Spline:
        return EvaluateSpline(raw, frame);
    case DoubleKeyFrameInterpolation::Linear:
        return Clamp01(raw);
    }
    return Clamp01(raw);
}

inline Base::Color LerpColor(Base::Color from, Base::Color to, double amount) noexcept {
    const float t = static_cast<float>(amount);
    return {
        from.red + (to.red - from.red) * t,
        from.green + (to.green - from.green) * t,
        from.blue + (to.blue - from.blue) * t,
        from.alpha + (to.alpha - from.alpha) * t};
}

inline Base::Point LerpPoint(Base::Point from, Base::Point to, double amount) noexcept {
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount};
}

inline Base::Rect LerpRect(Base::Rect from, Base::Rect to, double amount) noexcept {
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
        from.width + (to.width - from.width) * amount,
        from.height + (to.height - from.height) * amount};
}

inline Base::Thickness LerpThickness(
    Base::Thickness from, Base::Thickness to, double amount) noexcept {
    return {
        from.left + (to.left - from.left) * amount,
        from.top + (to.top - from.top) * amount,
        from.right + (to.right - from.right) * amount,
        from.bottom + (to.bottom - from.bottom) * amount};
}

inline Base::Size LerpSize(Base::Size from, Base::Size to, double amount) noexcept {
    return {
        from.width + (to.width - from.width) * amount,
        from.height + (to.height - from.height) * amount};
}

template<class T, class TFrame, class Lerp>
T SampleKeyFrames(
    const Base::Vector<TFrame>& frames,
    T base,
    AnimationTime sampleTime,
    Lerp lerp) noexcept {
    T previous = base;
    AnimationTime previousTime = 0U;
    T sampled = previous;
    bool found = false;
    for (std::uint32_t index = 0U; index < frames.Size(); ++index) {
        const TFrame& frame = frames[index];
        if (sampleTime > frame.keyTimeMicroseconds) {
            previous = frame.value;
            previousTime = frame.keyTimeMicroseconds;
            sampled = frame.value;
            continue;
        }
        sampled = lerp(
            previous,
            frame.value,
            SegmentProgress(frame, sampleTime, previousTime));
        found = true;
        break;
    }
    if (!found && !frames.Empty()) sampled = frames.Back().value;
    return sampled;
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
            (*track.target).GetAnimationBaseValueInternal( track.property);
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
        const Base::Color color = LerpColor(
            track.fromColor, track.toColor, Ease(progress, track.easing));
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Color>::Encode(color);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::Point) {
        const Base::Point point = LerpPoint(
            track.fromPoint, track.toPoint, Ease(progress, track.easing));
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Point>::Encode(
                point);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::Rect) {
        const Base::Rect rect = LerpRect(
            track.fromRect, track.toRect, Ease(progress, track.easing));
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Rect>::Encode(
                rect);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind ==
        Track::Kind::Thickness) {
        const Base::Thickness thickness = LerpThickness(
            track.fromThickness, track.toThickness, Ease(progress, track.easing));
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
        const Base::Size size = LerpSize(
            track.fromSize, track.toSize, Ease(progress, track.easing));
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
        const double sampledValue = SampleKeyFrames<double>(
            track.doubleFrames,
            track.baseValue,
            sampleTime,
            [](double from, double to, double amount) noexcept {
                return from + (to - from) * amount;
            });
        value = Meta::ValueCodec<double>::Encode(sampledValue).Value();
    } else if (track.kind == Track::Kind::ColorKeyFrames) {
        const Base::Color sampled = SampleKeyFrames<Base::Color>(
            track.colorFrames, track.fromColor, sampleTime,
            LerpColor);
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Color>::Encode(sampled);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::PointKeyFrames) {
        const Base::Point sampled = SampleKeyFrames<Base::Point>(
            track.pointFrames, track.fromPoint, sampleTime,
            LerpPoint);
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Point>::Encode(sampled);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::ThicknessKeyFrames) {
        const Base::Thickness sampled = SampleKeyFrames<Base::Thickness>(
            track.thicknessFrames, track.fromThickness, sampleTime,
            LerpThickness);
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Thickness>::Encode(sampled);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::IntegerKeyFrames) {
        const std::int64_t sampled = SampleKeyFrames<std::int64_t>(
            track.integerFrames, track.fromInteger, sampleTime, LerpInteger);
        Base::Result<Meta::PropertyValue> encoded =
            EncodeIntegerWidth(sampled, track.integerWidth);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::SizeKeyFrames) {
        const Base::Size sampled = SampleKeyFrames<Base::Size>(
            track.sizeFrames, track.fromSize, sampleTime,
            LerpSize);
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Size>::Encode(sampled);
        if (!encoded) return encoded.GetStatus();
        value = std::move(encoded).Value();
    } else if (track.kind == Track::Kind::MatrixKeyFrames) {
        const Base::Transform2D sampled = SampleKeyFrames<Base::Transform2D>(
            track.matrixFrames, track.fromMatrix, sampleTime, LerpMatrix);
        Base::Result<Meta::PropertyValue> encoded =
            Meta::ValueCodec<Base::Transform2D>::Encode(sampled);
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
        DependencyObjectAccess::PropertyRegistry(track.target).Find(
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
    if (UIElement* element = TryCast<UIElement>(track.target)) {
        if (track.property == UIElement::OpacityProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) element->SetAnimatedOpacity(decoded.Value());
        } else if (track.property == UIElement::VisibilityProperty.Handle()) {
            Base::Result<Visibility> decoded =
                Meta::ValueCodec<Visibility>::Decode(value);
            if (decoded) element->SetAnimatedVisibility(decoded.Value());
        }
    }
    if (FrameworkElement* framework = TryCast<FrameworkElement>(track.target)) {
        if (track.property == FrameworkElement::WidthProperty.Handle()) {
            Base::Result<Length> decoded = Meta::ValueCodec<Length>::Decode(value);
            if (decoded) framework->SetAnimatedWidth(decoded.Value());
        } else if (track.property == FrameworkElement::HeightProperty.Handle()) {
            Base::Result<Length> decoded = Meta::ValueCodec<Length>::Decode(value);
            if (decoded) framework->SetAnimatedHeight(decoded.Value());
        } else if (track.property == FrameworkElement::MinWidthProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) framework->SetAnimatedMinWidth(decoded.Value());
        } else if (track.property == FrameworkElement::MinHeightProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) framework->SetAnimatedMinHeight(decoded.Value());
        } else if (track.property == FrameworkElement::MaxWidthProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) framework->SetAnimatedMaxWidth(decoded.Value());
        } else if (track.property == FrameworkElement::MaxHeightProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) framework->SetAnimatedMaxHeight(decoded.Value());
        } else if (track.property == FrameworkElement::MarginProperty.Handle()) {
            Base::Result<Thickness> decoded =
                Meta::ValueCodec<Thickness>::Decode(value);
            if (decoded) framework->SetAnimatedMargin(decoded.Value());
        }
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
