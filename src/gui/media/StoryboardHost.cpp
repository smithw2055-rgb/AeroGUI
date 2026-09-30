#include "gui/ViewFrame.hpp"
#include "gui/templates/DataTemplateTriggerInstance.hpp"
#include "gui/media/StoryboardHost.hpp"
#include "gui/core/EventRouter.hpp"
#include <Aero/CommandBinding.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Documents.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cstdlib>

namespace Aero {

using namespace Media::Animation;


StoryboardHost::StoryboardHost(ViewFrame& owner) noexcept
    : view(&owner),
      storyboardSessions(owner.allocator),
      storyboardCompletionSessions(owner.allocator),
      storyboardCompletedSubscriptions(owner.allocator),
      animationEventSubscriptions(owner.allocator),
      pendingLoadedTriggers(owner.allocator) {}

void StoryboardHost::Bind() noexcept {
    // Services are read on demand from the owning ViewFrame / ElementTree hub.
}

Base::IAllocator* StoryboardHost::Allocator() const noexcept {
    return view != nullptr ? view->allocator : nullptr;
}

Meta::Registry* StoryboardHost::Metadata() const noexcept {
    return view != nullptr ? view->metadata : nullptr;
}

AnimationEngine* StoryboardHost::Animations() const noexcept {
    return view != nullptr ? view->Animations() : nullptr;
}

InputRouter* StoryboardHost::Input() const noexcept {
    return view != nullptr ? view->Input() : nullptr;
}

StyleEngine* StoryboardHost::Styles() const noexcept {
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
        Timeline& timeline,
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
        const Timeline& storyboard,
        bool preservesChildDuration) noexcept {
        StoryboardTimingState result =
            inherited != nullptr
            ? *inherited
            : StoryboardTimingState{};
        const Model::TimelineTiming authored =
            Media::Animation::Timing(storyboard);
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
                Timeline::RepeatBehaviorProperty).IsUnset()) {
            result.repeat = authored.repeat;
            result.hasRepeat = true;
        }
        result.speedRatio *= authored.speedRatio;
        result.autoReverse =
            result.autoReverse || authored.autoReverse;
        return result;
    }

