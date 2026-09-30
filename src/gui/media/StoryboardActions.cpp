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
#include "gui/triggers/TriggerValueCompare.hpp"
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
#include <cctype>
#include <cstdlib>

namespace Aero {

using namespace Media::Animation;

Base::Result<void>
StoryboardHost::ExecuteAnimationAction(
    Interactivity::TriggerAction& action,
    FrameworkElement& owner,
    Controls::DataTemplateTriggerInstance*
        dataTemplateContext,
    const NameScope* names) noexcept
{
    const Meta::TypeId type =
        action.RuntimeType();
    if (type ==
        Interactivity::ChangePropertyAction::StaticTypeId()) {
        auto& change =
            static_cast<Interactivity::ChangePropertyAction&>(
                action);
        Meta::PropertyValue targetValue;
        Base::Object* targetObject = nullptr;
        if (Base::Ref<Data::Binding> targetBinding =
                change.GetTargetObject()) {
            Base::Result<Meta::PropertyValue> evaluated =
                Interactivity()->EvaluateAuthoredBinding(
                    *targetBinding,
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            targetValue = std::move(evaluated).Value();
            if (targetValue.Kind() == Meta::ValueKind::Object &&
                !targetValue.IsNullObject() &&
                targetValue.AsObject()) {
                targetObject = targetValue.AsObject().Get();
            }
        } else {
            targetObject = change.GetTargetName().Empty()
                ? static_cast<Base::Object*>(&owner)
                : dataTemplateContext != nullptr
                    ? dataTemplateContext->FindName(
                          change.GetTargetName())
                    : names != nullptr
                        ? names->Find(change.GetTargetName())
                        : view->loadedDocument.names.Find(
                              change.GetTargetName());
        }
        if (targetObject == nullptr ||
            !Metadata()->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction TargetName did not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<DependencyObject&>(
                *targetObject);
        Base::Result<ResolvedAnimationProperty> resolved =
            ResolveAnimationProperty(
                target, change.GetPropertyName());
        if (!resolved) return resolved.GetStatus();

        DependencyObject& propertyTarget =
            *resolved.Value().target;
        const Meta::DependencyPropertyHandle propertyHandle =
            resolved.Value().property;
        const Meta::DependencyProperty* property =
            (*Metadata()).DependencyProperties()
                    .Find(propertyHandle);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction property metadata was not found");
        }

        Meta::PropertyValue value = change.GetValue();
        Base::Ref<Data::Binding> valueBinding =
            change.GetValueBinding();
        if (valueBinding) {
            Base::Result<Meta::PropertyValue> evaluated =
                Interactivity()->EvaluateAuthoredBinding(
                    *valueBinding,
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            value = std::move(evaluated).Value();
        }
        if (value.IsNullObject() &&
            propertyHandle ==
                Controls::Primitives::ToggleButton::
                    IsCheckedProperty.Handle() &&
            Metadata()->Types().IsDerivedFrom(
                propertyTarget.RuntimeType(),
                Controls::Primitives::ToggleButton::
                    StaticTypeId())) {
            static_cast<Controls::Primitives::ToggleButton&>(
                propertyTarget).SetIsChecked(Nullable<bool>{});
            return {};
        }
        Base::Result<Meta::PropertyValue> coerced =
            NormalizeValueForProperty(
                Metadata(),
                *property,
                std::move(value));
        if (!coerced) return coerced.GetStatus();
        propertyTarget.SetCurrentValue(
            propertyHandle,
            std::move(coerced).Value());
        return {};
    }

    if (type ==
        Interactivity::InvokeCommandAction::StaticTypeId()) {
        auto& invoke =
            static_cast<Interactivity::InvokeCommandAction&>(action);
        Base::Ref<Input::ICommand> command = invoke.GetCommand();
        if (!command && invoke.GetCommandBinding()) {
            Base::Result<Meta::PropertyValue> evaluated =
                Interactivity()->EvaluateAuthoredBinding(
                    *invoke.GetCommandBinding(),
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            if (evaluated.Value().Kind() != Meta::ValueKind::Object ||
                evaluated.Value().IsNullObject() ||
                !evaluated.Value().AsObject() ||
                !Metadata()->Types().IsDerivedFrom(
                    evaluated.Value().AsObject()->RuntimeType(),
                    Input::ICommand::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "InvokeCommandAction Binding did not return ICommand");
            }
            command = Base::Ref<Input::ICommand>::FromBorrowed(
                *static_cast<Input::ICommand*>(
                    evaluated.Value().AsObject().Get()));
        }
        if (!command) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "InvokeCommandAction Command is unavailable");
        }

        Meta::PropertyValue parameter = invoke.GetCommandParameter();
        if (invoke.GetCommandParameterBinding()) {
            Base::Result<Meta::PropertyValue> evaluated =
                Interactivity()->EvaluateAuthoredBinding(
                    *invoke.GetCommandParameterBinding(),
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            parameter = std::move(evaluated).Value();
        }
        if (parameter.IsUnset()) {
            parameter = Meta::PropertyValue::NullObject(
                Meta::TypeOf<Base::Object>());
        }
        UIElement* target = TryCast<UIElement>(&(owner));
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "InvokeCommandAction owner is not a UIElement");
        }
        Base::Result<bool> canExecute = Input() != nullptr
            ? Input()->CanExecute(*command, parameter, *target)
            : command->CanExecute(parameter, target);
        if (!canExecute) return canExecute.GetStatus();
        if (!canExecute.Value()) return {};
        if (Input() != nullptr) {
            Base::Result<bool> executed =
                Input()->Execute(*command, parameter, *target);
            return executed
                ? Base::Result<void>()
                : Base::Result<void>(executed.GetStatus());
        }
        command->Execute(parameter, target);
        return {};
    }

    if (type == Interactivity::SetFocusAction::StaticTypeId()) {
        auto& setFocus = static_cast<Interactivity::SetFocusAction&>(action);
        if (!setFocus.GetEngage() || Input() == nullptr) return {};
        Meta::PropertyValue targetValue;
        Base::Object* targetObject = nullptr;
        if (Base::Ref<Data::Binding> targetBinding =
                setFocus.GetTargetObject()) {
            Base::Result<Meta::PropertyValue> evaluated =
                Interactivity()->EvaluateAuthoredBinding(
                    *targetBinding,
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            targetValue = std::move(evaluated).Value();
            if (targetValue.Kind() == Meta::ValueKind::Object &&
                !targetValue.IsNullObject() &&
                targetValue.AsObject()) {
                targetObject = targetValue.AsObject().Get();
            }
        } else {
            if (setFocus.GetTargetName().Empty()) {
                targetObject = static_cast<Base::Object*>(&owner);
            } else if (dataTemplateContext != nullptr) {
                targetObject = dataTemplateContext->FindName(
                    setFocus.GetTargetName());
            }
            if (targetObject == nullptr) {
                targetObject = owner.FindName(setFocus.GetTargetName());
            }
            if (targetObject == nullptr) {
                if (Controls::TemplateEngine* templates = ElementTree::TemplatesOf(owner)) {
                    const Controls::Control* control = ::Aero::TryCast<Controls::Control>(&owner);
                    if (control == nullptr) {
                        control = ::Aero::TryCast<Controls::Control>(owner.GetTemplatedParent());
                    }
                    if (control != nullptr) {
                        const Controls::TemplateHandle handle = templates->AppliedHandle(*control);
                        if (handle.IsValid()) {
                            targetObject = templates->FindName(handle, setFocus.GetTargetName());
                        }
                    }
                }
            }
            if (targetObject == nullptr && names != nullptr) {
                targetObject = names->Find(setFocus.GetTargetName());
            }
            if (targetObject == nullptr) {
                targetObject = view->loadedDocument.names.Find(
                    setFocus.GetTargetName());
            }
        }
        UIElement* target =
            targetObject != nullptr && Metadata()->Types().IsDerivedFrom(
                targetObject->RuntimeType(), UIElement::StaticTypeId())
            ? static_cast<UIElement*>(targetObject)
            : nullptr;
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "SetFocusAction target is unavailable");
        }
        if (!target->GetIsLoaded()) {
            if (view->focus == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotInitialized,
                    "View focus host is unavailable");
            }
            return view->focus->QueueFocus(*target);
        }
        if (!target->GetIsEnabled()) return {};
        Base::Result<bool> focused = Input()->SetFocus(target);
        // Focus is best-effort. A failed SetFocus must not fail the
        // EventTrigger (QuestLog MouseEnter → SelectAction).
        static_cast<void>(focused);
        return {};
    }

    if (type == Interactivity::SelectAction::StaticTypeId()) {
        if (Metadata()->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::ListBoxItem::StaticTypeId())) {
            static_cast<Controls::ListBoxItem&>(owner)
                .SetIsSelected(true);
            return {};
        }
        if (Metadata()->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::TabItem::StaticTypeId())) {
            static_cast<Controls::TabItem&>(owner)
                .SetIsSelected(true);
            return {};
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "SelectAction owner is not a selectable item container");
    }

    if (type == Interactivity::SelectAllAction::StaticTypeId()) {
        if (Metadata()->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::TextBox::StaticTypeId())) {
            return static_cast<Controls::TextBox&>(owner)
                .SelectAll();
        }
        if (Metadata()->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::PasswordBox::StaticTypeId())) {
            return static_cast<Controls::PasswordBox&>(owner)
                .SelectAll();
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "SelectAllAction owner is not a text editor");
    }

    if (type == Interactivity::PlaySoundAction::StaticTypeId()) {
        auto& playSound =
            static_cast<Interactivity::PlaySoundAction&>(action);
        if (!playSound.GetIsEnabled() ||
            playSound.GetSource().Empty()) {
            return {};
        }
        const double volume = playSound.GetVolume();
        if (!std::isfinite(volume) ||
            volume < 0.0 || volume > 1.0) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "PlaySoundAction Volume must be between zero and one");
        }
        Base::Result<void> initialized = view->audio.Initialize();
        if (!initialized &&
            (initialized.GetStatus().code ==
                 Base::ErrorCode::Unsupported ||
             initialized.GetStatus().code ==
                 Base::ErrorCode::InvalidState)) {
            // Audio is optional for headless and provider-free hosts.
            return {};
        }
        if (!initialized) return initialized.GetStatus();
        view->audio.SetEffectsVolume(
            static_cast<float>(volume));
        Base::Result<void> played =
            view->audio.PlayEffect(playSound.GetSource());
        if (!played &&
            (played.GetStatus().code ==
                 Base::ErrorCode::InvalidState ||
             played.GetStatus().code ==
                 Base::ErrorCode::NotFound)) {
            // A missing device or authored file must not poison the UI
            // trigger pipeline.
            return {};
        }
        return played;
    }

    if (type == Interactivity::RemoveElementAction::StaticTypeId()) {
        auto& remove = static_cast<Interactivity::RemoveElementAction&>(action);
        Base::Object* targetObject = static_cast<Base::Object*>(&owner);
        Base::Ref<Data::Binding> targetBinding =
            remove.GetTargetObject();
        if (targetBinding) {
            const Base::Ref<Data::RelativeSource> relative = targetBinding->GetRelativeSource();
            if (!relative || relative->GetMode() != Data::RelativeSourceMode::FindAncestor ||
                relative->GetAncestorType() != Base::StringView("ContextMenu") ||
                targetBinding->GetPath().GetPath() != Base::StringView("PlacementTarget")) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "RemoveElementAction TargetObject binding is not supported");
            }
            Media::Visual* current = &owner;
            Controls::ContextMenu* contextMenu = nullptr;
            while (current != nullptr) {
                if (Metadata()->Types().IsDerivedFrom(
                        current->RuntimeType(),
                        Controls::ContextMenu::StaticTypeId())) {
                    contextMenu = static_cast<Controls::ContextMenu*>(
                        current);
                    break;
                }
                current = TryCast<Media::Visual>(current->GetLogicalParent()) != nullptr ? TryCast<Media::Visual>(current->GetLogicalParent()) : current->GetVisualParent();
            }
            if (contextMenu == nullptr ||
                !contextMenu->GetPlacementTarget()) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "RemoveElementAction ContextMenu PlacementTarget was not found");
            }
            targetObject = contextMenu->GetPlacementTarget().Get();
        }
        if (targetObject == nullptr ||
            !Metadata()->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                UIElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RemoveElementAction target is not a UIElement");
        }
        auto& target = static_cast<UIElement&>(*targetObject);
        Media::Visual* current =
            TryCast<Media::Visual>(target.GetLogicalParent());
        if (current == nullptr) current = target.GetVisualParent();
        while (current != nullptr) {
            if (Metadata()->Types().IsDerivedFrom(
                    current->RuntimeType(),
                    Controls::ItemsControl::StaticTypeId())) {
                auto& items = static_cast<Controls::ItemsControl&>(*current);
                std::uint32_t index = UINT32_MAX;
                for (std::uint32_t candidate = 0U;
                     candidate < items.GetCount(); ++candidate) {
                    Base::Ref<Base::Object> item = items.GetItem(candidate);
                    if (item.Get() == &target) {
                        index = candidate;
                        break;
                    }
                }
                if (index != UINT32_MAX) {
                    Base::Result<Base::Ref<Base::Object>> removed =
                        items.GetItems().RemoveAt(index);
                    return removed
                        ? Base::Result<void>()
                        : Base::Result<void>(removed.GetStatus());
                }
            }
            current = TryCast<Media::Visual>(current->GetLogicalParent()) != nullptr ? TryCast<Media::Visual>(current->GetLogicalParent()) : current->GetVisualParent();
        }
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "RemoveElementAction target is not owned by an ItemsControl");
    }

    if (Animations() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Storyboard action requires the animation manager");
    }
    if (type ==
        BeginStoryboard::StaticTypeId()) {
        auto& begin =
            static_cast<BeginStoryboard&>(
                action);
        if (!begin.GetStoryboard()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "BeginStoryboard Storyboard was not resolved");
        }
        if (!begin.GetName().Empty()) {
            for (std::uint32_t index = 0U;
                 index < storyboardSessions.Size();
                 ++index) {
                StoryboardSession& existing =
                    storyboardSessions[index];
                if (existing.name.View() != begin.GetName()) {
                    continue;
                }
                CancelStoryboardCompletionSessions(
                    existing.handles.AsSpan());
                for (Model::AnimationHandle handle :
                     existing.handles) {
                    static_cast<void>(
                        Animations()->Remove(handle));
                }
                for (std::uint32_t next = index + 1U;
                     next < storyboardSessions.Size();
                     ++next) {
                    storyboardSessions[next - 1U] =
                        std::move(
                            storyboardSessions[next]);
                }
                storyboardSessions.PopBack();
                break;
            }
        }
        StoryboardCompletionSession completion(Allocator());
        completion.storyboard = begin.GetStoryboard();
        completion.owner = &owner;
        Base::Result<std::uint32_t> started =
            BeginTimeline(
                *begin.GetStoryboard(),
                owner, names, nullptr,
                &completion.handles,
                dataTemplateContext);
        if (!started) {
            for (Model::AnimationHandle handle :
                 completion.handles) {
                static_cast<void>(
                    Animations()->Remove(handle));
            }
            return started.GetStatus();
        }
        if (completion.handles.Empty()) {
            return {};
        }
        StoryboardSession namedSession(Allocator());
        if (!begin.GetName().Empty()) {
            namedSession.owner = &owner;
            Base::Result<void> named =
                namedSession.name.Assign(begin.GetName());
            if (!named) {
                for (Model::AnimationHandle handle :
                     completion.handles) {
                    static_cast<void>(
                        Animations()->Remove(handle));
                }
                return named.GetStatus();
            }
            namedSession.handles.Append(
                completion.handles.AsSpan());
        }
        storyboardCompletionSessions.PushBack(
                std::move(completion));
        if (!begin.GetName().Empty()) {
            storyboardSessions.PushBack(
                std::move(namedSession));
        }
        return {};
    }

    if (type == ControlStoryboardAction::StaticTypeId()) {
        auto& control = static_cast<ControlStoryboardAction&>(action);
        if (!control.GetStoryboard()) return {};
        if (control.GetControlOption() == ControlStoryboardAction::Option::Play) {
            BeginStoryboard begin;
            begin.SetStoryboard(control.GetStoryboard());
            return ExecuteAnimationAction(
                begin, owner, dataTemplateContext, names);
        }
        bool found = false;
        Base::Vector<Model::AnimationHandle> stopped(
            Allocator());
        for (StoryboardCompletionSession& session : storyboardCompletionSessions) {
            // Shared resource storyboards (DataBinding ShowPopup) are started
            // from one ListBoxItem and stopped from another. Match the
            // storyboard instance, not the element that began it.
            if (session.storyboard.Get() != control.GetStoryboard().Get()) continue;
            found = true;
            for (Model::AnimationHandle handle : session.handles) {
                Base::Result<void> result;
                if (control.GetControlOption() == ControlStoryboardAction::Option::Stop) {
                    result = Animations()->Stop(handle);
                    if (result) {
                        stopped.PushBack(handle);
                    }
                } else if (control.GetControlOption() == ControlStoryboardAction::Option::Pause) result = Animations()->Pause(handle);
                else if (control.GetControlOption() == ControlStoryboardAction::Option::Resume) result = Animations()->Resume(handle);
                else return Base::Status::Failure(Base::ErrorCode::Unsupported, "ControlStoryboardAction option is not implemented");
                if (!result) return result.GetStatus();
            }
        }
        if (control.GetControlOption() ==
                ControlStoryboardAction::Option::Stop) {
            if (!stopped.Empty()) {
                // WPF ClockController.Stop does not raise Completed. Drop the
                // session so StoryboardCompletedTrigger cannot steal focus.
                CancelStoryboardCompletionSessions(stopped.AsSpan());
            }
            return {};
        }
        return found ? Base::Result<void>{} : Base::Status::Failure(
            Base::ErrorCode::NotFound, "ControlStoryboardAction storyboard was not started");
    }

    if (type == PlayMediaAction::StaticTypeId() ||
        type == PauseMediaAction::StaticTypeId() ||
        type == StopMediaAction::StaticTypeId()) {
        Base::StringView targetName = type ==
                PlayMediaAction::StaticTypeId()
            ? static_cast<PlayMediaAction&>(action)
                  .GetTargetName()
            : type == PauseMediaAction::StaticTypeId()
                ? static_cast<PauseMediaAction&>(action)
                      .GetTargetName()
                : static_cast<StopMediaAction&>(action)
                      .GetTargetName();
        Base::Object* targetObject = targetName.Empty()
            ? static_cast<Base::Object*>(&owner)
            : names != nullptr
                ? names->Find(targetName)
                : view->loadedDocument.names.Find(targetName);
        if (targetObject == nullptr ||
            !Metadata()->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                Media::MediaElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "MediaAction TargetName did not resolve to a MediaElement");
        }
        auto& media = static_cast<Media::MediaElement&>(
            *targetObject);
        if (type == PlayMediaAction::StaticTypeId()) {
            media.Play();
        } else if (type ==
            PauseMediaAction::StaticTypeId()) {
            media.Pause();
        } else {
            media.Stop();
        }
        return {};
    }

    if (!Metadata()->Types().IsDerivedFrom(
            type,
            ControllableStoryboardAction::
                    StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "EventTrigger contains an unsupported action");
    }
    auto& control =
        static_cast<
            ControllableStoryboardAction&>(
                action);
    std::uint32_t sessionIndex = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < storyboardSessions.Size();
         ++index) {
        if (storyboardSessions[index].name.View() ==
                control.GetBeginStoryboardName()) {
            sessionIndex = index;
            break;
        }
    }
    if (sessionIndex == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Controllable Storyboard was not started");
    }
    StoryboardSession& session =
        storyboardSessions[sessionIndex];
    for (Model::AnimationHandle handle :
         session.handles) {
        Base::Result<void> result;
        if (type ==
            PauseStoryboard::
                StaticTypeId()) {
            result = Animations()->Pause(handle);
        } else if (type ==
            ResumeStoryboard::
                StaticTypeId()) {
            result = Animations()->Resume(handle);
        } else if (type ==
            StopStoryboard::
                StaticTypeId()) {
            result = Animations()->Stop(handle);
        } else if (type ==
            RemoveStoryboard::
                StaticTypeId()) {
            result = Animations()->Remove(handle);
        } else if (type ==
            SeekStoryboard::
                StaticTypeId()) {
            result = Animations()->Seek(
                handle,
                static_cast<
                    SeekStoryboard&>(
                        action).
                    GetOffsetMicroseconds());
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Controllable Storyboard action is unsupported");
        }
        if (!result) return result.GetStatus();
    }
    if (type ==
            StopStoryboard::StaticTypeId() ||
        type ==
            RemoveStoryboard::StaticTypeId()) {
        CancelStoryboardCompletionSessions(
            session.handles.AsSpan());
    }
    if (type ==
        RemoveStoryboard::StaticTypeId()) {
        for (std::uint32_t next =
                 sessionIndex + 1U;
             next < storyboardSessions.Size();
             ++next) {
            storyboardSessions[next - 1U] =
                std::move(
                    storyboardSessions[next]);
        }
        storyboardSessions.PopBack();
    }
    return {};
}

} // namespace Aero

