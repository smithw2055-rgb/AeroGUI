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


StoryboardHost::StoryboardHost(ViewState& owner) noexcept
    : view(&owner),
      allocator(owner.allocator),
      storyboardSessions(owner.allocator),
      storyboardCompletionSessions(owner.allocator),
      storyboardCompletedSubscriptions(owner.allocator),
      animationEventSubscriptions(owner.allocator),
      pendingLoadedTriggers(owner.allocator) {}

void StoryboardHost::Bind() noexcept {
    allocator = view->allocator;
    metadata = view->metadata;
    animations = view->animations;
    input = view->input;
    styles = view->styles;
    interactivity = view->interactivity;
}


StoryboardHost::StoryboardSession::StoryboardSession(
            Base::IAllocator* allocator) noexcept
    : handles(allocator) {}

StoryboardHost::StoryboardCompletionSession::StoryboardCompletionSession(
            Base::IAllocator* allocator) noexcept
    : handles(allocator) {}

Base::Result<Base::StringView> StoryboardHost::AnimationAttachedString(
        MediaAnimation::Timeline& timeline,
        Meta::DependencyPropertyHandle property) noexcept {
        Base::Result<Meta::PropertyValue> value =
            timeline.GetValue(property);
        if (!value) return value.GetStatus();
        if (value.Value().Kind() != Meta::ValueKind::String) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Storyboard attached property must be a string");
        }
        return value.Value().AsString();
    }



StoryboardHost::StoryboardTimingState StoryboardHost::ComposeStoryboardTiming(
        const StoryboardTimingState* inherited,
        const MediaAnimation::Timeline& storyboard,
        bool preservesChildDuration) noexcept {
        StoryboardTimingState result =
            inherited != nullptr
            ? *inherited
            : StoryboardTimingState{};
        const Aero::Media::Animation::Model::TimelineTiming authored =
            Aero::Media::AnimationPrivate::Timing(storyboard);
        if (UINT64_MAX - result.beginTimeMicroseconds <
            authored.beginTimeMicroseconds) {
            result.beginTimeMicroseconds = UINT64_MAX;
        } else {
            result.beginTimeMicroseconds +=
                authored.beginTimeMicroseconds;
        }
        if (!storyboard.GetDuration().IsAutomatic()) {
            result.durationMicroseconds =
                authored.durationMicroseconds;
            result.hasDuration = true;
            result.preservesChildDuration =
                preservesChildDuration;
        }
        if (!storyboard.ReadLocalValue(
                MediaAnimation::Timeline::RepeatBehaviorProperty).IsUnset()) {
            result.repeat = authored.repeat;
            result.hasRepeat = true;
        }
        result.speedRatio *= authored.speedRatio;
        result.autoReverse =
            result.autoReverse || authored.autoReverse;
        return result;
    }