Model::TimelineTiming StoryboardHost::EffectiveTimelineTiming(
        const Timeline& timeline,
        const StoryboardTimingState* inherited) noexcept {
        Model::TimelineTiming result =
            Media::Animation::Timing(timeline);
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
            const Media::Animation::AnimationTime childBegin =
                Media::Animation::
                    Timing(timeline).beginTimeMicroseconds;
            const Media::Animation::AnimationTime available =
                childBegin >= inherited->durationMicroseconds
                ? 0U
                : inherited->durationMicroseconds - childBegin;
            if (result.durationMicroseconds == 0U) {
                result.durationMicroseconds = available;
                result.repeat =
                    Model::
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
                        Model::
                            RepeatBehavior::Once();
                } else if (result.repeat.forever ||
                           result.repeat.count >
                               maximumCount) {
                    result.repeat =
                        Model::
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
            Model::AnimationHandle>
            started,
        Base::Vector<
            Model::AnimationHandle>*
            retainedHandles) noexcept {
        if (!started) {
            return started.GetStatus();
        }
        if (retainedHandles != nullptr) {
            retainedHandles->PushBack(
                started.Value());
        }
        return std::uint32_t{1U};
    }



Base::Result<std::uint32_t> StoryboardHost::StartContentElementAnimations(
        FrameworkContentElement& content,
        FrameworkElement& actionOwner,
        const NameScope* names) noexcept {
        std::uint32_t count = 0U;
        for (const Base::Ref<Base::Object>& authored :
             (content).AuthoredTriggers()) {
            if (!authored || authored->RuntimeType() !=
                    EventTrigger::StaticTypeId()) {
                continue;
            }
            Base::Result<bool> started = StartEventTrigger(
                static_cast<EventTrigger&>(*authored),
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
        Media::Visual* visual,
        const NameScope* names) noexcept {
        if (visual == nullptr) return std::uint32_t{0U};
        std::uint32_t count = 0U;
        FrameworkElement* element =
            TryCast<FrameworkElement>(visual);
        if (element != nullptr) {
            for (const Base::Ref<Base::Object>& authoredBehavior :
                 (*element).AuthoredBehaviors()) {
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
                 (*element).StyleBehaviorPrototypes()) {
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
                 (*element).AuthoredTriggers()) {
                if (!authored) {
                    continue;
                }
                if (authored->RuntimeType() ==
                    Controls::DataTemplateTriggerInstance::
                            StaticTypeId()) {
                    Base::Result<std::uint32_t> started =
                        Interactivity()->StartDataTemplateTriggers(
                            static_cast<
                                Controls::DataTemplateTriggerInstance&>(
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
                    StoryboardCompletedTrigger::
                        StaticTypeId()) {
                    storyboardCompletedSubscriptions.
                        PushBack({
                            static_cast<
                                StoryboardCompletedTrigger*>(
                                        authored.Get()),
                            element,
                            names});
                    continue;
                }
                if (authored->RuntimeType() ==
                    Interactivity::PropertyChangedTrigger::
                        StaticTypeId()) {
                    Base::Result<bool> started =
                        Interactivity()->StartPropertyChangedTrigger(
                            static_cast<
                                Interactivity::PropertyChangedTrigger&>(
                                    *authored),
                            *element,
                            names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Interactivity::KeyTrigger::StaticTypeId()) {
                    Base::Result<bool> started = Interactivity()->StartKeyTrigger(
                        static_cast<Interactivity::KeyTrigger&>(
                            *authored),
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    DataTrigger::StaticTypeId()) {
                    Base::Result<bool> started =
                        Interactivity()->StartInteractionDataTrigger(
                            static_cast<DataTrigger&>(*authored),
                            *element,
                            names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() !=
                    EventTrigger::StaticTypeId()) {
                    continue;
                }
                Base::Result<bool> started = StartEventTrigger(
                    static_cast<EventTrigger&>(*authored),
                    *element,
                    *element,
                    names);
                if (started && started.Value()) ++count;
            }
            for (const Base::Ref<Base::Object>& authored :
                 (*element).StyleTriggerPrototypes()) {
                if (!authored) continue;
                if (authored->RuntimeType() ==
                    StoryboardCompletedTrigger::StaticTypeId()) {
                    storyboardCompletedSubscriptions.PushBack({
                            static_cast<StoryboardCompletedTrigger*>(
                                authored.Get()),
                            element,
                            names});
                    continue;
                }
                if (authored->RuntimeType() ==
                    Interactivity::PropertyChangedTrigger::StaticTypeId()) {
                    Base::Result<bool> started = Interactivity()->StartPropertyChangedTrigger(
                        static_cast<Interactivity::PropertyChangedTrigger&>(
                            *authored),
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Interactivity::KeyTrigger::StaticTypeId()) {
                    Base::Result<bool> started = Interactivity()->StartKeyTrigger(
                        static_cast<Interactivity::KeyTrigger&>(*authored),
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    DataTrigger::StaticTypeId()) {
                    Base::Result<bool> started =
                        Interactivity()->StartInteractionDataTrigger(
                            static_cast<DataTrigger&>(*authored),
                            *element,
                            names);
                    if (started && started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    EventTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartEventTrigger(
                        static_cast<EventTrigger&>(*authored),
                        *element,
                        *element,
                        names);
                    if (started && started.Value()) ++count;
                }
            }
            if (Styles() != nullptr) {
                const Style* applied = Styles()->AppliedStyle(*element);
                if (applied != nullptr) {
                    for (const Base::Ref<TriggerBase>& authored :
                         applied->GetAuthoredTriggers()) {
                        if (!authored ||
                            !Metadata()->Types().IsDerivedFrom(
                                authored->RuntimeType(),
                                EventTrigger::StaticTypeId())) {
                            continue;
                        }
                        Base::Result<bool> started = StartEventTrigger(
                            static_cast<EventTrigger&>(
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
                RoutedEventArgs loadedArgs;
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
        for (Media::Visual* child :
             (*visual).RenderChildren()) {
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

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
using namespace ::Aero::Meta;
using namespace ::Aero::Threading;
using namespace ::Aero::Input;
using namespace ::Aero::Media;
using namespace ::Aero::Data;
using namespace ::Aero::Interactivity;
    using namespace Interactivity;
    using Media::Animation::BeginStoryboard;
    using Media::Animation::BooleanAnimationUsingKeyFrames;
    using Media::Animation::BooleanKeyFrame;
    using Media::Animation::ColorAnimationUsingKeyFrames;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimationUsingKeyFrames;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::EventTrigger;
    using Media::Animation::Int16AnimationUsingKeyFrames;
    using Media::Animation::Int16KeyFrame;
    using Media::Animation::Int32AnimationUsingKeyFrames;
    using Media::Animation::Int32KeyFrame;
    using Media::Animation::Int64AnimationUsingKeyFrames;
    using Media::Animation::Int64KeyFrame;
    using Media::Animation::MatrixAnimationUsingKeyFrames;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::ObjectAnimationUsingKeyFrames;
    using Media::Animation::ObjectKeyFrame;
    using Media::Animation::PointAnimationUsingKeyFrames;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::SizeAnimationUsingKeyFrames;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::Storyboard;
    using Media::Animation::StoryboardCompletedTrigger;
    using Media::Animation::StringAnimationUsingKeyFrames;
    using Media::Animation::StringKeyFrame;
    using Media::Animation::ThicknessAnimationUsingKeyFrames;
    using Media::Animation::ThicknessKeyFrame;
    using Media::Animation::Timeline;
    using Media::Animation::TimelineGroup;
    using Media::Effect;
    using Media::FontFamily;
    using Media::Geometry;
    using Media::GeometryGroup;
    using Media::PathFigure;
    using Media::PathGeometry;
    using Media::PathSegment;
    using Media::StreamGeometry;
namespace {

void AddStoryboardTimeline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Timeline> retained =
        Base::Ref<Timeline>::FromBorrowed(
            static_cast<Timeline&>(*value));
    static_cast<TimelineGroup&>(owner)
        .AddChild(std::move(retained));
}

void ClearStoryboardTimelines(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TimelineGroup&>(owner).Clear();
    return;
}

} // namespace
} // namespace Aero::MetadataSupport

AERO_DESCRIBE(::Aero::Media::Animation::TimelineGroup) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
    using Media::Animation::ColorAnimation;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimation;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::MatrixAnimation;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::PointAnimation;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::RectAnimation;
    using Media::Animation::RepeatBehavior;
    using Media::Animation::SizeAnimation;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::ThicknessAnimation;
    using Media::Animation::ThicknessKeyFrame;
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<TimelineGroup>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(::Aero::Media::Animation::ParallelTimeline) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
    using Media::Animation::ColorAnimation;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimation;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::MatrixAnimation;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::PointAnimation;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::RectAnimation;
    using Media::Animation::RepeatBehavior;
    using Media::Animation::SizeAnimation;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::ThicknessAnimation;
    using Media::Animation::ThicknessKeyFrame;
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<ParallelTimeline>(context)
            .Content<Timeline>("Children", ContentKind::Collection, &::Aero::MetadataSupport::AddStoryboardTimeline, &::Aero::MetadataSupport::ClearStoryboardTimelines)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::Storyboard) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
    using Media::Animation::ColorAnimation;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimation;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::MatrixAnimation;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::PointAnimation;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::RectAnimation;
    using Media::Animation::RepeatBehavior;
    using Media::Animation::SizeAnimation;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::ThicknessAnimation;
    using Media::Animation::ThicknessKeyFrame;
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<Storyboard>(context)
            .Property(Storyboard::TargetNameProperty, Base::String{})
            .Property(Storyboard::TargetPropertyProperty, Base::String{})
            .Content<Timeline>("Children", ContentKind::Collection, &::Aero::MetadataSupport::AddStoryboardTimeline, &::Aero::MetadataSupport::ClearStoryboardTimelines)
            .Factory();
}
