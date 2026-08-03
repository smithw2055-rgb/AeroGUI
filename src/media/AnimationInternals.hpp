#pragma once

#include <Aero/Animation.hpp>

#include "AnimationModel.hpp"

namespace Aero::Media::Animation {

namespace Runtime = ::Aero::Animation;

struct Timeline::Impl {
public:
    static Runtime::TimelineTiming Timing(
        const Timeline& timeline) noexcept {
        Runtime::TimelineTiming result;
        result.beginTimeMicroseconds = timeline.beginTimeMicroseconds_;
        result.durationMicroseconds = timeline.durationMicroseconds_;
        result.repeat = timeline.repeatForever_
            ? Runtime::RepeatBehavior::Forever()
            : Runtime::RepeatBehavior::Count(timeline.repeatCount_);
        result.speedRatio = timeline.speedRatio_;
        result.autoReverse = timeline.autoReverse_;
        result.fillBehavior = timeline.fillBehavior_;
        return result;
    }

    static Runtime::EasingFunction Easing(
        const EasingFunctionBase& easing) noexcept {
        Runtime::EasingFunction result;
        result.kind = static_cast<Runtime::EasingFunctionKind>(
            static_cast<std::uint8_t>(easing.kind_));
        result.mode = easing.easingMode_;
        result.power = easing.power_;
        result.amplitude = easing.amplitude_;
        result.oscillations = easing.oscillations_;
        result.springiness = easing.springiness_;
        return result;
    }

    static Runtime::DoubleAnimation Double(
        const DoubleAnimation& animation) noexcept {
        Runtime::DoubleAnimation result;
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

    static Runtime::ColorAnimation Color(
        const ColorAnimation& animation) noexcept {
        Runtime::ColorAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Runtime::PointAnimation Point(
        const PointAnimation& animation) noexcept {
        Runtime::PointAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Runtime::RectAnimation Rect(
        const RectAnimation& animation) noexcept {
        Runtime::RectAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Runtime::ThicknessAnimation Thickness(
        const ThicknessAnimation& animation) noexcept {
        Runtime::ThicknessAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Runtime::DoubleKeyFrame DoubleFrame(
        const DoubleKeyFrame& frame) noexcept {
        Runtime::DoubleKeyFrame result;
        result.keyTimeMicroseconds = frame.keyTimeMicroseconds_;
        result.value = frame.value_;
        result.interpolation =
            static_cast<Runtime::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.interpolation_));
        result.controlPoint1X = frame.controlPoint1X_;
        result.controlPoint1Y = frame.controlPoint1Y_;
        result.controlPoint2X = frame.controlPoint2X_;
        result.controlPoint2Y = frame.controlPoint2Y_;
        if (frame.RuntimeType() == EasingDoubleKeyFrame::StaticTypeId()) {
            const auto& typed = static_cast<
                const EasingDoubleKeyFrame&>(frame);
            Base::Ref<EasingFunctionBase> easing =
                typed.GetEasingFunction();
            if (easing) result.easing = Easing(*easing);
        }
        return result;
    }

    static Runtime::ColorKeyFrame ColorFrame(
        const ColorKeyFrame& frame) noexcept {
        Runtime::ColorKeyFrame result;
        result.keyTimeMicroseconds = frame.keyTimeMicroseconds_;
        result.value = frame.value_;
        result.interpolation =
            static_cast<Runtime::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.interpolation_));
        result.controlPoint1X = frame.controlPoint1X_;
        result.controlPoint1Y = frame.controlPoint1Y_;
        result.controlPoint2X = frame.controlPoint2X_;
        result.controlPoint2Y = frame.controlPoint2Y_;
        if (frame.RuntimeType() == EasingColorKeyFrame::StaticTypeId()) {
            const auto& typed = static_cast<
                const EasingColorKeyFrame&>(frame);
            Base::Ref<EasingFunctionBase> easing =
                typed.GetEasingFunction();
            if (easing) result.easing = Easing(*easing);
        }
        return result;
    }
};

} // namespace Aero::Media::Animation

namespace Aero::Internal {
using AnimationPrivate = ::Aero::Media::Animation::Timeline::Impl;
}
