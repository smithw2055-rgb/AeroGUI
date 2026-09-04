#include "gui/ViewState.hpp"
#include "gui/media/StoryboardHostInternal.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/core/state/EventRouter.hpp"
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
namespace MediaAnimation = ::Aero::Media::Animation;

Base::Result<std::uint32_t> StoryboardHost::BeginTimeline(
        MediaAnimation::Timeline& timeline,
        Aero::FrameworkElement& triggerOwner,
        const Aero::NameScope* names,
        const StoryboardTimingState* inherited,
        Base::Vector<
            Aero::Media::Animation::Model::AnimationHandle>*
            retainedHandles,
        Aero::Controls::DataTemplateTriggerState*
            dataTemplateContext) noexcept {
        if (animations == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard requires the animation manager");
        }
        if (metadata->Types().IsDerivedFrom(
                timeline.RuntimeType(),
                MediaAnimation::TimelineGroup::StaticTypeId())) {
            auto& nested =
                static_cast<MediaAnimation::TimelineGroup&>(timeline);
            const StoryboardTimingState timing =
                ComposeStoryboardTiming(
                    inherited,
                    nested,
                    timeline.RuntimeType() ==
                        MediaAnimation::ParallelTimeline::
                            StaticTypeId());
            std::uint32_t count = 0U;
            for (const Base::Ref<MediaAnimation::Timeline>& child :
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
                MediaAnimation::Storyboard::TargetNameProperty);
        if (!targetName) return targetName.GetStatus();
        Base::Result<Base::StringView> targetPath =
            AnimationAttachedString(
                timeline,
                MediaAnimation::Storyboard::TargetPropertyProperty);
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
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                ::Aero::DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Storyboard target name does not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<::Aero::DependencyObject&>(*targetObject);
        Base::Result<ResolvedAnimationProperty> property =
            ResolveAnimationProperty(target, targetPath.Value());
        if (!property) return property.GetStatus();
        ::Aero::DependencyObject& propertyTarget =
            *property.Value().target;
        const Meta::DependencyPropertyHandle propertyHandle =
            property.Value().property;

        const Meta::TypeId type = timeline.RuntimeType();
        if (type == MediaAnimation::DoubleAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::DoubleAnimation&>(timeline);
            Aero::Media::Animation::Model::DoubleAnimation runtime =
                Aero::Media::AnimationPrivate::Double(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Aero::Media::Animation::Model::AnimationHandle> started =
                animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                MediaAnimation::DoubleAnimationBase::StaticTypeId())) {
            auto& animation = static_cast<
                MediaAnimation::DoubleAnimationBase&>(timeline);
            Base::Result<Meta::PropertyValue> current =
                propertyTarget.GetValue(propertyHandle);
            if (!current) return current.GetStatus();
            Base::Result<double> origin =
                Meta::ValueCodec<double>::Decode(current.Value());
            if (!origin) return origin.GetStatus();

            Aero::Media::Animation::Model::CustomDoubleAnimation runtime;
            runtime.animation =
                Base::Ref<MediaAnimation::DoubleAnimationBase>::
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
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type == MediaAnimation::ColorAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::ColorAnimation&>(timeline);
            Aero::Media::Animation::Model::ColorAnimation runtime =
                Aero::Media::AnimationPrivate::Color(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Aero::Media::Animation::Model::AnimationHandle> started =
                animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::PointAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    MediaAnimation::PointAnimation&>(
                        timeline);
            Aero::Media::Animation::Model::PointAnimation runtime =
                Aero::Media::AnimationPrivate::Point(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::RectAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    MediaAnimation::RectAnimation&>(
                        timeline);
            Aero::Media::Animation::Model::RectAnimation runtime =
                Aero::Media::AnimationPrivate::Rect(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::ThicknessAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    MediaAnimation::ThicknessAnimation&>(
                        timeline);
            Aero::Media::Animation::Model::ThicknessAnimation runtime =
                Aero::Media::AnimationPrivate::Thickness(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::Int16Animation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::Int16Animation&>(timeline);
            Aero::Media::Animation::Model::IntegerAnimation runtime =
                Aero::Media::AnimationPrivate::Integer16(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            MediaAnimation::Int32Animation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::Int32Animation&>(timeline);
            Aero::Media::Animation::Model::IntegerAnimation runtime =
                Aero::Media::AnimationPrivate::Integer32(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            MediaAnimation::Int64Animation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::Int64Animation&>(timeline);
            Aero::Media::Animation::Model::IntegerAnimation runtime =
                Aero::Media::AnimationPrivate::Integer64(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            MediaAnimation::SizeAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::SizeAnimation&>(timeline);
            Aero::Media::Animation::Model::SizeAnimation runtime =
                Aero::Media::AnimationPrivate::Size(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            MediaAnimation::MatrixAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::MatrixAnimation&>(timeline);
            Aero::Media::Animation::Model::MatrixAnimation runtime =
                Aero::Media::AnimationPrivate::Matrix(animation);
            runtime.timing =
                EffectiveTimelineTiming(animation, inherited);
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            MediaAnimation::DoubleAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::DoubleAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Aero::Media::Animation::Model::DoubleKeyFrame> frames(allocator);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<MediaAnimation::DoubleKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.PushBack(Aero::Media::AnimationPrivate::DoubleFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
                if (!appended) return appended.GetStatus();
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size(); ++index) {
                Aero::Media::Animation::Model::DoubleKeyFrame current =
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
            Aero::Media::Animation::Model::DoubleKeyFrameAnimation runtime;
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
            Base::Result<Aero::Media::Animation::Model::AnimationHandle> started =
                animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::ColorAnimationUsingKeyFrames::
                StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ColorAnimationUsingKeyFrames&>(
                    timeline);
            Base::Vector<Aero::Media::Animation::Model::ColorKeyFrame>
                frames(allocator);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<
                     MediaAnimation::ColorKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.PushBack(
                        Aero::Media::AnimationPrivate::ColorFrame(
                            *frame,
                            schedule.duration,
                            keyIndex,
                            schedule.count));
                ++keyIndex;
                if (!appended) {
                    return appended.GetStatus();
                }
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size();
                 ++index) {
                Aero::Media::Animation::Model::ColorKeyFrame current =
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
            Aero::Media::Animation::Model::ColorKeyFrameAnimation
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
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::PointAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::PointAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Aero::Media::Animation::Model::PointKeyFrame> frames(
                allocator);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<MediaAnimation::PointKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended = frames.PushBack(
                    Aero::Media::AnimationPrivate::PointFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
                if (!appended) return appended.GetStatus();
            }
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Aero::Media::Animation::Model::PointKeyFrame current =
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
            Aero::Media::Animation::Model::PointKeyFrameAnimation runtime;
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
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            MediaAnimation::ThicknessAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ThicknessAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Aero::Media::Animation::Model::ThicknessKeyFrame>
                frames(allocator);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<MediaAnimation::ThicknessKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended = frames.PushBack(
                    Aero::Media::AnimationPrivate::ThicknessFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
                if (!appended) return appended.GetStatus();
            }
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Aero::Media::Animation::Model::ThicknessKeyFrame current =
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
            Aero::Media::Animation::Model::ThicknessKeyFrameAnimation runtime;
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
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }

            auto startIntegerKeyFrames =
            [&](Aero::Media::Animation::Model::IntegerAnimationWidth width,
                auto&& collect)
                -> Base::Result<std::uint32_t> {
            Base::Vector<Aero::Media::Animation::Model::IntegerKeyFrame>
                frames(allocator);
            Base::Result<void> collected = collect(frames);
            if (!collected) return collected.GetStatus();
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Aero::Media::Animation::Model::IntegerKeyFrame current =
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
            Aero::Media::Animation::Model::IntegerKeyFrameAnimation runtime;
            runtime.width = width;
            if (width ==
                Aero::Media::Animation::Model::IntegerAnimationWidth::Int16) {
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
                Aero::Media::Animation::Model::IntegerAnimationWidth::Int64) {
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
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        };

        if (type ==
            MediaAnimation::Int16AnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::Int16AnimationUsingKeyFrames&>(timeline);
            return startIntegerKeyFrames(
                Aero::Media::Animation::Model::IntegerAnimationWidth::Int16,
                [&](auto& frames) -> Base::Result<void> {
                    const auto schedule = MakeKeyframeSchedule(
                        animation,
                        EffectiveTimelineTiming(
                            animation, inherited).durationMicroseconds);
                    std::uint32_t keyIndex = 0U;
                    for (const Base::Ref<MediaAnimation::Int16KeyFrame>& frame :
                         animation.GetKeyFrames()) {
                        if (!frame) continue;
                        Base::Result<void> appended = frames.PushBack(
                            Aero::Media::AnimationPrivate::IntegerFrame(
                                *frame,
                                schedule.duration,
                                keyIndex,
                                schedule.count));
                        ++keyIndex;
                        if (!appended) return appended.GetStatus();
                    }
                    return {};
                });
        }
        if (type ==
            MediaAnimation::Int32AnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::Int32AnimationUsingKeyFrames&>(timeline);
            return startIntegerKeyFrames(
                Aero::Media::Animation::Model::IntegerAnimationWidth::Int32,
                [&](auto& frames) -> Base::Result<void> {
                    const auto schedule = MakeKeyframeSchedule(
                        animation,
                        EffectiveTimelineTiming(
                            animation, inherited).durationMicroseconds);
                    std::uint32_t keyIndex = 0U;
                    for (const Base::Ref<MediaAnimation::Int32KeyFrame>& frame :
                         animation.GetKeyFrames()) {
                        if (!frame) continue;
                        Base::Result<void> appended = frames.PushBack(
                            Aero::Media::AnimationPrivate::IntegerFrame(
                                *frame,
                                schedule.duration,
                                keyIndex,
                                schedule.count));
                        ++keyIndex;
                        if (!appended) return appended.GetStatus();
                    }
                    return {};
                });
        }
        if (type ==
            MediaAnimation::Int64AnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::Int64AnimationUsingKeyFrames&>(timeline);
            return startIntegerKeyFrames(
                Aero::Media::Animation::Model::IntegerAnimationWidth::Int64,
                [&](auto& frames) -> Base::Result<void> {
                    const auto schedule = MakeKeyframeSchedule(
                        animation,
                        EffectiveTimelineTiming(
                            animation, inherited).durationMicroseconds);
                    std::uint32_t keyIndex = 0U;
                    for (const Base::Ref<MediaAnimation::Int64KeyFrame>& frame :
                         animation.GetKeyFrames()) {
                        if (!frame) continue;
                        Base::Result<void> appended = frames.PushBack(
                            Aero::Media::AnimationPrivate::IntegerFrame(
                                *frame,
                                schedule.duration,
                                keyIndex,
                                schedule.count));
                        ++keyIndex;
                        if (!appended) return appended.GetStatus();
                    }
                    return {};
                });
        }
        if (type ==
            MediaAnimation::SizeAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::SizeAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Aero::Media::Animation::Model::SizeKeyFrame>
                frames(allocator);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<MediaAnimation::SizeKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended = frames.PushBack(
                    Aero::Media::AnimationPrivate::SizeFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
                if (!appended) return appended.GetStatus();
            }
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Aero::Media::Animation::Model::SizeKeyFrame current =
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
            Aero::Media::Animation::Model::SizeKeyFrameAnimation runtime;
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
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }
        if (type ==
            MediaAnimation::MatrixAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::MatrixAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Aero::Media::Animation::Model::MatrixKeyFrame>
                frames(allocator);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(animation, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<MediaAnimation::MatrixKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended = frames.PushBack(
                    Aero::Media::AnimationPrivate::MatrixFrame(
                        *frame,
                        schedule.duration,
                        keyIndex,
                        schedule.count));
                ++keyIndex;
                if (!appended) return appended.GetStatus();
            }
            for (std::uint32_t index = 1U; index < frames.Size(); ++index) {
                Aero::Media::Animation::Model::MatrixKeyFrame current =
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
            Aero::Media::Animation::Model::MatrixKeyFrameAnimation runtime;
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
            Base::Result<Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started), retainedHandles);
        }

        Base::Vector<Aero::Media::Animation::Model::DiscreteAnimationKeyFrame>
            frames(allocator);
        if (type ==
            MediaAnimation::BooleanAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::BooleanAnimationUsingKeyFrames&>(timeline);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(timeline, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<
                     MediaAnimation::BooleanKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    Aero::Media::AnimationPrivate::ResolveKeyTime(
                        frame->GetKeyTime(),
                        schedule.duration,
                        keyIndex,
                        schedule.count);
                ++keyIndex;
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<bool>::Encode(frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value = std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.PushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else if (type ==
            MediaAnimation::ObjectAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ObjectAnimationUsingKeyFrames&>(timeline);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(timeline, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<
                     MediaAnimation::ObjectKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    Aero::Media::AnimationPrivate::ResolveKeyTime(
                        frame->GetKeyTime(),
                        schedule.duration,
                        keyIndex,
                        schedule.count);
                ++keyIndex;
                runtime.value = frame->GetValue();
                const Meta::DependencyProperty* targetProperty =
                    propertyTarget.PropertyRegistry().Find(
                        propertyHandle);
                if (targetProperty != nullptr &&
                    runtime.value.IsNullObject() &&
                    runtime.value.Type() !=
                        targetProperty->ValueType()) {
                    runtime.value =
                        Meta::PropertyValue::NullObject(
                            targetProperty->ValueType());
                }
                Base::Result<void> appended =
                    frames.PushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else if (type ==
            MediaAnimation::StringAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::StringAnimationUsingKeyFrames&>(timeline);
            const auto schedule = MakeKeyframeSchedule(
                animation,
                EffectiveTimelineTiming(timeline, inherited).durationMicroseconds);
            std::uint32_t keyIndex = 0U;
            for (const Base::Ref<
                     MediaAnimation::StringKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    Aero::Media::AnimationPrivate::ResolveKeyTime(
                        frame->GetKeyTime(),
                        schedule.duration,
                        keyIndex,
                        schedule.count);
                ++keyIndex;
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<Base::String>::Encode(frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value = std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.PushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Storyboard contains an unsupported Timeline type");
        }
        for (std::uint32_t index = 1U;
             index < frames.Size(); ++index) {
            Aero::Media::Animation::Model::DiscreteAnimationKeyFrame current =
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
        Aero::Media::Animation::Model::DiscreteAnimation runtime;
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
        Base::Result<Aero::Media::Animation::Model::AnimationHandle> started =
            animations->Begin(
                propertyTarget, propertyHandle, runtime);
        return RetainStartedAnimation(
            std::move(started),
            retainedHandles);
    }

} // namespace Aero
