#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include <Aero/Media/MediaElement.hpp>

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

Base::Result<void>
StoryboardHost::ExecuteAnimationAction(
    Aero::Interactivity::TriggerAction& action,
    Aero::FrameworkElement& owner,
    Aero::Controls::DataTemplateTriggerState*
        dataTemplateContext,
    const Aero::NameScope* names) noexcept
{
    const Meta::TypeId type =
        action.RuntimeType();
    if (type ==
        Aero::Interactivity::ChangePropertyAction::StaticTypeId()) {
        auto& change =
            static_cast<Aero::Interactivity::ChangePropertyAction&>(
                action);
        Base::Object* targetObject =
            change.GetTargetName().Empty()
            ? static_cast<Base::Object*>(&owner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(
                      change.GetTargetName())
                : names != nullptr
                    ? names->Find(change.GetTargetName())
                    : view->loadedDocument.names.Find(
                          change.GetTargetName());
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                ::Aero::DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction TargetName did not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<::Aero::DependencyObject&>(
                *targetObject);
        Base::Result<ResolvedAnimationProperty> resolved =
            ResolveAnimationProperty(
                target, change.GetPropertyName());
        if (!resolved) return resolved.GetStatus();

        ::Aero::DependencyObject& propertyTarget =
            *resolved.Value().target;
        const Meta::DependencyPropertyHandle propertyHandle =
            resolved.Value().property;
        const Meta::DependencyProperty* property =
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata)
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
                interactivity->EvaluateAuthoredBinding(
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
            metadata->Types().IsDerivedFrom(
                propertyTarget.RuntimeType(),
                Controls::Primitives::ToggleButton::
                    StaticTypeId())) {
            static_cast<Controls::Primitives::ToggleButton&>(
                propertyTarget).SetIsChecked(Nullable<bool>{});
            return {};
        }
        Base::Result<Meta::PropertyValue> coerced =
            Data::CoerceBindingTargetValue(
                metadata,
                *property,
                std::move(value));
        if (!coerced) return coerced.GetStatus();
        propertyTarget.SetCurrentValue(
            propertyHandle,
            std::move(coerced).Value());
        return {};
    }

    if (type ==
        Aero::Interactivity::InvokeCommandAction::StaticTypeId()) {
        auto& invoke =
            static_cast<Aero::Interactivity::InvokeCommandAction&>(action);
        Base::Ref<Input::ICommand> command = invoke.GetCommand();
        if (!command && invoke.GetCommandBinding()) {
            Base::Result<Meta::PropertyValue> evaluated =
                interactivity->EvaluateAuthoredBinding(
                    *invoke.GetCommandBinding(),
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            if (evaluated.Value().Kind() != Meta::ValueKind::Object ||
                evaluated.Value().IsNullObject() ||
                !evaluated.Value().AsObject() ||
                !metadata->Types().IsDerivedFrom(
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
                interactivity->EvaluateAuthoredBinding(
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
        Aero::UIElement* target = ::Aero::TryCast<::Aero::UIElement>(&(owner));
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "InvokeCommandAction owner is not a UIElement");
        }
        Base::Result<bool> canExecute = input != nullptr
            ? input->CanExecute(*command, parameter, *target)
            : command->CanExecute(parameter, target);
        if (!canExecute) return canExecute.GetStatus();
        if (!canExecute.Value()) return {};
        if (input != nullptr) {
            Base::Result<bool> executed =
                input->Execute(*command, parameter, *target);
            return executed
                ? Base::Result<void>()
                : Base::Result<void>(executed.GetStatus());
        }
        command->Execute(parameter, target);
        return {};
    }

    if (type == Aero::Interactivity::SetFocusAction::StaticTypeId()) {
        auto& setFocus = static_cast<Aero::Interactivity::SetFocusAction&>(action);
        if (!setFocus.GetEngage() || input == nullptr) return {};
        Base::Object* targetObject = setFocus.GetTargetName().Empty()
            ? static_cast<Base::Object*>(&owner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(setFocus.GetTargetName())
                : names != nullptr
                    ? names->Find(setFocus.GetTargetName())
                    : view->loadedDocument.names.Find(setFocus.GetTargetName());
        Aero::UIElement* target =
            targetObject != nullptr && metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(), Aero::UIElement::StaticTypeId())
            ? static_cast<Aero::UIElement*>(targetObject)
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
        Base::Result<bool> focused = input->SetFocus(target);
        return focused
            ? Base::Result<void>()
            : Base::Result<void>(focused.GetStatus());
    }

    if (type == Aero::Interactivity::SelectAction::StaticTypeId()) {
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::ListBoxItem::StaticTypeId())) {
            static_cast<Controls::ListBoxItem&>(owner)
                .SetIsSelected(true);
            return {};
        }
        if (metadata->Types().IsDerivedFrom(
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

    if (type == Aero::Interactivity::SelectAllAction::StaticTypeId()) {
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::TextBox::StaticTypeId())) {
            return static_cast<Controls::TextBox&>(owner)
                .SelectAll();
        }
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::PasswordBox::StaticTypeId())) {
            return static_cast<Controls::PasswordBox&>(owner)
                .SelectAll();
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "SelectAllAction owner is not a text editor");
    }

    if (type == Aero::Interactivity::PlaySoundAction::StaticTypeId()) {
        auto& playSound =
            static_cast<Aero::Interactivity::PlaySoundAction&>(action);
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

    if (type == Aero::Interactivity::RemoveElementAction::StaticTypeId()) {
        auto& remove = static_cast<Aero::Interactivity::RemoveElementAction&>(action);
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
            Aero::Media::Visual* current = &owner;
            Controls::ContextMenu* contextMenu = nullptr;
            while (current != nullptr) {
                if (metadata->Types().IsDerivedFrom(
                        current->RuntimeType(),
                        Controls::ContextMenu::StaticTypeId())) {
                    contextMenu = static_cast<Controls::ContextMenu*>(
                        current);
                    break;
                }
                current = ::Aero::TryCast<::Aero::Media::Visual>(current->GetLogicalParent()) != nullptr ? ::Aero::TryCast<::Aero::Media::Visual>(current->GetLogicalParent()) : current->GetVisualParent();
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
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                Aero::UIElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RemoveElementAction target is not a UIElement");
        }
        auto& target = static_cast<Aero::UIElement&>(*targetObject);
        Aero::Media::Visual* current =
            ::Aero::TryCast<::Aero::Media::Visual>(target.GetLogicalParent());
        if (current == nullptr) current = target.GetVisualParent();
        while (current != nullptr) {
            if (metadata->Types().IsDerivedFrom(
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
            current = ::Aero::TryCast<::Aero::Media::Visual>(current->GetLogicalParent()) != nullptr ? ::Aero::TryCast<::Aero::Media::Visual>(current->GetLogicalParent()) : current->GetVisualParent();
        }
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "RemoveElementAction target is not owned by an ItemsControl");
    }

    if (animations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Storyboard action requires the animation manager");
    }
    if (type ==
        MediaAnimation::BeginStoryboard::StaticTypeId()) {
        auto& begin =
            static_cast<MediaAnimation::BeginStoryboard&>(
                action);
        if (!begin.GetStoryboard()) return {};
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
                for (Aero::Media::Animation::Model::AnimationHandle handle :
                     existing.handles) {
                    static_cast<void>(
                        animations->Remove(handle));
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
        StoryboardCompletionSession completion(allocator);
        completion.storyboard = begin.GetStoryboard();
        completion.owner = &owner;
        Base::Result<std::uint32_t> started =
            BeginTimeline(
                *begin.GetStoryboard(),
                owner, names, nullptr,
                &completion.handles,
                dataTemplateContext);
        if (!started) {
            for (Aero::Media::Animation::Model::AnimationHandle handle :
                 completion.handles) {
                static_cast<void>(
                    animations->Remove(handle));
            }
            return started.GetStatus();
        }
        StoryboardSession namedSession(allocator);
        if (!begin.GetName().Empty()) {
            namedSession.owner = &owner;
            Base::Result<void> named =
                namedSession.name.Assign(begin.GetName());
            if (named) {
                named = namedSession.handles.Append(
                    completion.handles.AsSpan());
            }
            if (!named) {
                for (Aero::Media::Animation::Model::AnimationHandle handle :
                     completion.handles) {
                    static_cast<void>(
                        animations->Remove(handle));
                }
                return named.GetStatus();
            }
        }
        Base::Result<void> retained =
            storyboardCompletionSessions.PushBack(
                std::move(completion));
        if (!retained) {
            for (Aero::Media::Animation::Model::AnimationHandle handle :
                 completion.handles) {
                static_cast<void>(
                    animations->Remove(handle));
            }
            return retained.GetStatus();
        }
        if (!begin.GetName().Empty()) {
            retained = storyboardSessions.PushBack(
                std::move(namedSession));
            if (!retained) {
                for (Aero::Media::Animation::Model::AnimationHandle handle :
                     storyboardCompletionSessions.Back().
                         handles) {
                    static_cast<void>(
                        animations->Remove(handle));
                }
                storyboardCompletionSessions.PopBack();
                return retained.GetStatus();
            }
        }
        return {};
    }

    if (type == MediaAnimation::ControlStoryboardAction::StaticTypeId()) {
        auto& control = static_cast<MediaAnimation::ControlStoryboardAction&>(action);
        if (!control.GetStoryboard()) return {};
        if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Play) {
            MediaAnimation::BeginStoryboard begin;
            begin.SetStoryboard(control.GetStoryboard());
            return ExecuteAnimationAction(
                begin, owner, dataTemplateContext, names);
        }
        bool found = false;
        for (StoryboardCompletionSession& session : storyboardCompletionSessions) {
            if (session.owner != &owner || session.storyboard.Get() != control.GetStoryboard().Get()) continue;
            found = true;
            for (Aero::Media::Animation::Model::AnimationHandle handle : session.handles) {
                Base::Result<void> result;
                if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Stop) result = animations->Stop(handle);
                else if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Pause) result = animations->Pause(handle);
                else if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Resume) result = animations->Resume(handle);
                else return Base::Status::Failure(Base::ErrorCode::Unsupported, "ControlStoryboardAction option is not implemented");
                if (!result) return result.GetStatus();
            }
        }
        return found ? Base::Result<void>{} : Base::Status::Failure(
            Base::ErrorCode::NotFound, "ControlStoryboardAction storyboard was not started");
    }

    if (type == MediaAnimation::PlayMediaAction::StaticTypeId() ||
        type == MediaAnimation::PauseMediaAction::StaticTypeId() ||
        type == MediaAnimation::StopMediaAction::StaticTypeId()) {
        Base::StringView targetName = type ==
                MediaAnimation::PlayMediaAction::StaticTypeId()
            ? static_cast<MediaAnimation::PlayMediaAction&>(action)
                  .GetTargetName()
            : type == MediaAnimation::PauseMediaAction::StaticTypeId()
                ? static_cast<MediaAnimation::PauseMediaAction&>(action)
                      .GetTargetName()
                : static_cast<MediaAnimation::StopMediaAction&>(action)
                      .GetTargetName();
        Base::Object* targetObject = targetName.Empty()
            ? static_cast<Base::Object*>(&owner)
            : names != nullptr
                ? names->Find(targetName)
                : view->loadedDocument.names.Find(targetName);
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                Aero::Media::MediaElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "MediaAction TargetName did not resolve to a MediaElement");
        }
        auto& media = static_cast<Aero::Media::MediaElement&>(
            *targetObject);
        if (type == MediaAnimation::PlayMediaAction::StaticTypeId()) {
            media.Play();
        } else if (type ==
            MediaAnimation::PauseMediaAction::StaticTypeId()) {
            media.Pause();
        } else {
            media.Stop();
        }
        return {};
    }

    if (!metadata->Types().IsDerivedFrom(
            type,
            MediaAnimation::
                ControllableStoryboardAction::
                    StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "EventTrigger contains an unsupported action");
    }
    auto& control =
        static_cast<
            MediaAnimation::ControllableStoryboardAction&>(
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
    for (Aero::Media::Animation::Model::AnimationHandle handle :
         session.handles) {
        Base::Result<void> result;
        if (type ==
            MediaAnimation::PauseStoryboard::
                StaticTypeId()) {
            result = animations->Pause(handle);
        } else if (type ==
            MediaAnimation::ResumeStoryboard::
                StaticTypeId()) {
            result = animations->Resume(handle);
        } else if (type ==
            MediaAnimation::StopStoryboard::
                StaticTypeId()) {
            result = animations->Stop(handle);
        } else if (type ==
            MediaAnimation::RemoveStoryboard::
                StaticTypeId()) {
            result = animations->Remove(handle);
        } else if (type ==
            MediaAnimation::SeekStoryboard::
                StaticTypeId()) {
            result = animations->Seek(
                handle,
                static_cast<
                    MediaAnimation::SeekStoryboard&>(
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
            MediaAnimation::StopStoryboard::StaticTypeId() ||
        type ==
            MediaAnimation::RemoveStoryboard::StaticTypeId()) {
        CancelStoryboardCompletionSessions(
            session.handles.AsSpan());
    }
    if (type ==
        MediaAnimation::RemoveStoryboard::StaticTypeId()) {
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