namespace Aero {

using namespace Media::Animation;


Base::Result<bool> StoryboardHost::AnimationEventState::EvaluateComparison(
            const Interactivity::ComparisonCondition& condition) noexcept {
            const Base::Ref<Data::Binding> binding =
                condition.GetLeftOperand();
            if (!binding || runtime == nullptr ||
                runtime->Metadata() == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "ConditionBehavior requires a bound left operand");
            }
            Base::Result<Meta::PropertyValue> current =
                Base::Status::Failure(Base::ErrorCode::NotFound, "Value not found");
            if (runtime->Interactivity() != nullptr &&
                owner != nullptr &&
                TryCast<FrameworkElement>(owner) != nullptr) {
                current = runtime->Interactivity()->EvaluateAuthoredBinding(
                    *binding,
                    *static_cast<FrameworkElement*>(owner),
                    nullptr,
                    names,
                    nullptr);
            } else {
                if (binding->GetElementName().Empty()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "ConditionBehavior currently requires Binding ElementName");
                }
                Base::Object* source = names != nullptr
                    ? names->Find(binding->GetElementName())
                    : runtime->view->loadedDocument.names.Find(
                          binding->GetElementName());
                if (source == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "ConditionBehavior Binding ElementName was not found");
                }
                Base::Result<Meta::BindingPathPlan> plan =
                    Meta::BindingPathPlan::Compile(
                        *runtime->Metadata(),
                        source->RuntimeType(), binding->GetPath().GetPath());
                if (!plan) return plan.GetStatus();
                current = plan.Value().Get(*runtime->Metadata(), *source);
            }
            if (!current) return current.GetStatus();
            return ComparePropertyValues(
                current.Value(),
                condition.GetRightOperand(),
                runtime != nullptr ? runtime->Metadata() : nullptr,
                static_cast<PropertyComparisonOperator>(
                    condition.GetComparisonOperator()));
        }

