#include "gui/ViewState.hpp"
#include "gui/media/StoryboardHost.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/core/EventRouter.hpp"
#include <Aero/CommandBinding.hpp>
#include <Aero/Media/Animation/EventTrigger.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/PathGeometry.hpp>
#include <Aero/Media/LineSegment.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/CompositeTransform3D.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/TryCast.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
using namespace Media::Animation;

Base::Result<std::uint32_t> StoryboardHost::BeginTimeline(
        Timeline& timeline,
        FrameworkElement& triggerOwner,
        const NameScope* names,
        const StoryboardTimingState* inherited,
        Base::Vector<
            Model::AnimationHandle>*
            retainedHandles,
        Controls::DataTemplateTriggerInstance*
            dataTemplateContext) noexcept {
        // Disambiguate public Media::Animation types vs Model::* (AnimationEngine.hpp).
        using Media::Animation::ColorAnimation;
        using Media::Animation::ColorKeyFrame;
        using Media::Animation::DoubleAnimation;
        using Media::Animation::DoubleKeyFrame;
        using Media::Animation::MatrixAnimation;
        using Media::Animation::MatrixKeyFrame;
        using Media::Animation::PointAnimation;
        using Media::Animation::PointKeyFrame;
        using Media::Animation::RectAnimation;
        using Media::Animation::SizeAnimation;
        using Media::Animation::SizeKeyFrame;
        using Media::Animation::ThicknessAnimation;
        using Media::Animation::ThicknessKeyFrame;


        if (Animations() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard requires the animation manager");
        }
        if (Metadata()->Types().IsDerivedFrom(
                timeline.RuntimeType(),
                TimelineGroup::StaticTypeId())) {
            auto& nested =
                static_cast<TimelineGroup&>(timeline);
            const StoryboardTimingState timing =
                ComposeStoryboardTiming(
                    inherited,
                    nested,
                    timeline.RuntimeType() ==
                        ParallelTimeline::
                            StaticTypeId());
            std::uint32_t count = 0U;
            for (const Base::Ref<Timeline>& child :
                 nested.GetTimelines()) {
                if (!child) continue;
                Base::Result<std::uint32_t> started =
                    BeginTimeline(
                        *child, triggerOwner, names, &timing,
                        retainedHandles,
                        dataTemplateContext);
                if (!started) {
                    if (view != nullptr) {
                        view->ReportUpdateFailure(started.GetStatus());
                    }
                    continue;
                }
                if (count > UINT32_MAX - started.Value()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard child count overflow");
                }
                count += started.Value();
            }
            return count;
        }

        Base::Result<Base::StringView> targetName =
            AnimationAttachedString(
                timeline,
                Storyboard::TargetNameProperty);
        if (!targetName) return targetName.GetStatus();
        Base::Result<Base::StringView> targetPath =
            AnimationAttachedString(
                timeline,
                Storyboard::TargetPropertyProperty);
        if (!targetPath) return targetPath.GetStatus();

        Base::Object* targetObject =
            targetName.Value().Empty()
            ? static_cast<Base::Object*>(
                  &triggerOwner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(
                      targetName.Value())
                : names != nullptr
                    ? names->Find(targetName.Value())
                    : view->loadedDocument.names.Find(
                          targetName.Value());
        if (targetObject == nullptr) {
            targetObject = triggerOwner.FindName(targetName.Value());
        }
        if (targetObject == nullptr && names != nullptr) {
            targetObject = view->loadedDocument.names.Find(
                targetName.Value());
        }
        if (targetObject == nullptr ||
            !Metadata()->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Storyboard target name does not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<DependencyObject&>(*targetObject);
        Base::Result<ResolvedAnimationProperty> property =
            ResolveAnimationProperty(target, targetPath.Value());
        if (!property) return property.GetStatus();
        DependencyObject& propertyTarget =
            *property.Value().target;
        const Meta::DependencyPropertyHandle propertyHandle =
            property.Value().property;

        const Meta::TypeId type = timeline.RuntimeType();
        if (type == DoubleAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<DoubleAnimation&>(timeline);
            Model::DoubleAnimation runtime =
                Media::Animation::Double(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Model::AnimationHandle> started =
                Animations()->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (Metadata()->Types().IsDerivedFrom(
                type,
                DoubleAnimationBase::StaticTypeId())) {
            auto& animation = static_cast<
                DoubleAnimationBase&>(timeline);
            Base::Result<Meta::PropertyValue> current =
                propertyTarget.GetValue(propertyHandle);
            if (!current) return current.GetStatus();
            Base::Result<double> origin =
                Meta::ValueCodec<double>::Decode(current.Value());
            if (!origin) return origin.GetStatus();

            Model::CustomDoubleAnimation runtime;
            runtime.animation =
                Base::Ref<DoubleAnimationBase>::
                    TryFromBorrowed(animation);
            if (!runtime.animation) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Custom DoubleAnimation is not reference-counted");
            }
            runtime.defaultOriginValue = origin.Value();
            runtime.defaultDestinationValue =
                animation.ResolveTo(origin.Value());
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type == ColorAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<ColorAnimation&>(timeline);
            Model::ColorAnimation runtime =
                Media::Animation::Color(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Model::AnimationHandle> started =
                Animations()->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            PointAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    PointAnimation&>(
                        timeline);
            Model::PointAnimation runtime =
                Media::Animation::Point(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            RectAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    RectAnimation&>(
                        timeline);
            Model::RectAnimation runtime =
                Media::Animation::Rect(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            ThicknessAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    ThicknessAnimation&>(
                        timeline);
            Model::ThicknessAnimation runtime =
                Media::Animation::Thickness(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            Int16Animation::StaticTypeId()) {
            auto& animation =
                static_cast<Int16Animation&>(timeline);
            Model::IntegerAnimation runtime =
                Media::Animation::Integer16(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            Int32Animation::StaticTypeId()) {
            auto& animation =
                static_cast<Int32Animation&>(timeline);
            Model::IntegerAnimation runtime =
                Media::Animation::Integer32(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            Int64Animation::StaticTypeId()) {
            auto& animation =
                static_cast<Int64Animation&>(timeline);
            Model::IntegerAnimation runtime =
                Media::Animation::Integer64(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            SizeAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<SizeAnimation&>(timeline);
            Model::SizeAnimation runtime =
                Media::Animation::Size(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            MatrixAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MatrixAnimation&>(timeline);
            Model::MatrixAnimation runtime =
                Media::Animation::Matrix(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            DoubleAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                DoubleAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Model::DoubleKeyFrame> frames(Allocator());
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<DoubleKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                frames.PushBack(Media::Animation::DoubleFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size(); ++index) {
                Model::DoubleKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U]
                               .keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] =
                        frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<double> baseDouble =
                Meta::ValueCodec<double>::Decode(base.Value());
            Model::DoubleKeyFrameAnimation runtime;
            if (baseDouble) {
                runtime.baseValue = baseDouble.Value();
            } else if (!frames.Empty() &&
                       frames.Front().keyTimeMicroseconds == 0U) {
                // A zero-time key frame defines the initial animated value;
                // no interpolation can observe the underlying base value.
                // This also lets XAML start a key-frame animation on a
                // property whose unset metadata representation is not a
                // concrete double.
                runtime.baseValue = frames.Front().value;
            } else {
                return baseDouble.GetStatus();
            }
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            if (runtime.timing.durationMicroseconds == 0U &&
                !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<Model::AnimationHandle> started =
                Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            ColorAnimationUsingKeyFrames::
                StaticTypeId()) {
            auto& animation = static_cast<
                ColorAnimationUsingKeyFrames&>(
                    timeline);
            Base::Vector<Model::ColorKeyFrame>
                frames(Allocator());
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<
                     ColorKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                frames.PushBack(
                        Media::Animation::ColorFrame(
                            *frame,
                            schedule.duration,
                            keyIndex,
                            schedule.count));
                ++keyIndex;
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size();
                 ++index) {
                Model::ColorKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U]
                               .keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] =
                        frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(
                    propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<Base::Color> baseColor =
                Meta::ValueCodec<Base::Color>::Decode(
                    base.Value());
            if (!baseColor) {
                return baseColor.GetStatus();
            }
            Model::ColorKeyFrameAnimation
                runtime;
            runtime.baseValue = baseColor.Value();
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            if (runtime.timing.durationMicroseconds ==
                    0U &&
                !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back()
                        .keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<
                Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            PointAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                PointAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Model::PointKeyFrame> frames(
                Allocator());
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<PointKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                frames.PushBack(
                    Media::Animation::PointFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
            }
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Model::PointKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U].keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] = frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<Base::Point> basePoint =
                Meta::ValueCodec<Base::Point>::Decode(base.Value());
            Model::PointKeyFrameAnimation runtime;
            if (basePoint) {
                runtime.baseValue = basePoint.Value();
            } else if (!frames.Empty() &&
                       frames.Front().keyTimeMicroseconds == 0U) {
                runtime.baseValue = frames.Front().value;
            } else {
                return basePoint.GetStatus();
            }
            runtime.timing = EffectiveTimelineTiming(animation, inherited);
            if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            ThicknessAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                ThicknessAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Model::ThicknessKeyFrame>
                frames(Allocator());
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<ThicknessKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                frames.PushBack(
                    Media::Animation::ThicknessFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
            }
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Model::ThicknessKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U].keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] = frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<Base::Thickness> baseThickness =
                Meta::ValueCodec<Base::Thickness>::Decode(base.Value());
            Model::ThicknessKeyFrameAnimation runtime;
            if (baseThickness) {
                runtime.baseValue = baseThickness.Value();
            } else if (!frames.Empty() &&
                       frames.Front().keyTimeMicroseconds == 0U) {
                runtime.baseValue = frames.Front().value;
            } else {
                return baseThickness.GetStatus();
            }
            runtime.timing = EffectiveTimelineTiming(animation, inherited);
            if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }

            auto startIntegerKeyFrames =
            [&](Model::IntegerAnimationWidth width,
                auto&& collect)
                -> Base::Result<std::uint32_t> {
            Base::Vector<Model::IntegerKeyFrame>
                frames(Allocator());
            Base::Result<void> collected = collect(frames);
            if (!collected) return collected.GetStatus();
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Model::IntegerKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U].keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] = frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(propertyHandle);
            if (!base) return base.GetStatus();
            Model::IntegerKeyFrameAnimation runtime;
            runtime.width = width;
            if (width ==
                Model::IntegerAnimationWidth::Int16) {
                Base::Result<std::int16_t> decoded =
                    Meta::ValueCodec<std::int16_t>::Decode(base.Value());
                if (decoded) {
                    runtime.baseValue = decoded.Value();
                } else if (!frames.Empty() &&
                           frames.Front().keyTimeMicroseconds == 0U) {
                    runtime.baseValue = frames.Front().value;
                } else {
                    return decoded.GetStatus();
                }
            } else if (
                width ==
                Model::IntegerAnimationWidth::Int64) {
                Base::Result<std::int64_t> decoded =
                    Meta::ValueCodec<std::int64_t>::Decode(base.Value());
                if (decoded) {
                    runtime.baseValue = decoded.Value();
                } else if (!frames.Empty() &&
                           frames.Front().keyTimeMicroseconds == 0U) {
                    runtime.baseValue = frames.Front().value;
                } else {
                    return decoded.GetStatus();
                }
            } else {
                Base::Result<std::int32_t> decoded =
                    Meta::ValueCodec<std::int32_t>::Decode(base.Value());
                if (decoded) {
                    runtime.baseValue = decoded.Value();
                } else if (!frames.Empty() &&
                           frames.Front().keyTimeMicroseconds == 0U) {
                    runtime.baseValue = frames.Front().value;
                } else {
                    return decoded.GetStatus();
                }
            }
            runtime.timing = EffectiveTimelineTiming(timeline, inherited);
            if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        };

        if (type ==
            Int16AnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                Int16AnimationUsingKeyFrames&>(timeline);
            return startIntegerKeyFrames(
                Model::IntegerAnimationWidth::Int16,
                [&](auto& frames) -> Base::Result<void> {
                    const auto schedule = MakeKeyframeSchedule(
                        animation,
                        EffectiveTimelineTiming(
                            animation, inherited).durationMicroseconds);
                    std::uint32_t keyIndex = 0U;
                    for (const Base::Ref<Int16KeyFrame>& frame :
                         animation.GetKeyFrames()) {
                        if (!frame) continue;
                        frames.PushBack(
                            Media::Animation::IntegerFrame(
                                *frame,
                                schedule.duration,
                                keyIndex,
                                schedule.count));
                        ++keyIndex;
                    }
                    return {};
                });
        }
        if (type ==
            Int32AnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                Int32AnimationUsingKeyFrames&>(timeline);
            return startIntegerKeyFrames(
                Model::IntegerAnimationWidth::Int32,
                [&](auto& frames) -> Base::Result<void> {
                    const auto schedule = MakeKeyframeSchedule(
                        animation,
                        EffectiveTimelineTiming(
                            animation, inherited).durationMicroseconds);
                    std::uint32_t keyIndex = 0U;
                    for (const Base::Ref<Int32KeyFrame>& frame :
                         animation.GetKeyFrames()) {
                        if (!frame) continue;
                        frames.PushBack(
                            Media::Animation::IntegerFrame(
                                *frame,
                                schedule.duration,
                                keyIndex,
                                schedule.count));
                        ++keyIndex;
                    }
                    return {};
                });
        }
        if (type ==
            Int64AnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                Int64AnimationUsingKeyFrames&>(timeline);
            return startIntegerKeyFrames(
                Model::IntegerAnimationWidth::Int64,
                [&](auto& frames) -> Base::Result<void> {
                    const auto schedule = MakeKeyframeSchedule(
                        animation,
                        EffectiveTimelineTiming(
                            animation, inherited).durationMicroseconds);
                    std::uint32_t keyIndex = 0U;
                    for (const Base::Ref<Int64KeyFrame>& frame :
                         animation.GetKeyFrames()) {
                        if (!frame) continue;
                        frames.PushBack(
                            Media::Animation::IntegerFrame(
                                *frame,
                                schedule.duration,
                                keyIndex,
                                schedule.count));
                        ++keyIndex;
                    }
                    return {};
                });
        }
        if (type ==
            SizeAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                SizeAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Model::SizeKeyFrame>
                frames(Allocator());
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<SizeKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                frames.PushBack(
                    Media::Animation::SizeFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
            }
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Model::SizeKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U].keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] = frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<Base::Size> baseSize =
                Meta::ValueCodec<Base::Size>::Decode(base.Value());
            Model::SizeKeyFrameAnimation runtime;
            if (baseSize) {
                runtime.baseValue = baseSize.Value();
            } else if (!frames.Empty() &&
                       frames.Front().keyTimeMicroseconds == 0U) {
                runtime.baseValue = frames.Front().value;
            } else {
                return baseSize.GetStatus();
            }
            runtime.timing = EffectiveTimelineTiming(animation, inherited);
            if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            MatrixAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MatrixAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Model::MatrixKeyFrame>
                frames(Allocator());
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<MatrixKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                frames.PushBack(
                    Media::Animation::MatrixFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
            }
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Model::MatrixKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U].keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] = frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<Base::Transform2D> baseMatrix =
                Meta::ValueCodec<Base::Transform2D>::Decode(base.Value());
            Model::MatrixKeyFrameAnimation runtime;
            if (baseMatrix) {
                runtime.baseValue = baseMatrix.Value();
            } else if (!frames.Empty() &&
                       frames.Front().keyTimeMicroseconds == 0U) {
                runtime.baseValue = frames.Front().value;
            } else {
                return baseMatrix.GetStatus();
            }
            runtime.timing = EffectiveTimelineTiming(animation, inherited);
            if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<Model::AnimationHandle>
                started = Animations()->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }

        Base::Vector<Model::DiscreteAnimationKeyFrame>
            frames(Allocator());
        if (type ==
            BooleanAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                BooleanAnimationUsingKeyFrames&>(timeline);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(timeline, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<
                     BooleanKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    Media::Animation::ResolveKeyTime(
                        frame->GetKeyTime(),
                        schedule.duration,
                        keyIndex,
                        schedule.count);
                ++keyIndex;
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<bool>::Encode(frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value = std::move(encoded).Value();
                frames.PushBack(std::move(runtime));
            }
        } else if (type ==
            ObjectAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                ObjectAnimationUsingKeyFrames&>(timeline);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(timeline, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<
                     ObjectKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    Media::Animation::ResolveKeyTime(
                        frame->GetKeyTime(),
                        schedule.duration,
                        keyIndex,
                        schedule.count);
                ++keyIndex;
                runtime.value = frame->GetValue();
                const Meta::DependencyProperty* targetProperty =
                    AeroGuiInternal::PropertyRegistry(propertyTarget).Find(
                        propertyHandle);
                if (targetProperty != nullptr &&
                    runtime.value.IsNullObject() &&
                    runtime.value.Type() !=
                        targetProperty->ValueType()) {
                    runtime.value =
                        Meta::PropertyValue::NullObject(
                            targetProperty->ValueType());
                }
                frames.PushBack(std::move(runtime));
            }
        } else if (type ==
            StringAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                StringAnimationUsingKeyFrames&>(timeline);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(timeline, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<
                     StringKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    Media::Animation::ResolveKeyTime(
                        frame->GetKeyTime(),
                        schedule.duration,
                        keyIndex,
                        schedule.count);
                ++keyIndex;
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<Base::String>::Encode(frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value = std::move(encoded).Value();
                frames.PushBack(std::move(runtime));
            }
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Storyboard contains an unsupported Timeline type");
        }
        for (std::uint32_t index = 1U;
             index < frames.Size(); ++index) {
            Model::DiscreteAnimationKeyFrame current =
                std::move(frames[index]);
            std::uint32_t position = index;
            while (position > 0U &&
                   frames[position - 1U]
                           .keyTimeMicroseconds >
                       current.keyTimeMicroseconds) {
                frames[position] =
                    std::move(frames[position - 1U]);
                --position;
            }
            frames[position] = std::move(current);
        }
        Base::Result<Meta::PropertyValue> base =
            propertyTarget.GetValue(propertyHandle);
        if (!base) return base.GetStatus();
        Model::DiscreteAnimation runtime;
        runtime.baseValue = base.Value();
        runtime.timing =
            EffectiveTimelineTiming(
                timeline, inherited);
        if (runtime.timing.durationMicroseconds == 0U &&
            !frames.Empty()) {
            runtime.timing.durationMicroseconds =
                frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        Base::Result<Model::AnimationHandle> started =
            Animations()->Begin(
                propertyTarget, propertyHandle, runtime);
        return RetainStartedAnimation(
            std::move(started),
            retainedHandles);
    }

} // namespace Aero
