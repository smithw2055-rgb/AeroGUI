#pragma once

#include "gui/meta/MetadataState.hpp"
#include "render/DisplayList.hpp"

#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Base/Span.hpp>

#include "gui/media/AnimationModel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Aero::Media {

inline double SpreadPosition(
    double value,
    Media::GradientSpreadMethod method) noexcept {
    if (method == Media::GradientSpreadMethod::Repeat) {
        value -= std::floor(value);
        return value < 0.0 ? value + 1.0 : value;
    }
    if (method == Media::GradientSpreadMethod::Reflect) {
        value = std::fmod(std::fabs(value), 2.0);
        return value <= 1.0 ? value : 2.0 - value;
    }
    return std::clamp(value, 0.0, 1.0);
}

inline Base::Color SampleGradient(
    const Media::GradientBrush& brush,
    double position) noexcept {
    const auto stops = brush.GetGradientStops();
    if (stops.Empty()) return {};
    position = SpreadPosition(position, brush.GetSpreadMethod());
    const Media::GradientStop* lower = nullptr;
    const Media::GradientStop* upper = nullptr;
    for (const Base::Ref<Media::GradientStop>& stop : stops) {
        if (!stop) continue;
        if (stop->GetOffset() <= position &&
            (lower == nullptr || stop->GetOffset() >= lower->GetOffset())) {
            lower = stop.Get();
        }
        if (stop->GetOffset() >= position &&
            (upper == nullptr || stop->GetOffset() < upper->GetOffset())) {
            upper = stop.Get();
        }
    }
    if (lower == nullptr) lower = upper;
    if (upper == nullptr) upper = lower;
    if (lower == nullptr || upper == nullptr) return {};
    const double span = upper->GetOffset() - lower->GetOffset();
    const float amount = span > 0.0
        ? static_cast<float>(std::clamp(
            (position - lower->GetOffset()) / span, 0.0, 1.0))
        : 0.0F;
    const Base::Color a = lower->GetColor();
    const Base::Color b = upper->GetColor();
    Base::Color result{
        a.red + (b.red - a.red) * amount,
        a.green + (b.green - a.green) * amount,
        a.blue + (b.blue - a.blue) * amount,
        a.alpha + (b.alpha - a.alpha) * amount};
    result.alpha *= static_cast<float>(brush.GetOpacity());
    return result;
}

inline double ShaderDouble(
    const Media::BrushShader& shader,
    Base::StringView name,
    double fallback) noexcept {
    const Meta::DependencyProperty* property =
        shader.PropertyRegistry().Find(shader.RuntimeType(), name);
    if (property == nullptr) return fallback;
    Base::Result<Meta::Value> value =
        shader.GetValue(property->Handle());
    if (!value) return fallback;
    Base::Result<double> decoded =
        Meta::ValueCodec<double>::Decode(value.Value());
    return decoded ? decoded.Value() : fallback;
}

inline Base::Color ShaderColor(
    const Media::BrushShader& shader,
    Base::StringView name,
    Base::Color fallback) noexcept {
    const Meta::DependencyProperty* property =
        shader.PropertyRegistry().Find(shader.RuntimeType(), name);
    if (property == nullptr) return fallback;
    Base::Result<Meta::Value> value =
        shader.GetValue(property->Handle());
    if (!value) return fallback;
    Base::Result<Base::Color> decoded =
        Meta::ValueCodec<Base::Color>::Decode(value.Value());
    return decoded ? decoded.Value() : fallback;
}