Base::Result<bool> StoryboardHost::AnimationEventState::BehaviorsAllowExecution() noexcept {
            for (const Base::Ref<Base::Object>& behavior :
                 trigger->GetBehaviors()) {
                if (!behavior) continue;
                if (behavior->RuntimeType() !=
                    Interactivity::ConditionBehavior::StaticTypeId()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "EventTrigger contains an unsupported behavior");
                }
                const Base::Ref<Interactivity::ConditionalExpression> expression =
                    static_cast<Interactivity::ConditionBehavior&>(*behavior).GetExpression();
                if (!expression) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "ConditionBehavior has no expression");
                }
                const bool conjunction = expression->GetChaining() ==
                    Interactivity::ConditionalExpression::ForwardChaining::And;
                bool expressionResult = conjunction;
                bool hasCondition = false;
                for (const Base::Ref<Interactivity::ComparisonCondition>& condition :
                     expression->GetConditions()) {
                    if (!condition) continue;
                    hasCondition = true;
                    Base::Result<bool> matches = EvaluateComparison(*condition);
                    if (!matches) return matches.GetStatus();
                    expressionResult = matches.Value();
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
            Base::Result<bool> allowed = BehaviorsAllowExecution();
            if (!allowed) {
                return;
            }
            if (!allowed.Value()) return;
            for (const Base::Ref<Interactivity::TriggerAction>& action :
                 trigger->GetActions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    runtime->ExecuteAnimationAction(
                        *action, *owner, nullptr, names);
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

Base::Result<bool> StoryboardHost::StartEventTrigger(
        EventTrigger& trigger,
        Base::Object& defaultSource,
        FrameworkElement& actionOwner,
        const NameScope* names) noexcept {
        const Base::StringView routedEvent =
            trigger.GetRoutedEvent();
        Base::Object* eventSource =
            trigger.GetSourceName().Empty()
            ? &defaultSource
            : names != nullptr
                ? names->Find(trigger.GetSourceName())
                : view->loadedDocument.names.Find(
                      trigger.GetSourceName());
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
                        *action, actionOwner, nullptr, names);
                if (!executed) return executed.GetStatus();
            }
            return true;
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
                {&trigger, &actionOwner, names});
        }
        return true;
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
        for (std::uint32_t index = 0U;
             index < animationEventSubscriptions.Size();) {
            AnimationEventSubscription& subscription =
                animationEventSubscriptions[index];
            if (subscription.visualOwner == nullptr ||
                !EventTriggerOwnerInSubtree(
                    subscription.visualOwner, fragmentRoot)) {
                ++index;
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
            for (std::uint32_t next = index + 1U;
                 next < animationEventSubscriptions.Size(); ++next) {
                animationEventSubscriptions[next - 1U] =
                    std::move(animationEventSubscriptions[next]);
            }
            animationEventSubscriptions.PopBack();
        }
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
                    *action, *pending.owner, nullptr, pending.names);
                if (!executed) return executed.GetStatus();
            }
        }
        return {};
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

