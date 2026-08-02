#pragma once

#include <Aero/Animation.hpp>

#include "AnimationModel.hpp"

namespace Aero::Internal {

class AnimationPrivate final {
public:
    static Animation::TimelineTiming Timing(
        const Media::Animation::Timeline& timeline) noexcept {
        Animation::TimelineTiming result;
        result.beginTimeMicroseconds = timeline.beginTimeMicroseconds_;
        result.durationMicroseconds = timeline.durationMicroseconds_;
        result.repeat = timeline.repeatForever_
            ? Animation::RepeatBehavior::Forever()
            : Animation::RepeatBehavior::Count(timeline.repeatCount_);
        result.speedRatio = timeline.speedRatio_;
        result.autoReverse = timeline.autoReverse_;
        result.fillBehavior = timeline.fillBehavior_;
        return result;
    }

    static Animation::EasingFunction Easing(
        const Media::Animation::EasingFunctionBase& easing) noexcept {
        Animation::EasingFunction result;
        result.kind = static_cast<Animation::EasingFunctionKind>(
            static_cast<std::uint8_t>(easing.kind_));
        result.mode = easing.easingMode_;
        result.power = easing.power_;
        result.amplitude = easing.amplitude_;
        result.oscillations = easing.oscillations_;
        result.springiness = easing.springiness_;
        return result;
    }

    static Animation::DoubleAnimation Double(
        const Media::Animation::DoubleAnimation& animation) noexcept {
        Animation::DoubleAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.accelerationRatio = animation.GetAccelerationRatio();
        result.decelerationRatio = animation.GetDecelerationRatio();
        result.timing = Timing(animation);
        Base::Ref<Media::Animation::EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Animation::ColorAnimation Color(
        const Media::Animation::ColorAnimation& animation) noexcept {
        Animation::ColorAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<Media::Animation::EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Animation::PointAnimation Point(
        const Media::Animation::PointAnimation& animation) noexcept {
        Animation::PointAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<Media::Animation::EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Animation::RectAnimation Rect(
        const Media::Animation::RectAnimation& animation) noexcept {
        Animation::RectAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<Media::Animation::EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Animation::ThicknessAnimation Thickness(
        const Media::Animation::ThicknessAnimation& animation) noexcept {
        Animation::ThicknessAnimation result;
        result.from = animation.GetFrom();
        result.to = animation.GetTo();
        result.timing = Timing(animation);
        Base::Ref<Media::Animation::EasingFunctionBase> easing =
            animation.GetEasingFunction();
        if (easing) result.easing = Easing(*easing);
        return result;
    }

    static Animation::DoubleKeyFrame DoubleFrame(
        const Media::Animation::DoubleKeyFrame& frame) noexcept {
        Animation::DoubleKeyFrame result;
        result.keyTimeMicroseconds = frame.keyTimeMicroseconds_;
        result.value = frame.value_;
        result.interpolation =
            static_cast<Animation::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.interpolation_));
        result.controlPoint1X = frame.controlPoint1X_;
        result.controlPoint1Y = frame.controlPoint1Y_;
        result.controlPoint2X = frame.controlPoint2X_;
        result.controlPoint2Y = frame.controlPoint2Y_;
        if (frame.RuntimeType() ==
            Media::Animation::EasingDoubleKeyFrame::StaticTypeId()) {
            const auto& typed = static_cast<
                const Media::Animation::EasingDoubleKeyFrame&>(frame);
            Base::Ref<Media::Animation::EasingFunctionBase> easing =
                typed.GetEasingFunction();
            if (easing) result.easing = Easing(*easing);
        }
        return result;
    }

    static Animation::ColorKeyFrame ColorFrame(
        const Media::Animation::ColorKeyFrame& frame) noexcept {
        Animation::ColorKeyFrame result;
        result.keyTimeMicroseconds = frame.keyTimeMicroseconds_;
        result.value = frame.value_;
        result.interpolation =
            static_cast<Animation::DoubleKeyFrameInterpolation>(
                static_cast<std::uint8_t>(frame.interpolation_));
        result.controlPoint1X = frame.controlPoint1X_;
        result.controlPoint1Y = frame.controlPoint1Y_;
        result.controlPoint2X = frame.controlPoint2X_;
        result.controlPoint2Y = frame.controlPoint2Y_;
        if (frame.RuntimeType() ==
            Media::Animation::EasingColorKeyFrame::StaticTypeId()) {
            const auto& typed = static_cast<
                const Media::Animation::EasingColorKeyFrame&>(frame);
            Base::Ref<Media::Animation::EasingFunctionBase> easing =
                typed.GetEasingFunction();
            if (easing) result.easing = Easing(*easing);
        }
        return result;
    }
};

} // namespace Aero::Internal
