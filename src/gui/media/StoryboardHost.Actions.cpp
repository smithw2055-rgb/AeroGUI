#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
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

namespace Aero {

using namespace ::Aero;
using Media::Animation::BeginStoryboard;
using Media::Animation::ControlStoryboardAction;
using Media::Animation::ControllableStoryboardAction;
using Media::Animation::PauseMediaAction;
using Media::Animation::PauseStoryboard;
using Media::Animation::PlayMediaAction;
using Media::Animation::RemoveStoryboard;
using Media::Animation::ResumeStoryboard;
using Media::Animation::SeekStoryboard;
using Media::Animation::StopMediaAction;
using Media::Animation::StopStoryboard;

Base::Result<void>
StoryboardHost::ExecuteAnimationAction(
    Interactivity::TriggerAction& action,
    FrameworkElement& owner,
    Controls::DataTemplateTriggerState*
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
            Data::CoerceBindingTargetValue(
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
                for (Media::Animation::Model::AnimationHandle handle :
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
            for (Media::Animation::Model::AnimationHandle handle :
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
                for (Media::Animation::Model::AnimationHandle handle :
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
        Base::Vector<Media::Animation::Model::AnimationHandle> stopped(
            Allocator());
        for (StoryboardCompletionSession& session : storyboardCompletionSessions) {
            // Shared resource storyboards (DataBinding ShowPopup) are started
            // from one ListBoxItem and stopped from another. Match the
            // storyboard instance, not the element that began it.
            if (session.storyboard.Get() != control.GetStoryboard().Get()) continue;
            found = true;
            for (Media::Animation::Model::AnimationHandle handle : session.handles) {
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
                ControlStoryboardAction::Option::Stop &&
            !stopped.Empty()) {
            // WPF ClockController.Stop does not raise Completed. Drop the
            // session so StoryboardCompletedTrigger cannot steal focus.
            CancelStoryboardCompletionSessions(stopped.AsSpan());
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
    for (Media::Animation::Model::AnimationHandle handle :
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