AERO_DESCRIBE(::Aero::Media::Animation::ControllableStoryboardAction) {
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
    Register<ControllableStoryboardAction>(context, TypeFlags::Abstract)
            .Property("BeginStoryboardName", &ControllableStoryboardAction::GetBeginStoryboardName, &ControllableStoryboardAction::SetBeginStoryboardName);
}

AERO_DESCRIBE(::Aero::Media::Animation::BeginStoryboard) {
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
    Register<BeginStoryboard>(context)
            .Property("Name", &BeginStoryboard::GetName, &BeginStoryboard::SetName)
            .Content<Storyboard>("Storyboard", ContentKind::Single, &::Aero::MetadataSupport::SetBeginStoryboardContent, &::Aero::MetadataSupport::ClearBeginStoryboardContent)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::ControlStoryboardAction) {
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
    Register<ControlStoryboardAction>(context)
            .Property("Storyboard", &ControlStoryboardAction::GetStoryboard, &ControlStoryboardAction::SetStoryboard)
            .Property("ControlStoryboardOption", &ControlStoryboardAction::GetControlOption, &ControlStoryboardAction::SetControlOption)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::PauseStoryboard) {
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
    Register<PauseStoryboard>(context).Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::ResumeStoryboard) {
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
    Register<ResumeStoryboard>(context).Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::StopStoryboard) {
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
    Register<StopStoryboard>(context).Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::RemoveStoryboard) {
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
    Register<RemoveStoryboard>(context).Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::SeekStoryboard) {
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
    Register<SeekStoryboard>(context)
            .Property("Offset", &SeekStoryboard::GetOffset, &SeekStoryboard::SetOffset)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::PlayMediaAction) {
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
    Register<PlayMediaAction>(context)
            .Property("TargetName", &PlayMediaAction::GetTargetName, &PlayMediaAction::SetTargetName)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::PauseMediaAction) {
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
    Register<PauseMediaAction>(context)
            .Property("TargetName", &PauseMediaAction::GetTargetName, &PauseMediaAction::SetTargetName)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::Animation::StopMediaAction) {
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
    Register<StopMediaAction>(context)
            .Property("TargetName", &StopMediaAction::GetTargetName, &StopMediaAction::SetTargetName)
            .Factory();
}