Aero::Media::Animation::Model::TimelineTiming StoryboardHost::EffectiveTimelineTiming(
        const MediaAnimation::Timeline& timeline,
        const StoryboardTimingState* inherited) noexcept {
        Aero::Media::Animation::Model::TimelineTiming result =
            Aero::Media::AnimationPrivate::Timing(timeline);
        if (inherited == nullptr) return result;
        if (UINT64_MAX - inherited->beginTimeMicroseconds <
            result.beginTimeMicroseconds) {
            result.beginTimeMicroseconds = UINT64_MAX;
        } else {
            result.beginTimeMicroseconds +=
                inherited->beginTimeMicroseconds;
        }
        if (inherited->hasDuration &&
            !inherited->preservesChildDuration) {
            result.durationMicroseconds =
                inherited->durationMicroseconds;
        } else if (inherited->hasDuration &&
                   inherited->preservesChildDuration) {
            const Aero::Media::Animation::AnimationTime childBegin =
                Aero::Media::AnimationPrivate::
                    Timing(timeline).beginTimeMicroseconds;
            const Aero::Media::Animation::AnimationTime available =
                childBegin >= inherited->durationMicroseconds
                ? 0U
                : inherited->durationMicroseconds - childBegin;
            if (result.durationMicroseconds == 0U) {
                result.durationMicroseconds = available;
                result.repeat =
                    Aero::Media::Animation::Model::
                        RepeatBehavior::Once();
            } else {
                const long double cycle =
                    static_cast<long double>(
                        result.durationMicroseconds) *
                    (result.autoReverse ? 2.0L : 1.0L);
                const double maximumCount =
                    cycle > 0.0L
                    ? static_cast<double>(
                        static_cast<long double>(available) /
                        cycle)
                    : 1.0;
                if (available == 0U) {
                    result.durationMicroseconds = 0U;
                    result.repeat =
                        Aero::Media::Animation::Model::
                            RepeatBehavior::Once();
                } else if (result.repeat.forever ||
                           result.repeat.count >
                               maximumCount) {
                    result.repeat =
                        Aero::Media::Animation::Model::
                            RepeatBehavior::Count(
                                std::max(
                                    maximumCount,
                                    1.0e-9));
                }
            }
        }
        if (inherited->hasRepeat) {
            result.repeat = inherited->repeat;
        }
        result.speedRatio *= inherited->speedRatio;
        result.autoReverse =
            result.autoReverse || inherited->autoReverse;
        return result;
    }

Base::Result<std::uint32_t>
 StoryboardHost::RetainStartedAnimation(
        Base::Result<
            Aero::Media::Animation::Model::AnimationHandle>
            started,
        Base::Vector<
            Aero::Media::Animation::Model::AnimationHandle>*
            retainedHandles) noexcept {
        if (!started) {
            return started.GetStatus();
        }
        if (retainedHandles != nullptr) {
            Base::Result<void> retained =
                retainedHandles->PushBack(
                    started.Value());
            if (!retained) {
                static_cast<void>(
                    animations->Remove(
                        started.Value()));
                return retained.GetStatus();
            }
        }
        return std::uint32_t{1U};
    }



Base::Result<std::uint32_t> StoryboardHost::StartContentElementAnimations(
        Aero::FrameworkContentElement& content,
        Aero::FrameworkElement& actionOwner,
        const Aero::NameScope* names) noexcept {
        std::uint32_t count = 0U;
        for (const Base::Ref<Base::Object>& authored :
             AeroGuiInternal::AuthoredTriggers(
                 content)) {
            if (!authored || authored->RuntimeType() !=
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                continue;
            }
            Base::Result<bool> started = StartEventTrigger(
                static_cast<MediaAnimation::EventTrigger&>(*authored),
                content,
                actionOwner,
                names);
            if (started && started.Value()) ++count;
        }
        if (metadata->Types().IsDerivedFrom(
                content.RuntimeType(),
                Documents::Span::StaticTypeId())) {
            const Documents::InlineCollectionView inlines =
                static_cast<const Documents::Span&>(content).GetInlines();
            for (std::uint32_t index = 0U;
                 index < inlines.GetCount(); ++index) {
                const Documents::Inline* child = inlines.GetItem(index);
                if (child == nullptr) continue;
                Base::Result<std::uint32_t> nested =
                    StartContentElementAnimations(
                        const_cast<Documents::Inline&>(*child),
                        actionOwner,
                        names);
                if (!nested) return nested.GetStatus();
                if (count > UINT32_MAX - nested.Value()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Content trigger count overflow");
                }
                count += nested.Value();
            }
        }
        return count;
    }