inline Base::Color SampleStops(
    Base::Span<const Base::Ref<Media::GradientStop>> stops,
    double position,
    Base::Color fallback) noexcept {
    if (stops.Empty()) return fallback;
    position = SpreadPosition(
        position, Media::GradientSpreadMethod::Repeat);
    const Media::GradientStop* lower = nullptr;
    const Media::GradientStop* upper = nullptr;
    for (const Base::Ref<Media::GradientStop>& stop : stops) {
        if (!stop) continue;
        if (stop->GetOffset() <= position &&
            (lower == nullptr || stop->GetOffset() > lower->GetOffset())) {
            lower = stop.Get();
        }
        if (stop->GetOffset() >= position &&
            (upper == nullptr || stop->GetOffset() < upper->GetOffset())) {
            upper = stop.Get();
        }
    }
    if (lower == nullptr) lower = upper;
    if (upper == nullptr) upper = lower;
    if (lower == nullptr || upper == nullptr) return fallback;
    const double span = upper->GetOffset() - lower->GetOffset();
    const float amount = span > 0.0
        ? static_cast<float>((position - lower->GetOffset()) / span)
        : 0.0F;
    const Base::Color a = lower->GetColor();
    const Base::Color b = upper->GetColor();
    return {
        a.red + (b.red - a.red) * amount,
        a.green + (b.green - a.green) * amount,
        a.blue + (b.blue - a.blue) * amount,
        a.alpha + (b.alpha - a.alpha) * amount};
}

inline Base::Color ApplyShader(
    const Media::BrushShader& shader,
    Base::Color source,
    Base::Point uv,
    Base::Size) noexcept {
    if (shader.RuntimeType() ==
        Media::MonochromeShader::StaticTypeId()) {
        const Base::Color color =
            static_cast<const Media::MonochromeShader&>(shader).GetColor();
        const float luminance =
            source.red * 0.2126F +
            source.green * 0.7152F +
            source.blue * 0.0722F;
        return {color.red * luminance, color.green * luminance,
            color.blue * luminance, source.alpha * color.alpha};
    }
    if (shader.RuntimeType() ==
        Media::ConicGradientShader::StaticTypeId()) {
        const double angle =
            std::atan2(uv.y - 0.5, uv.x - 0.5) /
                (2.0 * 3.14159265358979323846) + 0.5;
        return SampleStops(
            static_cast<const Media::ConicGradientShader&>(shader)
                .GetGradientStops(),
            angle, source);
    }
    const double time = ShaderDouble(shader, "Time", 0.0);
    if (shader.RuntimeType() == Media::WavesShader::StaticTypeId()) {
        const float wave = static_cast<float>(
            0.65 + 0.35 * std::sin(
                (uv.x * 18.0 + uv.y * 12.0) + time * 0.01));
        return {source.red * wave, source.green * wave,
            source.blue * wave, source.alpha};
    }
    const double scaleX = ShaderDouble(shader, "ScaleX", 64.0);
    const double scaleY = ShaderDouble(shader, "ScaleY", 64.0);
    const double seed = ShaderDouble(shader, "Seed", 0.0);
    const double phase =
        std::sin((uv.x * scaleX + uv.y * scaleY + seed) * 12.9898 +
                 time * 0.0001) * 43758.5453;
    const float noise = static_cast<float>(phase - std::floor(phase));
    const Base::Color color = ShaderColor(shader, "Color", source);
    const float amount = 0.35F + noise * 0.65F;
    return {color.red * amount, color.green * amount,
        color.blue * amount, source.alpha * color.alpha};
}

inline Base::Color SampleBrush(
    const Base::Ref<Media::Brush>& brush,
    double position = 0.5,
    Base::Color fallback = {0.0F, 0.0F, 0.0F, 0.0F},
    Base::Point uv = {0.5, 0.5},
    Base::Size size = {1.0, 1.0}) noexcept {
    if (!brush) return fallback;
    Base::Color sampled = fallback;
    if (brush->RuntimeType() == Media::SolidColorBrush::StaticTypeId()) {
        sampled = static_cast<Media::SolidColorBrush*>(
            brush.Get())->GetColor();
        sampled.alpha *= static_cast<float>(brush->GetOpacity());
    } else if (brush->RuntimeType() ==
                   Media::LinearGradientBrush::StaticTypeId() ||
               brush->RuntimeType() ==
                   Media::RadialGradientBrush::StaticTypeId()) {
        sampled = SampleGradient(
            *static_cast<Media::GradientBrush*>(brush.Get()), position);
    } else if (brush->RuntimeType() == Media::ImageBrush::StaticTypeId()) {
        sampled = {1.0F, 1.0F, 1.0F,
            static_cast<float>(brush->GetOpacity())};
    }
    Base::Ref<Base::Object> shaderObject = brush->GetShader();
    if (shaderObject && brush->PropertyRegistry().Types().IsDerivedFrom(
            shaderObject->RuntimeType(), Media::BrushShader::StaticTypeId())) {
        sampled = ApplyShader(
            static_cast<const Media::BrushShader&>(*shaderObject),
            sampled, uv, size);
    }
    return sampled;
}

} // namespace Aero::Media

