#include "gui/ViewState.hpp"
#include "gui/media/StoryboardHostCommon.hpp"
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
      storyboardSessions(owner.allocator),
      storyboardCompletionSessions(owner.allocator),
      storyboardCompletedSubscriptions(owner.allocator),
      animationEventSubscriptions(owner.allocator),
      pendingLoadedTriggers(owner.allocator) {}

void StoryboardHost::Bind() noexcept {
    // Services are read on demand from the owning ViewState / ElementTree hub.
}

Base::IAllocator* StoryboardHost::Allocator() const noexcept {
    return view != nullptr ? view->allocator : nullptr;
}

::Aero::Meta::Registry* StoryboardHost::Metadata() const noexcept {
    return view != nullptr ? view->metadata : nullptr;
}

Aero::AnimationEngine* StoryboardHost::Animations() const noexcept {
    return view != nullptr ? view->Animations() : nullptr;
}

Aero::InputRouter* StoryboardHost::Input() const noexcept {
    return view != nullptr ? view->Input() : nullptr;
}

Aero::StyleEngine* StoryboardHost::Styles() const noexcept {
    return view != nullptr ? view->Styles() : nullptr;
}

InteractivityEngine* StoryboardHost::Interactivity() const noexcept {
    return view != nullptr ? view->interactivity : nullptr;
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
            Aero::Media::Animation::Timing(storyboard);
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
            Aero::Media::Animation::Timing(timeline);
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
                Aero::Media::Animation::
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
                    Animations()->Remove(
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
        if (Metadata()->Types().IsDerivedFrom(
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
                    !Metadata()->Types().IsDerivedFrom(
                        authoredBehavior->RuntimeType(),
                        Interactivity::Behavior::StaticTypeId())) {
                    continue;
                }
                Base::Result<void> attached = Interactivity()->AttachBehavior(
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
                    !Metadata()->Types().IsDerivedFrom(
                        behaviorPrototype->RuntimeType(),
                        Interactivity::Behavior::StaticTypeId())) {
                    continue;
                }
                Base::Result<void> attached = Interactivity()->AttachBehavior(
                    static_cast<const Interactivity::Behavior&>(
                        *behaviorPrototype),
                    *element,
                    names,
                    true);
                if (!attached) return attached.GetStatus();
            }
            if (Input() != nullptr) {
                for (const Base::Ref<Input::InputBinding>& binding :
                     element->GetInputBindings()) {
                    if (!binding) continue;
                    Base::Result<Input::InputBindingHandle> added =
                        Input()->AddInputBinding(*element, binding);
                    if (!added) return added.GetStatus();
                }
                for (const Base::Ref<Input::CommandBinding>& binding :
                     element->GetCommandBindings()) {
                    if (!binding) continue;
                    Base::Result<Input::CommandBindingHandle> added =
                        Input()->AddCommandBinding(*element, *binding);
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
                        Interactivity()->StartDataTemplateTriggers(
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
                        Interactivity()->StartPropertyChangedTrigger(
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
                    Base::Result<bool> started = Interactivity()->StartKeyTrigger(
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
                        Interactivity()->StartInteractionDataTrigger(
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
                    Base::Result<bool> started = Interactivity()->StartPropertyChangedTrigger(
                        static_cast<Aero::Interactivity::PropertyChangedTrigger&>(
                            *authored),
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Interactivity::KeyTrigger::StaticTypeId()) {
                    Base::Result<bool> started = Interactivity()->StartKeyTrigger(
                        static_cast<Aero::Interactivity::KeyTrigger&>(*authored),
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::DataTrigger::StaticTypeId()) {
                    Base::Result<bool> started =
                        Interactivity()->StartInteractionDataTrigger(
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
            if (Styles() != nullptr) {
                const Aero::Style* applied = Styles()->AppliedStyle(*element);
                if (applied != nullptr) {
                    for (const Base::Ref<Aero::TriggerBase>& authored :
                         applied->GetAuthoredTriggers()) {
                        if (!authored ||
                            !Metadata()->Types().IsDerivedFrom(
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
                view->Events() != nullptr) {
                Aero::RoutedEventArgs loadedArgs;
                static_cast<void>(view->Events()->RaiseEvent(
                    *element,
                    FrameworkElement::LoadedEvent.Handle(),
                    &loadedArgs));
            }
            if (Metadata()->Types().IsDerivedFrom(
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
