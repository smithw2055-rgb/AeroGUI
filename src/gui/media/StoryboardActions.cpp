// Storyboard actions and event-trigger subscriptions.
#include "gui/ViewFrame.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/TypeRegistryCore.hpp"
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
#include <Aero/Events.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
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
#include <Aero/CommandBinding.hpp>
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
#include <Aero/Controls/Buttons.hpp>
#include <cctype>
#include <cstdlib>
#include <string_view>

namespace Aero::Media::Animation {

namespace {

StoryboardHost* ResolveAnimationStoryboardHost(FrameworkElement& owner) noexcept {
    ElementTree* tree = ElementTree::Of(owner);
    ViewFrame* view = tree != nullptr ? tree->GetViewState() : nullptr;
    return view != nullptr ? view->Storyboards() : nullptr;
}

void RecordAnimationActionFailure(StoryboardHost* host, const Base::Status& status) noexcept {
    if (host != nullptr) {
        host->activeActionStatus = status;
    }
}

bool TargetsScaleTransform(
    StoryboardHost& host,
    const Timeline& timeline) noexcept {
    if (host.Metadata()->Types().IsDerivedFrom(
            timeline.RuntimeType(), TimelineGroup::StaticTypeId())) {
        const auto& group = static_cast<const TimelineGroup&>(timeline);
        for (const auto& child : group.GetTimelines()) {
            if (child && TargetsScaleTransform(host, *child)) {
                return true;
            }
        }
        return false;
    }
    Base::Result<Base::StringView> targetProp = host.AnimationAttachedString(
        const_cast<Timeline&>(timeline),
        Storyboard::TargetPropertyProperty);
    if (targetProp) {
        std::string_view prop(targetProp.Value().Data(), targetProp.Value().SizeBytes());
        if (prop.find("ScaleX") != std::string_view::npos ||
            prop.find("ScaleY") != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

enum class MediaElementOperation : std::uint8_t {
    Play = 0U,
    Pause,
    Stop,
};

void InvokeMediaElementOperation(
    Interactivity::TriggerAction& action,
    StringView targetName,
    MediaElementOperation operation,
    FrameworkElement& owner) noexcept {
    StoryboardHost* host = ResolveAnimationStoryboardHost(owner);
    if (host == nullptr) return;
    if (host->Animations() == nullptr) {
        RecordAnimationActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard action requires the animation manager"));
        return;
    }
    Base::Result<Base::Object*> resolvedMediaTarget =
        host->ResolveTargetObject(
            {},
            targetName,
            owner,
            host->activeActionDataTemplateContext,
            host->activeActionNames,
            &action);
    Base::Object* targetObject = resolvedMediaTarget ? resolvedMediaTarget.Value() : nullptr;
    if (targetObject == nullptr ||
        !host->Metadata()->Types().IsDerivedFrom(
            targetObject->RuntimeType(),
            Media::MediaElement::StaticTypeId())) {
        RecordAnimationActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "MediaAction TargetName did not resolve to a MediaElement"));
        return;
    }
    auto& media = static_cast<Media::MediaElement&>(*targetObject);
    switch (operation) {
    case MediaElementOperation::Play:
        media.Play();
        break;
    case MediaElementOperation::Pause:
        media.Pause();
        break;
    case MediaElementOperation::Stop:
        media.Stop();
        break;
    }
}

enum class ControllableStoryboardOperation : std::uint8_t {
    Pause = 0U,
    Resume,
    Stop,
    Remove,
    Seek,
};

void InvokeControllableStoryboardOperation(
    ControllableStoryboardAction& control,
    ControllableStoryboardOperation operation,
    AnimationTime seekOffsetMicroseconds,
    FrameworkElement& owner) noexcept {
    StoryboardHost* host = ResolveAnimationStoryboardHost(owner);
    if (host == nullptr) return;
    if (host->Animations() == nullptr) {
        RecordAnimationActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard action requires the animation manager"));
        return;
    }
    std::uint32_t sessionIndex = UINT32_MAX;
    for (std::uint32_t index = 0U; index < host->storyboardSessions.Size(); ++index) {
        if (host->storyboardSessions[index].name.View() == control.GetBeginStoryboardName()) {
            sessionIndex = index;
            break;
        }
    }
    if (sessionIndex == UINT32_MAX) {
        RecordAnimationActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Controllable Storyboard was not started"));
        return;
    }
    StoryboardHost::StoryboardSession& session = host->storyboardSessions[sessionIndex];
    for (Model::AnimationHandle handle : session.handles) {
        switch (operation) {
        case ControllableStoryboardOperation::Pause:
            host->Animations()->Pause(handle);
            break;
        case ControllableStoryboardOperation::Resume:
            host->Animations()->Resume(handle);
            break;
        case ControllableStoryboardOperation::Stop:
            host->Animations()->Stop(handle);
            break;
        case ControllableStoryboardOperation::Remove:
            host->Animations()->Remove(handle);
            break;
        case ControllableStoryboardOperation::Seek:
            host->Animations()->Seek(handle, seekOffsetMicroseconds);
            break;
        }
    }
    if (operation == ControllableStoryboardOperation::Stop ||
        operation == ControllableStoryboardOperation::Remove) {
        host->CancelStoryboardCompletionSessions(session.handles.AsSpan());
    }
    if (operation == ControllableStoryboardOperation::Remove) {
        for (std::uint32_t next = sessionIndex + 1U; next < host->storyboardSessions.Size(); ++next) {
            host->storyboardSessions[next - 1U] = std::move(host->storyboardSessions[next]);
        }
        host->storyboardSessions.PopBack();
    }
}

} // namespace

void BeginStoryboard::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    StoryboardHost* host = ResolveAnimationStoryboardHost(associatedObject);
    if (host == nullptr) return;
    if (host->Animations() == nullptr) {
        RecordAnimationActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard action requires the animation manager"));
        return;
    }
    if (!GetStoryboard()) {
        RecordAnimationActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "BeginStoryboard Storyboard was not resolved"));
        return;
    }
    const bool isRepeatButton = host->Metadata()->Types().IsDerivedFrom(
        associatedObject.RuntimeType(), Controls::Primitives::RepeatButton::StaticTypeId()) ||
        (associatedObject.GetTemplatedParent() != nullptr &&
         host->Metadata()->Types().IsDerivedFrom(
             associatedObject.GetTemplatedParent()->RuntimeType(), Controls::Primitives::RepeatButton::StaticTypeId()));
    if (isRepeatButton && TargetsScaleTransform(*host, *GetStoryboard())) {
        return;
    }
    if (!GetName().Empty()) {
        for (std::uint32_t index = 0U; index < host->storyboardSessions.Size(); ++index) {
            StoryboardHost::StoryboardSession& existing = host->storyboardSessions[index];
            if (existing.name.View() != GetName()) {
                continue;
            }
            host->CancelStoryboardCompletionSessions(existing.handles.AsSpan());
            host->Animations()->RemoveBatch(existing.handles.AsSpan());
            for (std::uint32_t next = index + 1U; next < host->storyboardSessions.Size(); ++next) {
                host->storyboardSessions[next - 1U] = std::move(host->storyboardSessions[next]);
            }
            host->storyboardSessions.PopBack();
            break;
        }
    }
    StoryboardHost::StoryboardCompletionSession completion(host->Allocator());
    completion.storyboard = GetStoryboard();
    completion.owner = &associatedObject;
    Base::Result<std::uint32_t> started =
        host->BeginTimeline(
            *GetStoryboard(),
            associatedObject,
            host->activeActionNames,
            nullptr,
            &completion.handles,
            host->activeActionDataTemplateContext);
    if (!started) {
        host->Animations()->RemoveBatch(completion.handles.AsSpan());
        RecordAnimationActionFailure(host, started.GetStatus());
        return;
    }
    if (completion.handles.Empty()) {
        return;
    }
    StoryboardHost::StoryboardSession namedSession(host->Allocator());
    if (!GetName().Empty()) {
        namedSession.owner = &associatedObject;
        Base::Result<void> named = namedSession.name.Assign(GetName());
        if (!named) {
            host->Animations()->RemoveBatch(completion.handles.AsSpan());
            RecordAnimationActionFailure(host, named.GetStatus());
            return;
        }
        namedSession.handles.Append(completion.handles.AsSpan());
    }
    host->storyboardCompletionSessions.PushBack(std::move(completion));
    if (!GetName().Empty()) {
        host->storyboardSessions.PushBack(std::move(namedSession));
    }
}

void ControlStoryboardAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    StoryboardHost* host = ResolveAnimationStoryboardHost(associatedObject);
    if (host == nullptr) return;
    if (host->Animations() == nullptr) {
        RecordAnimationActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard action requires the animation manager"));
        return;
    }
    if (!GetStoryboard()) return;
    if (GetControlOption() == Option::Play) {
        BeginStoryboard begin;
        begin.SetStoryboard(GetStoryboard());
        begin.Invoke(associatedObject, parameter);
        return;
    }
    bool found = false;
    Base::Vector<Model::AnimationHandle> stopped(host->Allocator());
    for (StoryboardHost::StoryboardCompletionSession& session : host->storyboardCompletionSessions) {
        // Shared resource storyboards (DataBinding ShowPopup) are started
        // from one ListBoxItem and stopped from another. Match the
        // storyboard instance, not the element that began it.
        if (session.storyboard.Get() != GetStoryboard().Get()) continue;
        found = true;
        for (Model::AnimationHandle handle : session.handles) {
            if (GetControlOption() == Option::Stop) {
                if (host->Animations()->Stop(handle)) {
                    stopped.PushBack(handle);
                }
            } else if (GetControlOption() == Option::Pause) {
                host->Animations()->Pause(handle);
            } else if (GetControlOption() == Option::Resume) {
                host->Animations()->Resume(handle);
            } else {
                RecordAnimationActionFailure(
                    host,
                    Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "ControlStoryboardAction option is not implemented"));
                return;
            }
        }
    }
    if (GetControlOption() == Option::Stop) {
        if (!stopped.Empty()) {
            // WPF ClockController.Stop does not raise Completed. Drop the
            // session so StoryboardCompletedTrigger cannot steal focus.
            host->CancelStoryboardCompletionSessions(stopped.AsSpan());
        }
        return;
    }
    if (!found) {
        RecordAnimationActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ControlStoryboardAction storyboard was not started"));
    }
}