namespace Aero::Media::Animation {

struct TimelineRuntime {
public:
    static Model::TimelineTiming Timing(
        const Timeline& timeline) noexcept {
        Model::TimelineTiming result;
        result.beginTimeMicroseconds =
            timeline.GetBeginTime().Microseconds();
        const Duration duration = timeline.GetDuration();
        if (duration.IsForever()) {
            result.durationMicroseconds = UINT64_MAX;
        } else if (duration.HasTimeSpan()) {
            result.durationMicroseconds =
                duration.GetTimeSpan().Microseconds();
        } else {
            result.durationMicroseconds = 0U;
        }
        const RepeatBehavior repeat = timeline.GetRepeatBehavior();
        if (repeat.IsForever()) {
            result.repeat = Model::RepeatBehavior::Forever();
        } else if (repeat.HasDuration()) {
            const std::uint64_t span = repeat.GetDuration().Microseconds();
            if (result.durationMicroseconds > 0U &&
                result.durationMicroseconds != UINT64_MAX) {
                result.repeat = Model::RepeatBehavior::Count(
                    static_cast<double>(span) /
                    static_cast<double>(result.durationMicroseconds));
            } else {
                result.repeat = Model::RepeatBehavior::Once();
            }
        } else {
            result.repeat = Model::RepeatBehavior::Count(repeat.GetCount());
        }
        result.speedRatio = timeline.GetSpeedRatio();
        result.autoReverse = timeline.GetAutoReverse();
        result.fillBehavior = timeline.GetFillBehavior();
        return result;
    }

    struct KeyframeSchedule {
        AnimationTime duration = 0U;
        std::uint32_t count = 0U;
    };

    template<class TKeyFrame>
    static KeyframeSchedule MakeSchedule(
        Base::Span<const Base::Ref<TKeyFrame>> frames,
        AnimationTime authoredDuration) noexcept {
        KeyframeSchedule schedule;
        AnimationTime maxTimeSpan = 0U;
        for (const Base::Ref<TKeyFrame>& frame : frames) {
            if (!frame) continue;
            ++schedule.count;
            const KeyTime keyTime = frame->GetKeyTime();
            if (keyTime.IsTimeSpan() &&
                keyTime.GetTimeSpan().Microseconds() > maxTimeSpan) {
                maxTimeSpan = keyTime.GetTimeSpan().Microseconds();
            }
        }
        schedule.duration =
            (authoredDuration == 0U || authoredDuration == UINT64_MAX)
            ? maxTimeSpan
            : authoredDuration;
        return schedule;
    }

    static AnimationTime ResolveKeyTime(
        const KeyTime& keyTime,
        AnimationTime duration,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        return keyTime.ResolveMicroseconds(duration, index, count);
    }

    static Model::EasingFunction Easing(
        const EasingFunctionBase& easing) noexcept {
        Model::EasingFunction result;
        result.kind = static_cast<Model::EasingFunctionKind>(
            static_cast<std::uint8_t>(easing.kind_));
        result.mode = easing.GetEasingMode();
        if (easing.RuntimeType() == PowerEase::StaticTypeId()) {
            result.power = static_cast<const PowerEase&>(easing).GetPower();
        } else if (easing.RuntimeType() == ExponentialEase::StaticTypeId()) {
            result.power = static_cast<const ExponentialEase&>(easing).GetExponent();
        } else if (easing.RuntimeType() == BackEase::StaticTypeId()) {
            result.amplitude = static_cast<const BackEase&>(easing).GetAmplitude();
        } else if (easing.RuntimeType() == BounceEase::StaticTypeId()) {
            const auto& bounce = static_cast<const BounceEase&>(easing);
            result.oscillations = bounce.GetBounces();
            result.springiness = bounce.GetBounciness();
        } else if (easing.RuntimeType() == ElasticEase::StaticTypeId()) {
            const auto& elastic = static_cast<const ElasticEase&>(easing);
            result.oscillations = elastic.GetOscillations();
            result.springiness = elastic.GetSpringiness();
        }
        return result;
    }