Base::Result<std::uint32_t> StoryboardHost::StartLoadedAnimations(
        Aero::Media::Visual* visual,
        const Aero::NameScope* names) noexcept {
        if (visual == nullptr) return std::uint32_t{0U};
        std::uint32_t count = 0U;
        Aero::FrameworkElement* element =
            ::Aero::TryCast<::Aero::FrameworkElement>(visual);
        if (element != nullptr) {
            for (const Base::Ref<Base::Object>& authoredBehavior :
                 AeroGuiInternal::AuthoredBehaviors(
                     *element)) {
                if (!authoredBehavior ||
                    !metadata->Types().IsDerivedFrom(
                        authoredBehavior->RuntimeType(),
                        Interactivity::Behavior::StaticTypeId())) {
                    continue;
                }
                Base::Result<void> attached = interactivity->AttachBehavior(
                    static_cast<const Interactivity::Behavior&>(
                        *authoredBehavior),
                    *element,
                    names,
                    false);
                if (!attached) return attached.GetStatus();
            }
            for (const Base::Ref<Base::Object>& behaviorPrototype :
                 AeroGuiInternal::StyleBehaviorPrototypes(
                     *element)) {
                if (!behaviorPrototype ||
                    !metadata->Types().IsDerivedFrom(
                        behaviorPrototype->RuntimeType(),
                        Interactivity::Behavior::StaticTypeId())) {
                    continue;
                }
                Base::Result<void> attached = interactivity->AttachBehavior(
                    static_cast<const Interactivity::Behavior&>(
                        *behaviorPrototype),
                    *element,
                    names,
                    true);
                if (!attached) return attached.GetStatus();
            }
            if (input != nullptr) {
                for (const Base::Ref<Input::InputBinding>& binding :
                     element->GetInputBindings()) {
                    if (!binding) continue;
                    Base::Result<Input::InputBindingHandle> added =
                        input->AddInputBinding(*element, binding);
                    if (!added) return added.GetStatus();
                }
                for (const Base::Ref<Input::CommandBinding>& binding :
                     element->GetCommandBindings()) {
                    if (!binding) continue;
                    Base::Result<Input::CommandBindingHandle> added =
                        input->AddCommandBinding(*element, *binding);
                    if (!added) return added.GetStatus();
                }
            }
            for (const Base::Ref<Base::Object>& authored :
                 AeroGuiInternal::AuthoredTriggers(*element)) {
                if (!authored) {
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Controls::DataTemplateTriggerState::
                            StaticTypeId()) {
                    Base::Result<std::uint32_t> started =
                        interactivity->StartDataTemplateTriggers(
                            static_cast<
                                Aero::Controls::DataTemplateTriggerState&>(
                                        *authored));
                    if (!started) {
                        return started.GetStatus();
                    }
                    if (count >
                        UINT32_MAX - started.Value()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::OutOfRange,
                            "DataTemplate Trigger subscription count overflow");
                    }
                    count += started.Value();
                    continue;
                }
                if (authored->RuntimeType() ==
                    MediaAnimation::StoryboardCompletedTrigger::
                        StaticTypeId()) {
                    Base::Result<void> retained =
                        storyboardCompletedSubscriptions.
                            PushBack({
                                static_cast<
                                    MediaAnimation::
                                        StoryboardCompletedTrigger*>(
                                            authored.Get()),
                                element,
                                names});
                    if (!retained) {
                        return retained.GetStatus();
                    }
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Interactivity::PropertyChangedTrigger::
                        StaticTypeId()) {
                    Base::Result<bool> started =
                        interactivity->StartPropertyChangedTrigger(
                            static_cast<
                                Aero::Interactivity::PropertyChangedTrigger&>(
                                    *authored),
                            *element,
                            names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Interactivity::KeyTrigger::StaticTypeId()) {
                    Base::Result<bool> started = interactivity->StartKeyTrigger(
                        static_cast<Aero::Interactivity::KeyTrigger&>(
                            *authored),
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::DataTrigger::StaticTypeId()) {
                    Base::Result<bool> started =
                        interactivity->StartInteractionDataTrigger(
                            static_cast<Aero::DataTrigger&>(*authored),
                            *element,
                            names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() !=
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                    continue;
                }
                Base::Result<bool> started = StartEventTrigger(
                    static_cast<MediaAnimation::EventTrigger&>(*authored),
                    *element,
                    *element,
                    names);
                if (started && started.Value()) ++count;
            }
            for (const Base::Ref<Base::Object>& authored :
                 AeroGuiInternal::StyleTriggerPrototypes(
                     *element)) {
                if (!authored) continue;
                if (authored->RuntimeType() ==
                    MediaAnimation::StoryboardCompletedTrigger::StaticTypeId()) {
                    Base::Result<void> retained =
                        storyboardCompletedSubscriptions.PushBack({
                            static_cast<MediaAnimation::StoryboardCompletedTrigger*>(
                                authored.Get()),
                            element,
                            names});
                    if (!retained) return retained.GetStatus();
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Interactivity::PropertyChangedTrigger::StaticTypeId()) {
                    Base::Result<bool> started = interactivity->StartPropertyChangedTrigger(
                        static_cast<Aero::Interactivity::PropertyChangedTrigger&>(
                            *authored),
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Interactivity::KeyTrigger::StaticTypeId()) {
                    Base::Result<bool> started = interactivity->StartKeyTrigger(
                        static_cast<Aero::Interactivity::KeyTrigger&>(*authored),
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::DataTrigger::StaticTypeId()) {
                    Base::Result<bool> started =
                        interactivity->StartInteractionDataTrigger(
                            static_cast<Aero::DataTrigger&>(*authored),
                            *element,
                            names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartEventTrigger(
                        static_cast<MediaAnimation::EventTrigger&>(*authored),
                        *element,
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                }
            }
            if (styles != nullptr) {
                const Aero::Style* applied = styles->AppliedStyle(*element);
                if (applied != nullptr) {
                    for (const Base::Ref<Aero::TriggerBase>& authored :
                         applied->GetAuthoredTriggers()) {
                        if (!authored ||
                            !metadata->Types().IsDerivedFrom(
                                authored->RuntimeType(),
                                MediaAnimation::EventTrigger::StaticTypeId())) {
                            continue;
                        }
                        Base::Result<bool> started = StartEventTrigger(
                            static_cast<MediaAnimation::EventTrigger&>(
                                *authored),
                            *element,
                            *element,
                            names);
                        if (started && started.Value()) ++count;
                    }
                }
            }
            if (element->GetIsLoaded() && view != nullptr &&
                view->events != nullptr) {
                Aero::RoutedEventArgs loadedArgs;
                static_cast<void>(view->events->RaiseEvent(
                    *element,
                    FrameworkElement::LoadedEvent.Handle(),
                    &loadedArgs));
            }
            if (metadata->Types().IsDerivedFrom(
                    element->RuntimeType(),
                    Controls::TextBlock::StaticTypeId())) {
                const Documents::InlineCollectionView inlines =
                    static_cast<const Controls::TextBlock&>(*element)
                        .GetInlines();
                for (std::uint32_t index = 0U;
                     index < inlines.GetCount(); ++index) {
                    const Documents::Inline* inlineValue =
                        inlines.GetItem(index);
                    if (inlineValue == nullptr) continue;
                    Base::Result<std::uint32_t> started =
                        StartContentElementAnimations(
                            const_cast<Documents::Inline&>(*inlineValue),
                            *element,
                            names);
                    if (!started) return started.GetStatus();
                    if (count > UINT32_MAX - started.Value()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::OutOfRange,
                            "Inline trigger count overflow");
                    }
                    count += started.Value();
                }
            }
        }
        for (Aero::Media::Visual* child :
             AeroGuiInternal::RenderChildren(*visual)) {
            Base::Result<std::uint32_t> started =
                StartLoadedAnimations(child, names);
            if (!started) return started.GetStatus();
            if (count > UINT32_MAX - started.Value()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Loaded animation count overflow");
            }
            count += started.Value();
        }
        return count;
    }


} // namespace Aero