void PauseStoryboard::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    InvokeControllableStoryboardOperation(
        *this, ControllableStoryboardOperation::Pause, 0U, associatedObject);
}

void ResumeStoryboard::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    InvokeControllableStoryboardOperation(
        *this, ControllableStoryboardOperation::Resume, 0U, associatedObject);
}

void StopStoryboard::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    InvokeControllableStoryboardOperation(
        *this, ControllableStoryboardOperation::Stop, 0U, associatedObject);
}

void RemoveStoryboard::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    InvokeControllableStoryboardOperation(
        *this, ControllableStoryboardOperation::Remove, 0U, associatedObject);
}

void SeekStoryboard::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    InvokeControllableStoryboardOperation(
        *this,
        ControllableStoryboardOperation::Seek,
        GetOffsetMicroseconds(),
        associatedObject);
}

void PlayMediaAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    InvokeMediaElementOperation(
        *this, GetTargetName(), MediaElementOperation::Play, associatedObject);
}

void PauseMediaAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    InvokeMediaElementOperation(
        *this, GetTargetName(), MediaElementOperation::Pause, associatedObject);
}

void StopMediaAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    InvokeMediaElementOperation(
        *this, GetTargetName(), MediaElementOperation::Stop, associatedObject);
}

} // namespace Aero::Media::Animation