    static Model::DoubleAnimation Double(
        const DoubleAnimation& animation) noexcept {
        Model::DoubleAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.accelerationRatio = animation.GetAccelerationRatio();
        result.decelerationRatio = animation.GetDecelerationRatio();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::ColorAnimation Color(
        const ColorAnimation& animation) noexcept {
        Model::ColorAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::PointAnimation Point(
        const PointAnimation& animation) noexcept {
        Model::PointAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::RectAnimation Rect(
        const RectAnimation& animation) noexcept {
        Model::RectAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::ThicknessAnimation Thickness(
        const ThicknessAnimation& animation) noexcept {
        Model::ThicknessAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::IntegerAnimation Integer16(
        const Int16Animation& animation) noexcept {
        Model::IntegerAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.width = Model::IntegerAnimationWidth::Int16;
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::IntegerAnimation Integer32(
        const Int32Animation& animation) noexcept {
        Model::IntegerAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.width = Model::IntegerAnimationWidth::Int32;
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::IntegerAnimation Integer64(
        const Int64Animation& animation) noexcept {
        Model::IntegerAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.width = Model::IntegerAnimationWidth::Int64;
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::SizeAnimation Size(
        const SizeAnimation& animation) noexcept {
        Model::SizeAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::MatrixAnimation Matrix(
        const MatrixAnimation& animation) noexcept {
        Model::MatrixAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::DoubleKeyFrame DoubleFrame(
        const DoubleKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::DoubleKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::ColorKeyFrame ColorFrame(
        const ColorKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::ColorKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::PointKeyFrame PointFrame(
        const PointKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::PointKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::ThicknessKeyFrame ThicknessFrame(
        const ThicknessKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::ThicknessKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::IntegerKeyFrame IntegerFrame(
        const Int16KeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::IntegerKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::IntegerKeyFrame IntegerFrame(
        const Int32KeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::IntegerKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::IntegerKeyFrame IntegerFrame(
        const Int64KeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::IntegerKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::SizeKeyFrame SizeFrame(
        const SizeKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::SizeKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Model::MatrixKeyFrame MatrixFrame(
        const MatrixKeyFrame& frame,
        AnimationTime durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) noexcept {
        Model::MatrixKeyFrame result;
        result.keyTimeMicroseconds = ResolveKeyTime(
            frame.GetKeyTime(), durationMicroseconds, index, count);
        result.value = frame.GetValue();
        result.interpolation =
            static_cast<Model::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.GetInterpolation()));
        result.controlPoint1X = frame.GetSplineControlPoint1X();
        result.controlPoint1Y = frame.GetSplineControlPoint1Y();
        result.controlPoint2X = frame.GetSplineControlPoint2X();
        result.controlPoint2Y = frame.GetSplineControlPoint2Y();
        Base::Ref<EasingFunctionBase> easing = frame.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }
};

} // namespace Aero::Media::Animation

namespace Aero::Media {
using AnimationPrivate = ::Aero::Media::Animation::TimelineRuntime;
}

namespace Aero::Media {

struct TransformRuntime {
public:
    static std::uint64_t Revision(
        const Aero::Media::Transform& transform) noexcept;
};

} // namespace Aero::Media

namespace Aero::Media {
using TransformPrivate = ::Aero::Media::TransformRuntime;

} // namespace Aero::Media

namespace Aero::Media {

// Effect ownership is a runtime attachment detail.  Keep it out of the SDK
// surface while allowing the metadata bridge to update it when an Effect
// property is assigned to a FrameworkElement.
struct EffectRuntime {
public:
    static std::uint64_t Revision(
        const Aero::Media::Effect& effect) noexcept;
};

} // namespace Aero::Media

namespace Aero::Media {
using EffectPrivate = ::Aero::Media::EffectRuntime;

} // namespace Aero::Media