namespace Aero {

Base::Result<void>
StoryboardHost::ExecuteAnimationAction(
    Interactivity::TriggerAction& action,
    FrameworkElement& owner,
    Controls::DataTemplateTriggerInstance*
        dataTemplateContext,
    const NameScope* names) noexcept
{
    Controls::DataTemplateTriggerInstance* const previousTemplateContext =
        activeActionDataTemplateContext;
    const NameScope* const previousNames = activeActionNames;
    const Base::Status previousStatus = activeActionStatus;

    activeActionDataTemplateContext = dataTemplateContext;
    activeActionNames = names;
    activeActionStatus = Base::Status::Ok();

    const Meta::PropertyValue unset;
    action.Invoke(owner, unset);

    const Base::Status status = activeActionStatus;
    activeActionDataTemplateContext = previousTemplateContext;
    activeActionNames = previousNames;
    activeActionStatus = previousStatus;
    return status.IsOk() ? Base::Result<void>{} : Base::Result<void>(status);
}

using namespace Media::Animation;


bool StoryboardHost::AnimationEventState::EvaluateComparison(
            const Interactivity::ComparisonCondition& condition) noexcept {
            const Base::Ref<Data::Binding> binding =
                condition.GetLeftOperand();
            if (!binding || runtime == nullptr ||
                runtime->Metadata() == nullptr) {
                return false;
            }
            Base::Result<Meta::PropertyValue> current =
                Base::Status::Failure(Base::ErrorCode::NotFound, "Value not found");
            if (runtime->Interactivity() != nullptr &&
                owner != nullptr &&
                TryCast<FrameworkElement>(owner) != nullptr) {
                current = runtime->Interactivity()->EvaluateAuthoredBinding(
                    *binding,
                    *static_cast<FrameworkElement*>(owner),
                    dataTemplateContext,
                    names,
                    nullptr);
            } else {
                if (binding->GetElementName().Empty()) {
                    return false;
                }
                Base::Object* source = nullptr;
                if (dataTemplateContext != nullptr) {
                    source = dataTemplateContext->FindName(binding->GetElementName());
                }
                if (source == nullptr && names != nullptr) {
                    source = names->Find(binding->GetElementName());
                }
                if (source == nullptr && runtime->view != nullptr) {
                    source = runtime->view->loadedDocument.names.Find(
                          binding->GetElementName());
                }
                if (source == nullptr) {
                    return false;
                }
                Base::Result<Meta::BindingPathPlan> plan =
                    Meta::BindingPathPlan::Compile(
                        *runtime->Metadata(),
                        source->RuntimeType(), binding->GetPath().GetPath());
                if (!plan) return false;
                current = plan.Value().Get(*runtime->Metadata(), *source);
            }
            if (!current) return false;
            return ComparePropertyValues(
                current.Value(),
                condition.GetRightOperand(),
                runtime != nullptr ? runtime->Metadata() : nullptr,
                static_cast<PropertyComparisonOperator>(
                    condition.GetComparisonOperator()));
        }

bool StoryboardHost::AnimationEventState::BehaviorsAllowExecution() noexcept {
            for (const Base::Ref<Base::Object>& behavior :
                 trigger->GetBehaviors()) {
                if (!behavior) continue;
                if (behavior->RuntimeType() !=
                    Interactivity::ConditionBehavior::StaticTypeId()) {
                    return false;
                }
                const Base::Ref<Interactivity::ConditionalExpression> expression =
                    static_cast<Interactivity::ConditionBehavior&>(*behavior).GetExpression();
                if (!expression) {
                    return false;
                }
                const bool conjunction = expression->GetChaining() ==
                    Interactivity::ConditionalExpression::ForwardChaining::And;
                bool expressionResult = conjunction;
                bool hasCondition = false;
                for (const Base::Ref<Interactivity::ComparisonCondition>& condition :
                     expression->GetConditions()) {
                    if (!condition) continue;
                    hasCondition = true;
                    const bool matches = EvaluateComparison(*condition);
                    expressionResult = matches;
                    if (conjunction && !expressionResult) return false;
                    if (!conjunction && expressionResult) break;
                }
                if (!hasCondition || !expressionResult) return false;
            }
            return true;
        }

void StoryboardHost::AnimationEventState::Invoke(
            Base::Object*,
            RoutedEventArgs&) noexcept {
            if (runtime == nullptr || trigger == nullptr ||
                owner == nullptr) {
                return;
            }
            if (!BehaviorsAllowExecution()) {
                return;
            }
            for (const Base::Ref<Interactivity::TriggerAction>& action :
                 trigger->GetActions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    runtime->ExecuteAnimationAction(
                        *action, *owner, dataTemplateContext, names);
                if (!executed) {
                    // One action failing (SetFocus before the item is
                    // enabled, unresolved TargetName) must not skip later
                    // actions in the same trigger. QuestLog MouseEnter runs
                    // SetFocusAction then SelectAction; aborting here left
                    // the list item unselected and the detail card unchanged.
                    continue;
                }
            }
        }

Base::Result<void> StoryboardHost::StartEventTrigger(
        EventTrigger& trigger,
        Base::Object& defaultSource,
        FrameworkElement& actionOwner,
        const NameScope* names,
        Aero::Controls::DataTemplateTriggerInstance* dataTemplateContext) noexcept {
        const Base::StringView routedEvent =
            trigger.GetRoutedEvent();
        Base::Object* eventSource = nullptr;
        if (trigger.GetSourceName().Empty()) {
            eventSource = &defaultSource;
        } else {
            if (dataTemplateContext != nullptr) {
                eventSource = dataTemplateContext->FindName(trigger.GetSourceName());
            }
            if (eventSource == nullptr && names != nullptr) {
                eventSource = names->Find(trigger.GetSourceName());
            }
            if (eventSource == nullptr && view != nullptr) {
                eventSource = view->loadedDocument.names.Find(trigger.GetSourceName());
            }
        }
        if (eventSource == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "EventTrigger SourceName was not found");
        }
        // Microsoft.Xaml.Behaviors EventTrigger defaults EventName to
        // Loaded. Several reference samples intentionally omit EventName
        // to request that startup behavior.
        Base::StringView eventName = routedEvent.Empty()
            ? Base::StringView("Loaded")
            : routedEvent;
        std::uint32_t dot = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < eventName.SizeBytes(); ++index) {
            if (eventName[index] == '.') dot = index;
        }
        Base::StringView eventOwnerName;
        if (dot != UINT32_MAX) {
            eventOwnerName = eventName.Substr(0U, dot);
            eventName = eventName.Substr(
                dot + 1U,
                eventName.SizeBytes() - dot - 1U);
        }
        if (eventName == Base::StringView("GotFocus")) {
            eventName = Base::StringView("GotKeyboardFocus");
        }

        const bool loadedEvent =
            eventName == Base::StringView("Loaded");
        const bool uiSource = Metadata()->Types().IsDerivedFrom(
            eventSource->RuntimeType(), UIElement::StaticTypeId());
        const bool contentSource = Metadata()->Types().IsDerivedFrom(
            eventSource->RuntimeType(), ContentElement::StaticTypeId());
        if (!uiSource && !contentSource) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "EventTrigger source does not support routed events");
        }
        const Meta::EventInfo* event = nullptr;
        if (!eventOwnerName.Empty()) {
            Base::StringView ownerName = eventOwnerName;
            for (std::uint32_t index = 0U;
                 index < ownerName.SizeBytes(); ++index) {
                if (ownerName[index] == ':') {
                    ownerName = ownerName.Substr(
                        index + 1U,
                        ownerName.SizeBytes() - index - 1U);
                }
            }
            for (const Meta::TypeInfo& type :
                 Metadata()->Types().Types()) {
                if (type.Name() != ownerName) continue;
                event = Metadata()->Types().FindEvent(
                    type.Id(), eventName, true);
                if (event != nullptr) break;
            }
        } else {
            event = Metadata()->Types().FindEvent(
                eventSource->RuntimeType(), eventName, true);
        }
        if (event == nullptr && loadedEvent) {
            for (const Base::Ref<Interactivity::TriggerAction>& action :
                 trigger.GetActions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    ExecuteAnimationAction(
                        *action, actionOwner, dataTemplateContext, names);
                if (!executed) return executed.GetStatus();
            }
            return {};
        }
        if (event == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "EventTrigger RoutedEvent was not found on its source");
        }
        const RoutedEventHandle eventHandle{event->Id()};
        AnimationEventState* eventContext = nullptr;
        Base::Result<void> created = AllocateObject(
            *Allocator(), Base::MemoryTag::Ui, eventContext);
        if (!created) return created.GetStatus();
        eventContext->runtime = this;
        eventContext->trigger = &trigger;
        eventContext->owner = &actionOwner;
        eventContext->names = names;
        eventContext->dataTemplateContext = dataTemplateContext;
        auto callback = [eventContext](
            Base::Object* sender,
            RoutedEventArgs& args) noexcept {
            eventContext->Invoke(sender, args);
        };
        RoutedEventHandler handler(callback);
        if (uiSource) {
            static_cast<UIElement*>(eventSource)->AddHandler(
                eventHandle, handler);
        } else {
            static_cast<ContentElement*>(eventSource)->AddHandler(
                eventHandle, handler);
        }
        AnimationEventSubscription subscription;
        subscription.source = eventSource;
        subscription.visualOwner = &actionOwner;
        subscription.event = eventHandle;
        subscription.handler = handler;
        subscription.context = eventContext;
        subscription.contentSource = contentSource;
        animationEventSubscriptions.PushBack(
                std::move(subscription));
        // Microsoft.Xaml.Behaviors EventTrigger fires Loaded immediately when
        // the associated object is already loaded (InitializeComponent / mount
        // often raises Loaded before Interaction.Triggers are attached).
        if (loadedEvent && uiSource &&
            static_cast<UIElement*>(eventSource)->GetIsLoaded()) {
            // Defer until after the next layout/image pass so ElementName
            // bindings (Menu3D parallax TranslateTransform.X) see measured
            // ActualWidth before BackgroundAnim captures its From value.
            pendingLoadedTriggers.PushBack(
                {&trigger, &actionOwner, names, dataTemplateContext});
        }
        return {};
    }


namespace {

bool EventTriggerOwnerInSubtree(
        Media::Visual* node,
        const Media::Visual& fragmentRoot) noexcept {
    while (node != nullptr) {
        if (node == &fragmentRoot) return true;
        node = TryCast<Media::Visual>(node->GetLogicalParent()) != nullptr ? TryCast<Media::Visual>(node->GetLogicalParent()) : node->GetVisualParent();
    }
    return false;
}

} // namespace

void StoryboardHost::ClearEventTriggersFor(
        Media::Visual& fragmentRoot) noexcept {
        std::uint32_t writeIndex = 0U;
        for (std::uint32_t readIndex = 0U;
             readIndex < animationEventSubscriptions.Size();
             ++readIndex) {
            AnimationEventSubscription& subscription =
                animationEventSubscriptions[readIndex];
            if (subscription.visualOwner == nullptr ||
                !EventTriggerOwnerInSubtree(
                    subscription.visualOwner, fragmentRoot)) {
                if (writeIndex != readIndex) {
                    animationEventSubscriptions[writeIndex] =
                        std::move(subscription);
                }
                ++writeIndex;
                continue;
            }
            if (subscription.source != nullptr) {
                if (subscription.contentSource) {
                    static_cast<void>(
                        static_cast<ContentElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                } else {
                    static_cast<void>(
                        static_cast<UIElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                }
            }
            FreeObject(
                *Allocator(), Base::MemoryTag::Ui,
                subscription.context);
        }
        static_cast<void>(animationEventSubscriptions.Resize(writeIndex));
    }

void StoryboardHost::ClearEventTriggers() noexcept {
        for (AnimationEventSubscription& subscription :
             animationEventSubscriptions) {
            if (subscription.source != nullptr) {
                if (subscription.contentSource) {
                    static_cast<void>(
                        static_cast<ContentElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                } else {
                    static_cast<void>(
                        static_cast<UIElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                }
            }
            FreeObject(
                *Allocator(),
                Base::MemoryTag::Ui,
                subscription.context);
        }
        animationEventSubscriptions.Clear();
        pendingLoadedTriggers.Clear();
        eventTriggerStatus = Base::Status::Ok();
    }

Base::Result<void> StoryboardHost::FlushPendingLoadedTriggers() noexcept {
        Base::Vector<PendingLoadedTrigger> snapshot(Allocator());
        for (const PendingLoadedTrigger& pending : pendingLoadedTriggers) {
            snapshot.PushBack(pending);
        }
        pendingLoadedTriggers.Clear();
        for (const PendingLoadedTrigger& pending : snapshot) {
            if (pending.trigger == nullptr || pending.owner == nullptr) {
                continue;
            }
            for (const Base::Ref<Interactivity::TriggerAction>& action :
                 pending.trigger->GetActions()) {
                if (!action) continue;
                Base::Result<void> executed = ExecuteAnimationAction(
                    *action, *pending.owner, pending.dataTemplateContext, pending.names);
                if (!executed) return executed.GetStatus();
            }
        }
        return {};
    }

} // namespace Aero

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
using Media::Animation::BeginStoryboard;
using Media::Animation::Storyboard;
namespace {

void SetBeginStoryboardContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Storyboard> retained =
        Base::Ref<Storyboard>::TryFromBorrowed(
            static_cast<Storyboard&>(*value));
    if (!retained) {
        return;
    }
    static_cast<BeginStoryboard&>(owner)
        .SetStoryboard(std::move(retained));
    return;
}

void ClearBeginStoryboardContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<BeginStoryboard&>(owner)
        .SetStoryboard({});
    return;
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero::Media::Animation {

AERO_DESCRIBE(ControllableStoryboardAction) {
    using namespace Aero::Meta;
    Register<ControllableStoryboardAction>(context, TypeFlags::Abstract)
            .Property<&ControllableStoryboardAction::GetBeginStoryboardName, &ControllableStoryboardAction::SetBeginStoryboardName>("BeginStoryboardName");
}

AERO_DESCRIBE(BeginStoryboard) {
    using namespace Aero::Meta;
    Register<BeginStoryboard>(context)
            .Property<&BeginStoryboard::GetName, &BeginStoryboard::SetName>("Name")
            .Property<&BeginStoryboard::GetStoryboard, &BeginStoryboard::SetStoryboard>("Storyboard", PropertyFlags::Structural)
            .ContentAccessor("Storyboard",
                             ContentKind::Single,
                             &::Aero::MetadataSupport::SetBeginStoryboardContent,
                             &::Aero::MetadataSupport::ClearBeginStoryboardContent)
            .Factory();
}

AERO_DESCRIBE(ControlStoryboardAction) {
    using namespace Aero::Meta;
    Register<ControlStoryboardAction>(context)
            .Property<&ControlStoryboardAction::GetStoryboard, &ControlStoryboardAction::SetStoryboard>("Storyboard")
            .Property<&ControlStoryboardAction::GetControlOption, &ControlStoryboardAction::SetControlOption>("ControlStoryboardOption")
            .Factory();
}

AERO_DESCRIBE(PauseStoryboard) {
    using namespace Aero::Meta;
    Register<PauseStoryboard>(context).Factory();
}

AERO_DESCRIBE(ResumeStoryboard) {
    using namespace Aero::Meta;
    Register<ResumeStoryboard>(context).Factory();
}

AERO_DESCRIBE(StopStoryboard) {
    using namespace Aero::Meta;
    Register<StopStoryboard>(context).Factory();
}

AERO_DESCRIBE(RemoveStoryboard) {
    using namespace Aero::Meta;
    Register<RemoveStoryboard>(context).Factory();
}

AERO_DESCRIBE(SeekStoryboard) {
    using namespace Aero::Meta;
    Register<SeekStoryboard>(context)
            .Property<&SeekStoryboard::GetOffset, &SeekStoryboard::SetOffset>("Offset")
            .Factory();
}

AERO_DESCRIBE(PlayMediaAction) {
    using namespace Aero::Meta;
    Register<PlayMediaAction>(context)
            .Property<&PlayMediaAction::GetTargetName, &PlayMediaAction::SetTargetName>("TargetName")
            .Factory();
}

AERO_DESCRIBE(PauseMediaAction) {
    using namespace Aero::Meta;
    Register<PauseMediaAction>(context)
            .Property<&PauseMediaAction::GetTargetName, &PauseMediaAction::SetTargetName>("TargetName")
            .Factory();
}

AERO_DESCRIBE(StopMediaAction) {
    using namespace Aero::Meta;
    Register<StopMediaAction>(context)
            .Property<&StopMediaAction::GetTargetName, &StopMediaAction::SetTargetName>("TargetName")
            .Factory();
}

} // namespace Aero::Media::Animation
